// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "tr_mme.h"
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <random>



bool shotBufPermInitialized = false;
byte* shotBufPerm;
std::thread* saveShotThread;
std::mutex saveShotThreadMutex;

extern int rollingShutterSuperSampleMultiplier;


static char *workAlloc = 0;
static char *workAlign = 0;
static int workSize, workUsed;
static qboolean allocFailed = qfalse;

static struct {
	mmeBlurControl_t control;
	mmeBlurBlock_t shot, depth, stencil;
	float	jitter[BLURMAX][2];
} blurData;



shotData_t shotData;


typedef struct {
	float position[2];
	float probability;
} potentialDofJitterPosition_t;

typedef struct {
	potentialDofJitterPosition_t* superRandomPotentialJitterPositions; // If mme_dofQuickRandom == 3. Probabilistic jitter position determination on each frame.
	int		superRandomPotentialJitterPositionsCount;
	float	pixelSize; // For randomizing on the fly
	float	sign;
	float   currentPosition[2]; // Calculated on each frame if superrandom is used.
	double  probabilityTotal;
} superRandomDofJitterControl_t;

//Data to contain the blurring factors
static struct {
	mmeBlurControl_t control;
	mmeBlurBlock_t dof;
	float	jitter[BLURMAX][2];

	// QuickDOF
	int		quickJitterTotalCount;
	int		quickJitterIndex;
	float*	quickJitter;

	superRandomDofJitterControl_t superRandomDofJitterControl;
} passData;


static struct {
	int pixelCount;
} mainData;

// MME cvars
cvar_t	*mme_aviFormat;

cvar_t	*mme_screenShotFormat;
cvar_t	*mme_screenShotGamma;
cvar_t	*mme_screenShotAlpha;
cvar_t	*mme_jpegQuality;
cvar_t	*mme_jpegDownsampleChroma;
cvar_t	*mme_jpegOptimizeHuffman;
cvar_t	*mme_tgaCompression;
cvar_t	*mme_pngCompression;
cvar_t	*mme_skykey;
cvar_t	*mme_worldShader;
cvar_t	*mme_skyShader;
cvar_t	*mme_worldDeform;
cvar_t	*mme_worldBlend;
cvar_t* mme_worldNoCull;
cvar_t	*mme_skyColor;
cvar_t	*mme_skyTint;
cvar_t	*mme_fboImageTint;
cvar_t	* mme_cinNoClamp;
cvar_t	*mme_pip;
cvar_t	*mme_blurFrames;
cvar_t	*mme_blurType;
cvar_t	*mme_blurOverlap;
cvar_t	*mme_blurGamma;
cvar_t	*mme_blurJitter;

cvar_t	*mme_dofFrames;
cvar_t	*mme_dofRadius;
cvar_t	*mme_dofQuick;
cvar_t	* mme_dofQuickRandom;
cvar_t	* mme_dofQuickRandomMod;
cvar_t	* mme_dofMask;
cvar_t	* mme_dofMaskInvert;

cvar_t	*mme_cpuSSE2;
cvar_t	*mme_pbo;

cvar_t	*mme_renderWidth;
cvar_t	*mme_renderHeight;
cvar_t	*mme_workMegs;
cvar_t	*mme_depthFocus;
cvar_t	*mme_depthRange;
cvar_t	*mme_saveOverwrite;
cvar_t	*mme_saveShot;
cvar_t	*mme_saveAEKeyframes;
cvar_t	*mme_saveStencil;
cvar_t	*mme_saveDepth;
cvar_t	*mme_saveADM;
cvar_t  *mme_rollingShutterEnabled;
cvar_t  *mme_rollingShutterBlur;
cvar_t  *mme_rollingShutterPixels;
cvar_t  *mme_rollingShutterMultiplier;
cvar_t  *mme_mvShaderLoadOrder;


#ifdef JEDIACADEMY_GLOW
extern std::vector<GLuint> pboIds;
extern std::vector<int> pboRollingShutterProgresses;
extern std::vector<float> pboRollingShutterDrifts;
extern int rollingShutterBufferCount;
extern int progressOvershoot;
extern float drift;
#endif

static void R_MME_MakeBlurBlock( mmeBlurBlock_t *block, int size, mmeBlurControl_t* control ) {
	memset( block, 0, sizeof( *block ) );
	size = (size + 15) & ~15;
	block->count = size / sizeof ( __m64 );
	block->control = control;

	if ( control->totalFrames ) {
		//Allow for floating point buffer with sse
		block->accum = (__m64 *)(workAlign + workUsed);
		workUsed += size * 4;
		if ( workUsed > workSize ) {
			ri.Error( ERR_FATAL, "Failed to allocate %d bytes from the mme_workMegs buffer\n", workUsed );
		}
	} 
	if ( control->overlapFrames ) {
		block->overlap = (__m64 *)(workAlign + workUsed);
		workUsed += control->overlapFrames * size;
		if ( workUsed > workSize ) {
			ri.Error( ERR_FATAL, "Failed to allocate %d bytes from the mme_workMegs buffer\n", workUsed );
		}
	}
}

// Used to start recording from another frame than the first but still keep perfect sync in terms of rolling shutter. 
// Since I'm too dumb to abstract this down into a simple variable setting, we just repeat the code used during frame capturing
// as many times as needed. Lame, but should work.
void R_MME_FakeAdvanceFrames(int count) {

	mmeRollingShutterInfo_t* rsInfo = R_MME_GetRollingShutterInfo();

	if (rsInfo->rollingShutterEnabled) {

		for (int c = 0; c < count; c++) {

			//int rollingShutterFactor = glConfig.vidHeight*rollingShutterSuperSampleMultiplier / mme_rollingShutterPixels->integer;

			for (int i = 0; i < rollingShutterBufferCount; i++) {

				int& rollingShutterProgress = pboRollingShutterProgresses[i];
				float& hereDrift = pboRollingShutterDrifts[i];

				rollingShutterProgress++;
				if (rollingShutterProgress == rsInfo->rollingShutterFactor) {
					//R_FrameBuffer_RollingShutterFlipDoubleBuffer(i);
					//rollingShutterProgress = 0;
					rollingShutterProgress = -progressOvershoot; // Since the rolling shutter multiplier can be a non-integer, sometimes we have to pause rendering frames for a little. Imagine if the rolling shutter is half the shutter speed. Then half the time we're not actually recording anything.
					hereDrift += drift;
					while (hereDrift > 1.0f) { // Drift has reached one frame (or rather one line) of duration. Adjust to keep sync with audio.
						rollingShutterProgress -= 1;
						hereDrift -= 1.0f;
					}
				}

			}
		}
	}
}

mmeRollingShutterInfo_t* R_MME_GetRollingShutterInfo() {

	static mmeRollingShutterInfo_t rsInfo;

	rsInfo.rollingShutterEnabled = (qboolean)(mme_rollingShutterEnabled->integer && !mme_blurFrames->integer && !mme_dofFrames->integer); // RS is not compatible with those, it does its own thing.
	rsInfo.rollingShutterPixels = mme_rollingShutterPixels->integer;
	rsInfo.rollingShutterMultiplier = mme_rollingShutterMultiplier->value;
	rsInfo.bufferCountNeededForRollingshutter = (int)(ceil(rsInfo.rollingShutterMultiplier) + 0.5f); // ceil bc if value is 1.1 we need 2 buffers. +.5 to avoid float issues..
	rsInfo.rollingShutterFactor = glConfig.vidHeight*rollingShutterSuperSampleMultiplier/ rsInfo.rollingShutterPixels;
	rsInfo.captureFpsMultiplier = (float)rsInfo.rollingShutterFactor / rsInfo.rollingShutterMultiplier;
	rsInfo.rollingShutterSuperSampleMultiplier = rollingShutterSuperSampleMultiplier;

	return &rsInfo;
}

// TODO Do a softer superrandom that randoms, but doesnt random every single pixel but rather just changes a settable percentage of the samples on each frame.
// Maybe make that a separate CVAR.
// Also: Find a way to seed in a way that the same image will always result in same results. Might be hard tho.

static inline void R_MME_GetSuperRandomJitterPosition(superRandomDofJitterControl_t* srJControl,float* x, float *y) {
	
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<double> dis(0.0, srJControl->probabilityTotal);
	
	double goal = dis(mt);

	double currentPosition = 0.0;
	int index = -1;
	while (++index < srJControl->superRandomPotentialJitterPositionsCount && currentPosition < goal) {
		currentPosition += srJControl->superRandomPotentialJitterPositions[index].probability;
	}
	index = max(index-1,0);
	float xRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
	float yRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
	*x = srJControl->sign * (srJControl->superRandomPotentialJitterPositions[index].position[0] + xRand * srJControl->pixelSize);
	*y = srJControl->sign * (srJControl->superRandomPotentialJitterPositions[index].position[1] + yRand * srJControl->pixelSize);

	/*if (srJControl->superRandomPotentialJitterPositions) {
		float highestProbability = -std::numeric_limits<float>::infinity();
		potentialDofJitterPosition_t* highestProbabilityPos = NULL;

		for (int i = 0; i < srJControl->superRandomPotentialJitterPositionsCount; i++) {
			float randomizedProbabilityHere = rand() * srJControl->superRandomPotentialJitterPositions[i].probability;
			if (randomizedProbabilityHere >= highestProbability) {
				highestProbability = randomizedProbabilityHere;
				highestProbabilityPos = &srJControl->superRandomPotentialJitterPositions[i];
			}
		}

		if (highestProbabilityPos) { // No reason why that should not be the case, but let's go safe.

			float xRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
			float yRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
			srJControl->currentPosition[0] = srJControl->sign * (highestProbabilityPos->position[0] + xRand * srJControl->pixelSize);
			srJControl->currentPosition[1] = srJControl->sign * (highestProbabilityPos->position[1] + yRand * srJControl->pixelSize);
		}
	}*/
}


static void R_MME_ClearSuperRandomJitter(superRandomDofJitterControl_t* srJControl) {
	if (srJControl->superRandomPotentialJitterPositions) {
		delete[] srJControl->superRandomPotentialJitterPositions;
		srJControl->superRandomPotentialJitterPositions = NULL;
	}
	srJControl->superRandomPotentialJitterPositionsCount = 0;
	Com_Memset(srJControl, 0, sizeof(superRandomDofJitterControl_t));
}

static qboolean R_MME_LoadDOFMask(float* jitterTable, int countNeeded, char* maskPath, superRandomDofJitterControl_t* superRandomDofJitterControl) {
	int dofMaskWidth, dofMaskHeight;
	textureImage_t picWrap;
	R_LoadImage(maskPath, &picWrap, &dofMaskWidth, &dofMaskHeight);
	if (picWrap.ptr != NULL) {

		R_MME_ClearSuperRandomJitter(superRandomDofJitterControl);
		superRandomDofJitterControl->superRandomPotentialJitterPositions = new potentialDofJitterPosition_t[dofMaskWidth * dofMaskHeight];
		superRandomDofJitterControl->superRandomPotentialJitterPositionsCount = dofMaskWidth * dofMaskHeight;
		Com_Memset(superRandomDofJitterControl->superRandomPotentialJitterPositions, 0, sizeof(potentialDofJitterPosition_t) * dofMaskHeight * dofMaskWidth);


		int pixelCount = dofMaskWidth * dofMaskHeight;
		float* dofMaskFloat = new float[(dofMaskWidth+1) * dofMaskHeight+1];// The two +1 additions are bc of the Floyd Steinberg Dithering, it needs some extra.

		// Convert image to monochrome float mask. We just take the red channel and ignore the others, who cares, the mask can't have colors anyway.
		double totalValue = 0.0;
		for (int i = 0; i < pixelCount; i++) {
			switch (picWrap.bpc) {
			case BPC_8BIT:
				dofMaskFloat[i] = ((byte*)picWrap.ptr)[i * 4];
				break;
			case BPC_16BIT:
				dofMaskFloat[i] = ((unsigned short*)picWrap.ptr)[i * 4];
				break;
			case BPC_32BIT:
				dofMaskFloat[i] = ((unsigned int*)picWrap.ptr)[i * 4];
				break;
			case BPC_32FLOAT:
				dofMaskFloat[i] = ((float*)picWrap.ptr)[i * 4];
				break;
			}
			totalValue += (double)dofMaskFloat[i];
		}


		

		// Make it so that the added up value of each pixel together equals the needed count of entries in jitter table.
		// Basically, this dofMaskFloat will be a map saying how many samples should be taken at each point of the image.
		double multiplier = countNeeded/totalValue; 
		for (int i = 0; i < pixelCount; i++) {
			dofMaskFloat[i] *= multiplier;
		}

		
		int addedSamples = 0;
		int longerSide = max(dofMaskWidth, dofMaskHeight);
		float pixelSideSize = 1.0f / (float)longerSide;
		float sign = mme_dofMaskInvert->integer ? -1.0f : 1.0f;
		superRandomDofJitterControl->pixelSize = pixelSideSize;
		superRandomDofJitterControl->sign = sign;
		double totalProbability = 0.0;
		for (int y = 0; y < dofMaskHeight; y++) {
			for (int x = 0; x < dofMaskWidth; x++) {
				float oldPixel = dofMaskFloat[y * dofMaskWidth + x];
				superRandomDofJitterControl->superRandomPotentialJitterPositions[y * dofMaskWidth + x].position[0] = (float)x / (float)longerSide - 0.5f;
				superRandomDofJitterControl->superRandomPotentialJitterPositions[y * dofMaskWidth + x].position[1] = (float)y / (float)longerSide - 0.5f;
				superRandomDofJitterControl->superRandomPotentialJitterPositions[y * dofMaskWidth + x].probability = oldPixel;
				totalProbability += oldPixel;
			}
		}
		superRandomDofJitterControl->probabilityTotal = totalProbability;
		
		/*// Now apply a nice little Floyd-Steinberg dithering because we will have nonsensical stuff like
		// pixels saying that 0.3 samples must be taken at them. The dithering will make it so that every pixel has an integer value
		// and the total added up value stays consistent.
		// Dithering algorithm based on pseudo code from https://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering
		for (int y = 0; y < dofMaskHeight; y++) {
			for (int x = 0; x < dofMaskWidth; x++) {
				float oldPixel = dofMaskFloat[y * dofMaskWidth + x];
				float newPixel = roundf(oldPixel);
				dofMaskFloat[y * dofMaskWidth + x] = newPixel;

				// Add samples
				int samplesHere = newPixel+0.5f;
				while (samplesHere-- > 0 && addedSamples < countNeeded) {
					float xRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
					float yRand = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) - 0.5f;
					jitterTable[addedSamples * 2] = sign*((float)x / (float)longerSide -0.5f+ xRand* pixelSideSize);
					jitterTable[addedSamples * 2+1] = sign*((float)y / (float)longerSide -0.5f + yRand * pixelSideSize);
					addedSamples++;
				}

				// Distribute error
				float quantError = oldPixel - newPixel;
				dofMaskFloat[y * dofMaskWidth + x+1] += quantError * 7.0f / 16.0f;
				dofMaskFloat[(y+1) * dofMaskWidth + x -1] += quantError * 3.0f / 16.0f;
				dofMaskFloat[(y+1) * dofMaskWidth + x] += quantError * 5.0f / 16.0f;
				dofMaskFloat[(y+1) * dofMaskWidth + x+1] += quantError * 1.0f / 16.0f;
			}
		}

		// May happen.
		while (addedSamples < countNeeded) {
			// Just duplicate some random one.
			int sourceSample = rand() % addedSamples;
			jitterTable[addedSamples * 2] = jitterTable[sourceSample * 2];
			jitterTable[addedSamples * 2+1] = jitterTable[sourceSample * 2+1];
			addedSamples++;
		}*/

		while (addedSamples < countNeeded) {
			R_MME_GetSuperRandomJitterPosition(superRandomDofJitterControl, &jitterTable[addedSamples * 2], &jitterTable[addedSamples * 2+1]);
			addedSamples++;
		}

		// Done.

		delete[] dofMaskFloat;
		ri.Free(picWrap.ptr);
		return qtrue;
	}
	else {
		return qfalse;
	}
}


static void R_MME_CheckCvars( void ) {
	int pixelCount, blurTotal, passTotal, quickDOF;
	mmeBlurControl_t* blurControl = &blurData.control;
	mmeBlurControl_t* passControl = &passData.control;

	pixelCount = glConfig.vidHeight * glConfig.vidWidth;

	if (mme_blurFrames->integer > BLURMAX) {
		ri.Cvar_Set( "mme_blurFrames", va( "%d", BLURMAX) );
	} else if (mme_blurFrames->integer < 0) {
		ri.Cvar_Set( "mme_blurFrames", "0" );
	}

	if (mme_blurOverlap->integer > BLURMAX ) {
		ri.Cvar_Set( "mme_blurOverlap", va( "%d", BLURMAX) );
	} else if (mme_blurOverlap->integer < 0 ) {
		ri.Cvar_Set( "mme_blurOverlap", "0");
	}
	
	if (mme_dofFrames->integer > BLURMAX ) {
		ri.Cvar_Set( "mme_dofFrames", va( "%d", BLURMAX) );
	} else if (mme_dofFrames->integer < 0 ) {
		ri.Cvar_Set( "mme_dofFrames", "0");
	}

	blurTotal = mme_blurFrames->integer + mme_blurOverlap->integer ;
	passTotal = mme_dofFrames->integer;

	if ( (mme_blurType->modified || passTotal != passControl->totalFrames ||  blurTotal != blurControl->totalFrames || pixelCount != mainData.pixelCount || blurControl->overlapFrames != mme_blurOverlap->integer) && !allocFailed ) {
		workUsed = 0;
		
		mainData.pixelCount = pixelCount;

		blurCreate( blurControl, mme_blurType->string, blurTotal );
		blurControl->totalFrames = blurTotal;
		blurControl->totalIndex = 0;
		blurControl->overlapFrames = mme_blurOverlap->integer; 
		blurControl->overlapIndex = 0;

		R_MME_MakeBlurBlock( &blurData.shot, pixelCount * 3, blurControl );
//		R_MME_MakeBlurBlock( &blurData.stencil, pixelCount * 1, blurControl );
		R_MME_MakeBlurBlock( &blurData.depth, pixelCount * 1, blurControl );

		R_MME_JitterTable( blurData.jitter[0], blurTotal );

		//Multi pass data
		blurCreate( passControl, "median", passTotal );
		passControl->totalFrames = passTotal;
		passControl->totalIndex = 0;
		passControl->overlapFrames = 0;
		passControl->overlapIndex = 0;
		R_MME_MakeBlurBlock( &passData.dof, pixelCount * 3, passControl );
		R_MME_JitterTable( passData.jitter[0], passTotal );
	}

	// Quick DOF
	// Jitters while the demo keeps moving. Obviously not incredibly accurate but especially with the high rolling shutter
	// capture FPS, it may not be bad enough to make a real dent.
	quickDOF = mme_dofQuick->integer;
	if (quickDOF) { 
		if (passTotal) { // mme_dofQuick is incompatible with mme_dofFrames
			passData.quickJitterTotalCount = 0;
			if (passData.quickJitter) {
				delete[] passData.quickJitter;
				passData.quickJitter = NULL;
			}
		}
		else {
			mmeRollingShutterInfo_t* rsInfo = R_MME_GetRollingShutterInfo();

			// Check how many frames it SHOULD be.
			
			// Unify this fps calculation code somewhere. UGLY.
			// Also TODO make this work with normal mme_blurFrames
			int blurFrames = 0;
			qboolean doit = qfalse;
			if (rsInfo->rollingShutterEnabled) {
				float captureFPS = shotData.fps * rsInfo->captureFpsMultiplier;
				float blurDuration = mme_rollingShutterBlur->value * (1.0f / shotData.fps);
				blurFrames = (int)(blurDuration * captureFPS);
				doit = qtrue;
			}
			else if (blurControl->totalFrames) {
				// Make this for mme_blurframes.
				blurFrames = blurControl->totalFrames;
				doit = qtrue;
			}

			if(doit){
				if (blurFrames != passData.quickJitterTotalCount || mme_dofMask->modified || mme_dofQuickRandom->modified || mme_dofMaskInvert->modified) {
					passData.quickJitterTotalCount = blurFrames;
					if (passData.quickJitter) {
						delete[] passData.quickJitter;
						passData.quickJitter = NULL;
					}
					passData.quickJitter = new float[blurFrames * 2];
					passData.quickJitterIndex = 0;

					bool dofMaskLoaded = false;
					superRandomDofJitterControl_t* srJControl = &passData.superRandomDofJitterControl;
					if (strlen(mme_dofMask->string)) {
						dofMaskLoaded = R_MME_LoadDOFMask(passData.quickJitter, blurFrames, mme_dofMask->string, srJControl);
					}
					if (!dofMaskLoaded) {
						R_MME_ClearSuperRandomJitter(srJControl);
						R_MME_JitterTable(passData.quickJitter, blurFrames);
					}
					if (mme_dofQuickRandom->integer) {

						std::random_device rd;
						std::mt19937 g(rd());
						std::shuffle((uint64_t*)&passData.quickJitter[0], (uint64_t*)&passData.quickJitter[blurFrames * 2], g);
					}
				}
			}
			
		}

	}

	mme_blurOverlap->modified = qfalse;
	mme_blurType->modified = qfalse;
	mme_blurFrames->modified = qfalse;
	mme_dofFrames->modified = qfalse;
	mme_dofQuick->modified = qfalse; // Not sure why I'm even doing this tbh. I'm an idiot.
	mme_dofQuickRandom->modified = qfalse; 
	mme_dofMask->modified = qfalse;
	mme_dofMaskInvert->modified = qfalse;
}

/* each loop LEFT shotData.take becomes true, but we don't want it when taking RIGHT (stereo) screenshot,
because we may want pause, and it will continue taking LEFT screenshot (and that's wrong) */
void R_MME_DoNotTake( ) {
	shotData.take = qfalse;
}

qboolean R_MME_JitterOrigin( float *x, float *y ) {
	mmeBlurControl_t* passControl = &passData.control;
	*x = 0;
	*y = 0;

	if (r_fboFishEye->integer) {
		return qfalse; // Handled elsewhere.
	}


	if (tr.captureIsActive && passData.quickJitter) {
		int i = passData.quickJitterIndex;
		float scale;
		float focus = shotData.dofFocus;
		float radius = shotData.dofRadius;
		R_MME_ClampDof(&focus, &radius);
		scale = radius * R_MME_FocusScale(focus);
		if (mme_dofQuickRandom->integer == 3 && passData.superRandomDofJitterControl.superRandomPotentialJitterPositions) {
			superRandomDofJitterControl_t* srJControl = &passData.superRandomDofJitterControl;

			*x = scale * srJControl->currentPosition[0];
			*y = -scale * srJControl->currentPosition[1];
		}
		else {

			*x = scale * passData.quickJitter[i * 2];
			*y = -scale * passData.quickJitter[i * 2 + 1];
		}
		return qtrue;
	}

	if ( !shotData.take || tr.finishStereo ) {
		shotData.take = qfalse;
		return qfalse;
	}
	if ( passControl->totalFrames ) {
		int i = passControl->totalIndex;
		float scale;
		float focus = shotData.dofFocus;
		float radius = shotData.dofRadius;
		R_MME_ClampDof(&focus, &radius);
		scale = radius * R_MME_FocusScale(focus);
		*x = scale * passData.jitter[i][0];
		*y = -scale * passData.jitter[i][1];
		return qtrue;
	}
	return qfalse;
}

void R_MME_JitterView( float *pixels, float *eyes ) {
	mmeBlurControl_t* blurControl = &blurData.control;
	mmeBlurControl_t* passControl = &passData.control;


	if (tr.captureIsActive &&  passData.quickJitter) {
		int i = passData.quickJitterIndex;
		float scale;
		float focus = shotData.dofFocus;
		float radius = shotData.dofRadius;
		R_MME_ClampDof(&focus, &radius);
		scale = r_znear->value / focus;
		scale *= radius * R_MME_FocusScale(focus);
		if (r_fboFishEye->integer) scale = 1.0f;
		if (mme_dofQuickRandom->integer == 3 && passData.superRandomDofJitterControl.superRandomPotentialJitterPositions) {
			superRandomDofJitterControl_t* srJControl = &passData.superRandomDofJitterControl;

			eyes[0] = scale * srJControl->currentPosition[0];
			eyes[1] = scale * srJControl->currentPosition[1];
		}
		else {
			eyes[0] = scale * passData.quickJitter[i * 2];
			eyes[1] = scale * passData.quickJitter[i * 2 + 1];
		}
	}

	if ( !shotData.take || tr.finishStereo ) {
		shotData.take = qfalse;
		return;
	}
	if ( blurControl->totalFrames ) {
		int i = blurControl->totalIndex;
		pixels[0] = mme_blurJitter->value * blurData.jitter[i][0];
		pixels[1] = mme_blurJitter->value * blurData.jitter[i][1];
	}
	if ( passControl->totalFrames ) {
		int i = passControl->totalIndex;
		float scale;
		float focus = shotData.dofFocus;
		float radius = shotData.dofRadius;
		R_MME_ClampDof(&focus, &radius);
		scale = r_znear->value / focus;
		scale *= radius * R_MME_FocusScale(focus);;
		if (r_fboFishEye->integer) scale = 1.0f;
		eyes[0] = scale * passData.jitter[i][0];
		eyes[1] = scale * passData.jitter[i][1];
	}

}

int R_MME_MultiPassNext( ) {
	mmeBlurControl_t* control = &passData.control;
	byte* outAlloc;
	__m64 *outAlign;
	int index;
	if ( !shotData.take || tr.finishStereo ) {
		shotData.take = qfalse;
		return 0;
	}
	
	// QuickJitter
	superRandomDofJitterControl_t* srJControl = &passData.superRandomDofJitterControl;
	if (mme_dofQuickRandom->integer==3 && srJControl->superRandomPotentialJitterPositions) {

		// This is when mme_dofQuickRandom == 3. Bokeh is basically a probability image. We basically multiply probability with rand() and pick the highest one on each frame.
		// Should essentially result in a constantly changing noise in the shape of the bokeh.

		R_MME_GetSuperRandomJitterPosition(srJControl, &srJControl->currentPosition[0], &srJControl->currentPosition[1]);
		
	}
	else if (++(passData.quickJitterIndex) >= passData.quickJitterTotalCount) { 
		// We don't really care about alignment with capture times or anything. It jitters through the wole jitterarray
		// in the correct amount of frames, that's good enough because I think the jitter table is randomized anyway.
		passData.quickJitterIndex = 0;



		// Change a few random samples to new ones.
		// TODO maybe make an angle speed dependent one. As angle of camera changes, more are changed, to simulate real life a bit.
		if (mme_dofQuickRandomMod->value > 0.0f && srJControl->superRandomPotentialJitterPositions) {
			static float leftOver = 0.0f; // Like a kind of dithering if the number to change would grow too small otherwise. If it would be 0.5 to change per frame then one will change every 2 frames.

			// mme_dofQuickRandomMod says how much percent of samples should be changed per second, so divide by fps.
			float countToChangeF = mme_dofQuickRandomMod->value / shotData.fps * (float)passData.quickJitterTotalCount + leftOver;
			int countToChange = roundf(countToChangeF) + 0.5f;
			leftOver = countToChangeF - (float)countToChange;

			std::random_device rd;
			std::mt19937 mt(rd());
			std::uniform_int_distribution<int> dis(0, passData.quickJitterTotalCount - 1);

			while (countToChange--) {

				int index = dis(mt);
				R_MME_GetSuperRandomJitterPosition(srJControl, &passData.quickJitter[index*2], &passData.quickJitter[index * 2+1]);
			}
			
		}

		if (mme_dofQuickRandom->integer > 1) {

			std::random_device rd;
			std::mt19937 g(rd());
			std::shuffle((uint64_t*)&passData.quickJitter[0], (uint64_t*)&passData.quickJitter[passData.quickJitterTotalCount * 2], g);
		}
	}


	if ( !control->totalFrames )
		return 0;

	index = control->totalIndex;
	outAlloc = (byte *)ri.Hunk_AllocateTempMemory( mainData.pixelCount * 3 + 16);
	outAlign = (__m64 *)((((int)(outAlloc)) + 15) & ~15);

	GLimp_EndFrame();
	R_MME_GetShot( outAlign );
	R_MME_BlurAccumAdd( &passData.dof, outAlign );
	
	tr.capturingDofOrStereo = qtrue;

	ri.Hunk_FreeTempMemory( outAlloc );
	if ( ++(control->totalIndex) < control->totalFrames ) {
		int nextIndex = control->totalIndex;
		if ( ++(nextIndex) >= control->totalFrames && r_stereoSeparation->value == 0.0f )
			tr.latestDofOrStereoFrame = qtrue;
		return 1;
	}
	control->totalIndex = 0;
	R_MME_BlurAccumShift( &passData.dof );
	return 0;
}

// Non rolling shutter version
static void R_MME_MultiShot( byte * target ) {
	if ( !passData.control.totalFrames ) {
		//Com_Printf("GetShot");
		R_MME_GetShot( target );
	}
	else {
		Com_Printf("MemCpy");
		Com_Memcpy( target, passData.dof.accum, mainData.pixelCount * 3 );
	}
}
static void R_MME_MultiShot( byte * target,int rollingShutterFactor,int rollingShutterProgress,int rollingShutterPixels,int rollingShutterBufferIndex) {
	if ( !passData.control.totalFrames ) {
		//Com_Printf("GetShot");
		R_MME_GetShot( target, rollingShutterFactor,rollingShutterProgress,rollingShutterPixels, rollingShutterBufferIndex);
	}
	else {
		Com_Printf("MemCpy");
		Com_Memcpy( target, passData.dof.accum, mainData.pixelCount * 3 );
	}
}


inline float pq(float in) {
	static const float m1 = 1305.0f / 8192.0f;
	static const float m2 = 2523.0f / 32.0f;
	static const float c1 = 107.0f / 128.0f;
	static const float c2 = 2413.0f / 128.0f;
	static const float c3 = 2392.0f / 128.0f;
	return std::pow((c1 + c2 * std::pow(in, m1)) / (1 + c3 * std::pow(in, m1)), m2);
}



void R_MME_FlushMultiThreading() {
	{
		std::lock_guard<std::mutex> guard(saveShotThreadMutex);
		if (saveShotThread) {
			saveShotThread->join();
			delete saveShotThread;
			saveShotThread = nullptr;
		}
	}
}


qboolean R_MME_TakeShot( void ) {
	int pixelCount;
	byte inSound[MME_SAMPLERATE] = {0};
	int sizeSound = 0;
	qboolean audio = qfalse, audioTaken = qfalse;
	qboolean doGamma;
	mmeBlurControl_t* blurControl = &blurData.control;

	//int mme_rollingShutterPixels = Cvar_Get("mme_rollingShutterPixels","1",);
	//int rollingShutterFactor = glConfig.vidHeight* rollingShutterSuperSampleMultiplier/mme_rollingShutterPixels->integer;
	mmeRollingShutterInfo_t* rsInfo = R_MME_GetRollingShutterInfo();

	//static int rollingShutterProgress = 0;

	if ( !shotData.take || allocFailed || tr.finishStereo )
		return qfalse;
	shotData.take = qfalse;

	pixelCount = glConfig.vidHeight * glConfig.vidWidth;

	doGamma = (qboolean)(( mme_screenShotGamma->integer || (tr.overbrightBits > 0) ) && (glConfig.deviceSupportsGamma ));
	R_MME_CheckCvars();

	if (!shotBufPermInitialized) {
#ifdef CAPTURE_FLOAT
		shotBufPerm = (byte*)ri.Hunk_AllocateTempMemory(pixelCount * 5 * 4);
#else
		shotBufPerm = (byte*)ri.Hunk_AllocateTempMemory(pixelCount * 5);
#endif
		shotBufPermInitialized = true;
	}


	//Special early version using the framebuffer
	if ( mme_saveShot->integer && blurControl->totalFrames > 0 &&
		R_FrameBuffer_Blur( blurControl->Float[ blurControl->totalIndex ], blurControl->totalIndex, blurControl->totalFrames ) ) {
		float fps;
		byte *shotBuf;
		if ( ++(blurControl->totalIndex) < blurControl->totalFrames ) 
			return qtrue;
		blurControl->totalIndex = 0;
		//shotBuf = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 3 );
		//R_MME_MultiShot( shotBufPerm );
		//if ( doGamma ) 
		//	R_GammaCorrect( shotBuf, pixelCount * 3 );

		R_MME_FlushMultiThreading();
		R_MME_MultiShot(shotBufPerm);

		bool dither = true;

		std::lock_guard<std::mutex> guard(saveShotThreadMutex);
		int totalFramesCount = blurControl->totalFrames;
		saveShotThread = new std::thread([&, dither, pixelCount, totalFramesCount] {
			qboolean audio = qfalse, audioTaken = qfalse;
			int sizeSound = 0;
			byte inSound[MME_SAMPLERATE] = { 0 };

#ifdef CAPTURE_FLOAT



			float* asFloatBuffer = (float*)shotBufPerm;
			if (dither) {

				// Floyd-Steinberg dither
				float oldPixel = 0.0f, newPixel = 0.0f, quantError = 0.0f;
				int stride = glConfig.vidWidth * 3;

				for (int i = 0; i < pixelCount * 3; i++) {

					oldPixel = asFloatBuffer[i]; // Important note: shader adds 0.5 for the rounded casting. keep in mind.
					newPixel = 0.5f + (float)(int)std::clamp(oldPixel, 0.5f, 255.5f);
					shotBufPerm[i] = newPixel;
					// Can we just remove the 0.5 stuff altogether if we add 0.5f to newpixel on generation?
					// oldPixel-0.5f-newPixel == oldPixel - (newPixel+0.5f)? == oldPixel - newPixel - 0.5f. yup, seems so.
					quantError = oldPixel - newPixel;
					asFloatBuffer[i + 3] += quantError * 7.0f / 16.0f; // This is the pixel to the right
					asFloatBuffer[i + stride - 3] += quantError * 3.0f / 16.0f; // This is the pixel to the left in lower row
					asFloatBuffer[i + stride] += quantError * 5.0f / 16.0f; // This is the pixel to below
					asFloatBuffer[i + stride + 3] += quantError * 1.0f / 16.0f; // This is the pixel to below, to the right

					// Normally we'd increase the buffer size because the bottom row of the dithering needs extra space
					// but the shotbuffer is already 5*pixelCount because it was meant to account for depth and whatnot?
				}
			}
			else {

				for (int i = 0; i < pixelCount; i++) {

					shotBufPerm[i * 3 + 0] = asFloatBuffer[i * 3 + 0];
					shotBufPerm[i * 3 + 1] = asFloatBuffer[i * 3 + 1];
					shotBufPerm[i * 3 + 2] = asFloatBuffer[i * 3 + 2];
				}
			}

#endif

			shotData.main.type = mmeShotTypeBGR;

			if (!audioTaken)
				audio = ri.S_MMEAviImport(inSound, &sizeSound);

			audioTaken = qtrue;

			//fps = shotData.fps / (blurControl->totalFrames);
			int outputFps = shotData.fps / (totalFramesCount);
			R_MME_SaveShot(&shotData.main, glConfig.vidWidth, glConfig.vidHeight, outputFps, shotBufPerm, audio, sizeSound, inSound);

			//delete shotDataThreadCopy;
		});



		//
		//audio = ri.S_MMEAviImport(inSound, &sizeSound);
		//R_MME_SaveShot( &shotData.main, glConfig.vidWidth, glConfig.vidHeight, fps, shotBuf, audio, sizeSound, inSound );
		//ri.Hunk_FreeTempMemory( shotBuf );
		return qtrue;
	}

	/* Test if we need to do blurred shots */
	if ( blurControl->totalFrames > 0 ) {
		mmeBlurBlock_t *blurShot = &blurData.shot;
		mmeBlurBlock_t *blurDepth = &blurData.depth;
//		mmeBlurBlock_t *blurStencil = &blurData.stencil;

		/* Test if we blur with overlapping frames */
		if ( blurControl->overlapFrames ) {
			/* First frame in a sequence, fill the buffer with the last frames */
			if (blurControl->totalIndex == 0) {
				int i;
				for ( i = 0; i < blurControl->overlapFrames; i++ ) {
					if ( mme_saveShot->integer ) {
						R_MME_BlurOverlapAdd( blurShot, i );
					}
					if ( mme_saveDepth->integer ) {
						R_MME_BlurOverlapAdd( blurDepth, i );
					}
//					if ( mme_saveStencil->integer ) {
//						R_MME_BlurOverlapAdd( blurStencil, i );
//					}
					blurControl->totalIndex++;
				}
			}
			if ( mme_saveShot->integer == 1 ) {
				byte* shotBuf = R_MME_BlurOverlapBuf( blurShot );
				R_MME_MultiShot( shotBuf ); 
				if ( doGamma && mme_blurGamma->integer ) {
					R_GammaCorrect( shotBuf, glConfig.vidWidth * glConfig.vidHeight * 3 );
				}
				R_MME_BlurOverlapAdd( blurShot, 0 );
			}
			if ( mme_saveDepth->integer == 1 ) {
				R_MME_GetDepth( R_MME_BlurOverlapBuf( blurDepth ) ); 
				R_MME_BlurOverlapAdd( blurDepth, 0 );
			}
//			if ( mme_saveStencil->integer == 1 ) {
//				R_MME_GetStencil( R_MME_BlurOverlapBuf( blurStencil ) ); 
//				R_MME_BlurOverlapAdd( blurStencil, 0 );
//			}
			blurControl->overlapIndex++;
			blurControl->totalIndex++;
		} else {
			byte *outAlloc;
			__m64 *outAlign;
			outAlloc = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 3 + 16);
			outAlign = (__m64 *)((((int)(outAlloc)) + 15) & ~15);

			if ( mme_saveShot->integer == 1 ) {
				R_MME_MultiShot( (byte*)outAlign );
				if ( doGamma && mme_blurGamma->integer ) {
					R_GammaCorrect( (byte *) outAlign, pixelCount * 3 );
				}
				R_MME_BlurAccumAdd( blurShot, outAlign );
			}

			if ( mme_saveDepth->integer == 1 ) {
				R_MME_GetDepth( (byte *)outAlign );
				R_MME_BlurAccumAdd( blurDepth, outAlign );
			}

//			if ( mme_saveStencil->integer == 1 ) {
//				R_MME_GetStencil( (byte *)outAlign );
//				R_MME_BlurAccumAdd( blurStencil, outAlign );
//			}
			ri.Hunk_FreeTempMemory( outAlloc );
			blurControl->totalIndex++;
		}

		if ( blurControl->totalIndex >= blurControl->totalFrames ) {
			float fps;
			blurControl->totalIndex = 0;

			fps = shotData.fps / ( blurControl->totalFrames );
		
			if ( mme_saveShot->integer == 1 ) {
				R_MME_BlurAccumShift( blurShot );
				if (doGamma && !mme_blurGamma->integer)
					R_GammaCorrect( (byte *)blurShot->accum, pixelCount * 3);
			}
			if ( mme_saveDepth->integer == 1 )
				R_MME_BlurAccumShift( blurDepth );
//			if ( mme_saveStencil->integer == 1 )
//				R_MME_BlurAccumShift( blurStencil );
		
			audio = ri.S_MMEAviImport(inSound, &sizeSound);
			audioTaken = qtrue;
			// Big test for an rgba shot
			if ( mme_saveShot->integer == 1 && shotData.main.type == mmeShotTypeRGBA ) {
				int i;
				byte *alphaShot = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 4);
				byte *rgbData = (byte *)(blurShot->accum );
				if ( mme_saveDepth->integer == 1 ) {
					byte *depthData = (byte *)( blurDepth->accum );
					for ( i = 0;i < pixelCount; i++ ) {
						alphaShot[i*4+0] = rgbData[i*3+0];
						alphaShot[i*4+1] = rgbData[i*3+1];
						alphaShot[i*4+2] = rgbData[i*3+2];
						alphaShot[i*4+3] = depthData[i];
					}
/*				} else if ( mme_saveStencil->integer == 1) {
					byte *stencilData = (byte *)( blurStencil->accum );
					for ( i = 0;i < pixelCount; i++ ) {
						alphaShot[i*4+0] = rgbData[i*3+0];
						alphaShot[i*4+1] = rgbData[i*3+1];
						alphaShot[i*4+2] = rgbData[i*3+2];
						alphaShot[i*4+3] = stencilData[i];
					}
*/				}
				R_MME_SaveShot( &shotData.main, glConfig.vidWidth, glConfig.vidHeight, fps, alphaShot, audio, sizeSound, inSound );
				ri.Hunk_FreeTempMemory( alphaShot );
			} else {
				if ( mme_saveShot->integer == 1 )
					R_MME_SaveShot( &shotData.main, glConfig.vidWidth, glConfig.vidHeight, fps, (byte *)( blurShot->accum ), audio, sizeSound, inSound );
				if ( mme_saveDepth->integer == 1 )
					R_MME_SaveShot( &shotData.depth, glConfig.vidWidth, glConfig.vidHeight, fps, (byte *)( blurDepth->accum ), audio, sizeSound, inSound );
//				if ( mme_saveStencil->integer == 1 )
//					R_MME_SaveShot( &shotData.stencil, glConfig.vidWidth, glConfig.vidHeight, fps, (byte *)( blurStencil->accum), audio, sizeSound, inSound );
			}
		}
	} 
	//Com_Printf("FrameInTakeShot");
	if ( mme_saveShot->integer > 1 || (!blurControl->totalFrames && mme_saveShot->integer )) {
		
		
		//byte *shotBuf = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 5 );
		
		//byte *shotBuf = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 5 );

		bool hdrConversionDone = false;

		if (!rsInfo->rollingShutterEnabled) {
			R_MME_FlushMultiThreading();
			R_MME_MultiShot(shotBufPerm);

			bool dither = true;

			std::lock_guard<std::mutex> guard(saveShotThreadMutex);
			saveShotThread = new std::thread([&, dither, pixelCount] {
				qboolean audio = qfalse, audioTaken = qfalse;
				int sizeSound = 0;
				byte inSound[MME_SAMPLERATE] = { 0 };

#ifdef CAPTURE_FLOAT



				float* asFloatBuffer = (float*)shotBufPerm;
				if (dither) {

					// Floyd-Steinberg dither
					float oldPixel = 0.0f, newPixel = 0.0f, quantError = 0.0f;
					int stride = glConfig.vidWidth * 3;

					for (int i = 0; i < pixelCount * 3; i++) {

						oldPixel = asFloatBuffer[i]; // Important note: shader adds 0.5 for the rounded casting. keep in mind.
						newPixel = 0.5f + (float)(int)std::clamp(oldPixel, 0.5f, 255.5f);
						shotBufPerm[i] = newPixel;
						// Can we just remove the 0.5 stuff altogether if we add 0.5f to newpixel on generation?
						// oldPixel-0.5f-newPixel == oldPixel - (newPixel+0.5f)? == oldPixel - newPixel - 0.5f. yup, seems so.
						quantError = oldPixel - newPixel;
						asFloatBuffer[i + 3] += quantError * 7.0f / 16.0f; // This is the pixel to the right
						asFloatBuffer[i + stride - 3] += quantError * 3.0f / 16.0f; // This is the pixel to the left in lower row
						asFloatBuffer[i + stride] += quantError * 5.0f / 16.0f; // This is the pixel to below
						asFloatBuffer[i + stride + 3] += quantError * 1.0f / 16.0f; // This is the pixel to below, to the right

						// Normally we'd increase the buffer size because the bottom row of the dithering needs extra space
						// but the shotbuffer is already 5*pixelCount because it was meant to account for depth and whatnot?
					}
				}
				else {

					for (int i = 0; i < pixelCount; i++) {

						shotBufPerm[i * 3 + 0] = asFloatBuffer[i * 3 + 0];
						shotBufPerm[i * 3 + 1] = asFloatBuffer[i * 3 + 1];
						shotBufPerm[i * 3 + 2] = asFloatBuffer[i * 3 + 2];
					}
				}

#endif

				shotData.main.type = mmeShotTypeBGR;

				if (!audioTaken)
					audio = ri.S_MMEAviImport(inSound, &sizeSound);

				audioTaken = qtrue;

				R_MME_SaveShot(&shotData.main, glConfig.vidWidth, glConfig.vidHeight, shotData.fps, shotBufPerm, audio, sizeSound, inSound);

				//delete shotDataThreadCopy;
			});
		}
		else {

			for (int i = 0; i < rollingShutterBufferCount; i++) {

				int& rollingShutterProgress = pboRollingShutterProgresses[i];
				float& hereDrift = pboRollingShutterDrifts[i];

				// For example 1.0 * 1080/9.8 = 110.20408163265306122448979591837
				//int rsBlurFrameCount = (int)(mme_rollingShutterBlur->value*(float)rsInfo->rollingShutterFactor/mme_rollingShutterMultiplier->value);
				int rsBlurFrameCount = (int)(mme_rollingShutterBlur->value * rsInfo->captureFpsMultiplier);
				float intensityMultiplier = 1.0f / (float)rsBlurFrameCount;

				//if(rollingShutterProgress >= 0){ // the later pbos have negative offsets because they start capturing later
				if (rollingShutterProgress >= -rsBlurFrameCount) { // the later pbos have negative offsets because they start capturing later. We also make use of this for blur as far as possible.


					int rollingShutterProgressReversed = rsInfo->rollingShutterFactor - rollingShutterProgress - 1;

					// 1. Check lines we can write into the current frame.
					//		In short, we can write from the current block up to rsBlurFrameCount blocks to the future,
					//		long as we don't shoot into the next picture.
					int howManyBlocks = min(rsBlurFrameCount, rsInfo->rollingShutterFactor - rollingShutterProgress);
					int negativeOffset = mme_rollingShutterPixels->integer * (howManyBlocks - 1); // Opengl is from bottom up, so we gotta move things around...
					R_FrameBuffer_RollingShutterCapture(i, mme_rollingShutterPixels->integer * rollingShutterProgressReversed - negativeOffset, mme_rollingShutterPixels->integer * howManyBlocks, true, false, intensityMultiplier);

					// 2. Check lines we can write into the next frame
					//		This applies basically only if our blur multiplier is bigger than  ceil(rollingshuttermultiplier)-rollingshuttermultiplier.
					int preProgressOvershootFrames = rsBlurFrameCount - progressOvershoot;
					if (preProgressOvershootFrames > 0) {
						// Possible that we write some.
						int framesLeftToWrite = rsInfo->rollingShutterFactor - rollingShutterProgress;
						if (framesLeftToWrite <= preProgressOvershootFrames) { // Otherwise too early.
							int blockOffset = preProgressOvershootFrames - framesLeftToWrite - rsBlurFrameCount;
							int blockOffsetReversed = rsInfo->rollingShutterFactor - blockOffset - 1;
							int howManyBlocks = min(rsBlurFrameCount, rsInfo->rollingShutterFactor - blockOffset);
							int negativeOffset = mme_rollingShutterPixels->integer * (howManyBlocks - 1); // Opengl is from bottom up, so we gotta move things around...
							R_FrameBuffer_RollingShutterCapture(i, mme_rollingShutterPixels->integer * blockOffsetReversed - negativeOffset, mme_rollingShutterPixels->integer * howManyBlocks, true, true, intensityMultiplier);
						}
					}


					//R_FrameBuffer_RollingShutterCapture(i, mme_rollingShutterPixels->integer* rollingShutterProgressReversed, mme_rollingShutterPixels->integer,true,false);


					if (rollingShutterProgress == rsInfo->rollingShutterFactor - 1) {

						R_MME_FlushMultiThreading();
						R_MME_MultiShot(shotBufPerm, rsInfo->rollingShutterFactor, rollingShutterProgress, mme_rollingShutterPixels->integer, i);

						bool dither = true;

						std::lock_guard<std::mutex> guard(saveShotThreadMutex);
						saveShotThread = new std::thread([&, dither, pixelCount] {
							qboolean audio = qfalse, audioTaken = qfalse;
							int sizeSound = 0;
							byte inSound[MME_SAMPLERATE] = { 0 };

#ifdef CAPTURE_FLOAT



							float* asFloatBuffer = (float*)shotBufPerm;
							if (dither) {

								// Floyd-Steinberg dither
								float oldPixel = 0.0f, newPixel = 0.0f, quantError = 0.0f;
								int stride = glConfig.vidWidth * 3;

								for (int i = 0; i < pixelCount * 3; i++) {

									oldPixel = asFloatBuffer[i]; // Important note: shader adds 0.5 for the rounded casting. keep in mind.
									newPixel = 0.5f + (float)(int)std::clamp(oldPixel, 0.5f, 255.5f);
									shotBufPerm[i] = newPixel;
									// Can we just remove the 0.5 stuff altogether if we add 0.5f to newpixel on generation?
									// oldPixel-0.5f-newPixel == oldPixel - (newPixel+0.5f)? == oldPixel - newPixel - 0.5f. yup, seems so.
									quantError = oldPixel - newPixel;
									asFloatBuffer[i + 3] += quantError * 7.0f / 16.0f; // This is the pixel to the right
									asFloatBuffer[i + stride - 3] += quantError * 3.0f / 16.0f; // This is the pixel to the left in lower row
									asFloatBuffer[i + stride] += quantError * 5.0f / 16.0f; // This is the pixel to below
									asFloatBuffer[i + stride + 3] += quantError * 1.0f / 16.0f; // This is the pixel to below, to the right

									// Normally we'd increase the buffer size because the bottom row of the dithering needs extra space
									// but the shotbuffer is already 5*pixelCount because it was meant to account for depth and whatnot?
								}
							}
							else {

								for (int i = 0; i < pixelCount; i++) {

									shotBufPerm[i * 3 + 0] = asFloatBuffer[i * 3 + 0];
									shotBufPerm[i * 3 + 1] = asFloatBuffer[i * 3 + 1];
									shotBufPerm[i * 3 + 2] = asFloatBuffer[i * 3 + 2];
								}
							}

#endif

							shotData.main.type = mmeShotTypeBGR;

							if (!audioTaken)
								audio = ri.S_MMEAviImport(inSound, &sizeSound);

							audioTaken = qtrue;

							R_MME_SaveShot(&shotData.main, glConfig.vidWidth, glConfig.vidHeight, shotData.fps, shotBufPerm, audio, sizeSound, inSound);

							//delete shotDataThreadCopy;
						});

					}
				}
				rollingShutterProgress++;
				if (rollingShutterProgress == rsInfo->rollingShutterFactor) {
					R_FrameBuffer_RollingShutterFlipDoubleBuffer(i);
					//rollingShutterProgress = 0;
					rollingShutterProgress = -progressOvershoot; // Since the rolling shutter multiplier can be a non-integer, sometimes we have to pause rendering frames for a little. Imagine if the rolling shutter is half the shutter speed. Then half the time we're not actually recording anything.
					hereDrift += drift;
					while (hereDrift > 1.0f) { // Drift has reached one frame (or rather one line) of duration. Adjust to keep sync with audio.
						rollingShutterProgress -= 1;
						hereDrift -= 1.0f;
					}
				}

			}
		}
		//ri.Hunk_FreeTempMemory( shotBuf );
	}

	if ( shotData.main.type == mmeShotTypeRGB ) {
/*		if ( mme_saveStencil->integer > 1 || ( !blurControl->totalFrames && mme_saveStencil->integer) ) {
			byte *stencilShot = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 1);
			R_MME_GetStencil( stencilShot );
			R_MME_SaveShot( &shotData.stencil, glConfig.vidWidth, glConfig.vidHeight, shotData.fps, stencilShot, audio, sizeSound, inSound );
			ri.Hunk_FreeTempMemory( stencilShot );
		}
*/		if ( mme_saveDepth->integer > 1 || ( !blurControl->totalFrames && mme_saveDepth->integer) ) {
			byte *depthShot = (byte *)ri.Hunk_AllocateTempMemory( pixelCount * 1);
			R_MME_GetDepth( depthShot );
			if (!audioTaken && ((mme_saveDepth->integer > 1 && mme_saveShot->integer > 1)
				|| (mme_saveDepth->integer == 1 && mme_saveShot->integer == 1)))
				audio = ri.S_MMEAviImport(inSound, &sizeSound);
			R_MME_SaveShot( &shotData.depth, glConfig.vidWidth, glConfig.vidHeight, shotData.fps, depthShot, audio, sizeSound, inSound );
			ri.Hunk_FreeTempMemory( depthShot );
		}
	}

	

	return qtrue;
}

const void *R_MME_CaptureShotCmd( const void *data ) {
	const captureCommand_t *cmd = (const captureCommand_t *)data;

	if (!cmd->name[0])
		return (const void *)(cmd + 1);

	shotData.take = qtrue;
	shotData.fps = cmd->fps;
	shotData.dofFocus = cmd->focus;
	shotData.dofRadius = cmd->radius;
	if (strcmp( cmd->name, shotData.main.name) || mme_screenShotFormat->modified || mme_screenShotAlpha->modified ) {
		/* Also reset the the other data */
		blurData.control.totalIndex = 0;
		if ( workAlign )
			Com_Memset( workAlign, 0, workUsed );
		Com_sprintf( shotData.main.name, sizeof( shotData.main.name ), "%s", cmd->name );
		Com_sprintf( shotData.depth.name, sizeof( shotData.depth.name ), "%s.depth", cmd->name );
		Com_sprintf( shotData.stencil.name, sizeof( shotData.stencil.name ), "%s.stencil", cmd->name );
		
		mme_screenShotFormat->modified = qfalse;
		mme_screenShotAlpha->modified = qfalse;

		if (!Q_stricmp(mme_screenShotFormat->string, "jpg")) {
			shotData.main.format = mmeShotFormatJPG;
		} else if (!Q_stricmp(mme_screenShotFormat->string, "tga")) {
			shotData.main.format = mmeShotFormatTGA;
		} else if (!Q_stricmp(mme_screenShotFormat->string, "png")) {
			shotData.main.format = mmeShotFormatPNG;
		} else if (!Q_stricmp(mme_screenShotFormat->string, "avi")) {
			shotData.main.format = mmeShotFormatAVI;
		} else {
			shotData.main.format = mmeShotFormatTGA;
		}
		
		//grayscale works fine only with compressed avi :(
		if (shotData.main.format != mmeShotFormatAVI || !mme_aviFormat->integer) {
			shotData.depth.format = mmeShotFormatPNG;
			shotData.stencil.format = mmeShotFormatPNG;
		} else {
			shotData.depth.format = mmeShotFormatAVI;
			shotData.stencil.format = mmeShotFormatAVI;
		}

		shotData.main.type = mmeShotTypeRGB;
		if ( mme_screenShotAlpha->integer ) {
			if ( shotData.main.format == mmeShotFormatPNG )
				shotData.main.type = mmeShotTypeRGBA;
			else if ( shotData.main.format == mmeShotFormatTGA )
				shotData.main.type = mmeShotTypeRGBA;
		}
		shotData.main.counter = -1;
		shotData.depth.type = mmeShotTypeGray;
		shotData.depth.counter = -1;
		shotData.stencil.type = mmeShotTypeGray;
		shotData.stencil.counter = -1;	
	}
	return (const void *)(cmd + 1);	
}

void R_MME_Capture( const char *shotName, float fps, float focus, float radius ) {
	captureCommand_t *cmd;
	
	if ( !tr.registered || !fps ) {
		return;
	}
	cmd = (captureCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	if (mme_dofFrames->integer > 0)
		tr.capturingDofOrStereo = qtrue;
	cmd->commandId = RC_CAPTURE;
	cmd->fps = fps;
	cmd->focus = focus;
	cmd->radius = radius;
	Q_strncpyz( cmd->name, shotName, sizeof( cmd->name ));
}

void R_MME_BlurInfo( int* total, int *index ) {
	*total = mme_blurFrames->integer;
	*index = blurData.control.totalIndex;
	if (*index )
		*index -= blurData.control.overlapFrames;
}

extern std::vector<AECamPosition> AECamPositions;
extern std::vector<std::vector<AEPlayerPosition>> AEPlayerPositions;
void R_MME_WriteAECamPath() {
	char fileName[MAX_OSPATH];
	int i;

	if (!AECamPositions.size()) {
		return;
	}

	/* First see if the file already exist */
	for (i = 0; i < AVI_MAX_FILES; i++) {
		Com_sprintf(fileName, sizeof(fileName), "%s.AECamPath.%03d.txt", shotData.main.name, i);
		if (!FS_FileExists(fileName))
			break;
	}

	fileHandle_t file = FS_FOpenFileWrite(fileName);

	std::string tmpString = "Adobe After Effects 8.0 Keyframe Data\r\n\r\n";
	tmpString += "\tUnits Per Second\t";
	tmpString += std::to_string(shotData.fps);
	tmpString += "\r\n\tSource Width\t";
	tmpString += std::to_string(glConfig.vidWidth);
	tmpString += "\r\n\tSource Height\t";
	tmpString += std::to_string(glConfig.vidHeight);
	tmpString += "\r\n\tSource Pixel Aspect Ratio\t";
	tmpString += std::to_string(1);
	tmpString += "\r\n\tComp Pixel Aspect Ratio\t";
	tmpString += std::to_string(1);
	tmpString += "\r\n";

	// Zoom first. (how to do this exactly?)
	tmpString += "\r\nCamera Options\tZoom\r\n\tFrame\tpixels\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		// Convert fov to zoom
		float zoom = glConfig.vidWidth / (2 * tan(AECamPositions[i].fov/2 * M_PI / 180));

		tmpString += std::to_string(zoom);
		tmpString += "\t\r\n";
	}
	/*
	// X Rotation
	tmpString += "\r\nTransform\tX Rotation\r\n\tFrame\tdegrees\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		float rot = AECamPositions[i].viewAngles[0];

		tmpString += std::to_string(rot);
		tmpString += "\t\r\n";
	}
	
	// Y Rotation
	tmpString += "\r\nTransform\tY Rotation\r\n\tFrame\tdegrees\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		float rot = AECamPositions[i].viewAngles[1];

		tmpString += std::to_string(rot);
		tmpString += "\t\r\n";
	}*/

	// Z Rotation
	tmpString += "\r\nTransform\tRotation\r\n\tFrame\tdegrees\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		float rot = AECamPositions[i].viewAngles[2];

		tmpString += std::to_string(rot);
		tmpString += "\t\r\n";
	}

	// Orientation
	/*tmpString += "\r\nTransform\tOrientation\r\n\tFrame\tX degrees\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";

		tmpString += std::to_string(AECamPositions[i].viewAngles[0]);
		tmpString += "\t";
		tmpString += std::to_string(AECamPositions[i].viewAngles[1]);
		tmpString += "\t";
		tmpString += std::to_string(AECamPositions[i].viewAngles[2]);
		tmpString += "\t\r\n";
	}*/

	// Position
	tmpString += "\r\nTransform\tPosition\r\n\tFrame\tX pixels\tY pixels\tZ pixels\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		tmpString += std::to_string(AECamPositions[i].viewOrg[0]);
		tmpString += "\t";
		tmpString += std::to_string(-AECamPositions[i].viewOrg[2]);// In AE x is right/left, y is height and z is depth. Also, needs inversion of height
		tmpString += "\t";
		tmpString += std::to_string(AECamPositions[i].viewOrg[1]);
		tmpString += "\t\r\n";
	}
	
	tmpString += "\r\nTransform\tPoint of Interest\r\n\tFrame\tX pixels\tY pixels\tZ pixels\t\r\n";
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += "\t";
		tmpString += std::to_string(i);
		tmpString += "\t";
		vec3_t dir; 
		vec3_t poi;
		VectorScale(AECamPositions[i].viewAxis[0],100,dir); // just so its easier to see in AE
		VectorAdd(AECamPositions[i].viewOrg, dir, poi);
		tmpString += std::to_string(poi[0]);
		tmpString += "\t";
		tmpString += std::to_string(-poi[2]); // In AE x is right/left, y is height and z is depth. Also, needs inversion of height
		tmpString += "\t";
		tmpString += std::to_string(poi[1]);
		tmpString += "\t\r\n";
	}

	tmpString += "\r\n\r\nEnd of Keyframe Data\r\n";


	FS_Write(tmpString.c_str(),tmpString.size(),file);

	FS_FCloseFile(file);

	//AECamPositions.clear();

	///
	// Now the player positions.
	///
	///
	/// 
	/* First see if the file already exist */
	for (i = 0; i < AVI_MAX_FILES; i++) {
		Com_sprintf(fileName, sizeof(fileName), "%s.AEPlayerPaths.%03d.txt", shotData.main.name, i);
		if (!FS_FileExists(fileName))
			break;
	}

	file = FS_FOpenFileWrite(fileName);

	tmpString = "//\r\n// Player Positions\r\n// Copy each player you want individually\r\n//\r\n\r\n";

	for(int a=0;a<AEPlayerPositions.size();a++){
		tmpString += "//\r\n// Player #";
		tmpString += std::to_string(a);
		tmpString += ":\r\n//\r\n\r\n";

		tmpString += "Adobe After Effects 8.0 Keyframe Data\r\n\r\n";
		tmpString += "\tUnits Per Second\t";
		tmpString += std::to_string(shotData.fps);
		tmpString += "\r\n\tSource Width\t";
		tmpString += std::to_string(glConfig.vidWidth);
		tmpString += "\r\n\tSource Height\t";
		tmpString += std::to_string(glConfig.vidHeight);
		tmpString += "\r\n\tSource Pixel Aspect Ratio\t";
		tmpString += std::to_string(1);
		tmpString += "\r\n\tComp Pixel Aspect Ratio\t";
		tmpString += std::to_string(1);
		tmpString += "\r\n";
		
		// Position
		tmpString += "\r\nTransform\tPosition\r\n\tFrame\tX pixels\tY pixels\tZ pixels\t\r\n";
		for (i = 0; i < AEPlayerPositions[a].size(); i++) {
			tmpString += "\t";
			tmpString += std::to_string(i);
			tmpString += "\t";
			tmpString += std::to_string(AEPlayerPositions[a][i].origin[0]);
			tmpString += "\t";
			tmpString += std::to_string(-AEPlayerPositions[a][i].origin[2]);// In AE x is right/left, y is height and z is depth. Also, needs inversion of height
			tmpString += "\t";
			tmpString += std::to_string(AEPlayerPositions[a][i].origin[1]);
			tmpString += "\t\r\n";
		}

		tmpString += "\r\n\r\nEnd of Keyframe Data\r\n";
	}

	FS_Write(tmpString.c_str(), tmpString.size(), file);
	

	FS_FCloseFile(file);

	//AEPlayerPositions.clear();
}

void R_MME_WriteRotationCSV() {
	char fileName[MAX_OSPATH];
	int i;

	if (!AECamPositions.size()) {
		return;
	}

	/* First see if the file already exist */
	for (i = 0; i < AVI_MAX_FILES; i++) {
		Com_sprintf(fileName, sizeof(fileName), "%s.CamOrientation.%03d.csv", shotData.main.name, i);
		if (!FS_FileExists(fileName))
			break;
	}

	fileHandle_t file = FS_FOpenFileWrite(fileName);

	std::string tmpString = "viewAxis[0][0],viewAxis[0][1],viewAxis[0][2],viewAxis[1][0],viewAxis[1][1],viewAxis[1][2],viewAxis[2][0],viewAxis[2][1],viewAxis[2][2],viewAngles[0],viewAngles[1],viewAngles[2]\n";
	
	for (int i = 0; i < AECamPositions.size(); i++) {
		tmpString += std::to_string(AECamPositions[i].viewAxis[0][0]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[0][1]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[0][2]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[1][0]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[1][1]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[1][2]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[2][0]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[2][1]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAxis[2][2]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAngles[0]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAngles[1]);
		tmpString += ",";
		tmpString += std::to_string(AECamPositions[i].viewAngles[2]);
		tmpString += "\n";
	}

	FS_Write(tmpString.c_str(),tmpString.size(),file);

	FS_FCloseFile(file);

}

extern void S_MMEWavClose(void);
void R_MME_Shutdown(void) {

	R_MME_FlushMultiThreading();
	R_MME_WriteAECamPath();
	R_MME_WriteRotationCSV();
	AECamPositions.clear();
	AEPlayerPositions.clear();
	aviClose( &shotData.main.avi );
	aviClose( &shotData.depth.avi );
	aviClose( &shotData.stencil.avi );
	S_MMEWavClose(); // If the game crashes at the end of a demo, at least save the damn audio stuff.
}

void R_MME_Init(void) {

	// MME cvars
	mme_aviFormat = ri.Cvar_Get ("mme_aviFormat", "0", CVAR_ARCHIVE);

	mme_jpegQuality = ri.Cvar_Get ("mme_jpegQuality", "90", CVAR_ARCHIVE);
	mme_jpegDownsampleChroma = ri.Cvar_Get ("mme_jpegDownsampleChroma", "0", CVAR_ARCHIVE);
	mme_jpegOptimizeHuffman = ri.Cvar_Get ("mme_jpegOptimizeHuffman", "1", CVAR_ARCHIVE);
	mme_screenShotFormat = ri.Cvar_Get ("mme_screenShotFormat", "png", CVAR_ARCHIVE);
	mme_screenShotGamma = ri.Cvar_Get ("mme_screenShotGamma", "0", CVAR_ARCHIVE);
	mme_screenShotAlpha = ri.Cvar_Get ("mme_screenShotAlpha", "0", CVAR_ARCHIVE);
	mme_tgaCompression = ri.Cvar_Get ("mme_tgaCompression", "1", CVAR_ARCHIVE);
	mme_pngCompression = ri.Cvar_Get("mme_pngCompression", "5", CVAR_ARCHIVE);
	mme_skykey = ri.Cvar_Get( "mme_skykey", "0", CVAR_ARCHIVE );
	mme_pip = ri.Cvar_Get( "mme_pip", "0", CVAR_CHEAT );	//-
	mme_worldShader = ri.Cvar_Get( "mme_worldShader", "0", CVAR_CHEAT );
	mme_skyShader = ri.Cvar_Get( "mme_skyShader", "0", CVAR_CHEAT );
	mme_worldDeform = ri.Cvar_Get( "mme_worldDeform", "0", CVAR_CHEAT );
	mme_worldBlend = ri.Cvar_Get( "mme_worldBlend", "0", CVAR_CHEAT );
	mme_worldNoCull = ri.Cvar_Get( "mme_worldNoCull", "0", CVAR_CHEAT );
	mme_skyColor = ri.Cvar_Get( "mme_skyColor", "0", CVAR_CHEAT );
	mme_skyTint = ri.Cvar_Get( "mme_skyTint", "0", CVAR_CHEAT );
	mme_fboImageTint = ri.Cvar_Get( "mme_fboImageTint", "0", CVAR_CHEAT );
	mme_cinNoClamp = ri.Cvar_Get( "mme_cinNoClamp", "0", CVAR_ARCHIVE);
	mme_renderWidth = ri.Cvar_Get( "mme_renderWidth", "0", CVAR_LATCH | CVAR_ARCHIVE );
	mme_renderHeight = ri.Cvar_Get( "mme_renderHeight", "0", CVAR_LATCH | CVAR_ARCHIVE );

	mme_blurFrames = ri.Cvar_Get ( "mme_blurFrames", "0", CVAR_ARCHIVE );
	mme_blurOverlap = ri.Cvar_Get ("mme_blurOverlap", "0", CVAR_ARCHIVE );
	mme_blurType = ri.Cvar_Get ( "mme_blurType", "gaussian", CVAR_ARCHIVE );
	mme_blurGamma = ri.Cvar_Get ( "mme_blurGamma", "0", CVAR_ARCHIVE );
	mme_blurJitter = ri.Cvar_Get ( "mme_blurJitter", "1", CVAR_ARCHIVE );

	mme_dofFrames = ri.Cvar_Get ( "mme_dofFrames", "0", CVAR_ARCHIVE );
	mme_dofRadius = ri.Cvar_Get ( "mme_dofRadius", "2", CVAR_ARCHIVE );
	mme_dofQuick = ri.Cvar_Get ( "mme_dofQuick", "1", CVAR_ARCHIVE );
	mme_dofQuickRandom = ri.Cvar_Get ( "mme_dofQuickRandom", "3", CVAR_ARCHIVE ); 
	mme_dofQuickRandomMod = ri.Cvar_Get ( "mme_dofQuickRandomMod", "0.2", CVAR_ARCHIVE );
	mme_dofMaskInvert = ri.Cvar_Get ( "mme_dofMaskInvert", "0", CVAR_ARCHIVE );
	mme_dofMask = ri.Cvar_Get ( "mme_dofMask", "gfx/bokeh/circle", CVAR_ARCHIVE );

	mme_cpuSSE2 = ri.Cvar_Get ( "mme_cpuSSE2", "0", CVAR_ARCHIVE );
	mme_pbo = ri.Cvar_Get ( "mme_pbo", "1", CVAR_ARCHIVE );
	
	mme_depthRange = ri.Cvar_Get ( "mme_depthRange", "512", CVAR_ARCHIVE );
	mme_depthFocus = ri.Cvar_Get ( "mme_depthFocus", "1024", CVAR_ARCHIVE );
	mme_saveOverwrite = ri.Cvar_Get ( "mme_saveOverwrite", "0", CVAR_ARCHIVE );
	mme_saveStencil = ri.Cvar_Get ( "mme_saveStencil", "0", CVAR_INTERNAL);//CVAR_ARCHIVE ); //need to rewrite tr_backend.cpp :s
	mme_saveADM = ri.Cvar_Get ( "mme_saveADM", "1", CVAR_ARCHIVE );
	mme_saveDepth = ri.Cvar_Get ( "mme_saveDepth", "0", CVAR_ARCHIVE );
	mme_saveShot = ri.Cvar_Get ( "mme_saveShot", "1", CVAR_ARCHIVE );
	mme_saveAEKeyframes = ri.Cvar_Get ( "mme_saveAEKeyframes", "1", CVAR_ARCHIVE );
	mme_workMegs = ri.Cvar_Get ( "mme_workMegs", "128", CVAR_LATCH | CVAR_ARCHIVE );

	mme_rollingShutterEnabled = ri.Cvar_Get ( "mme_rollingShutterEnabled", "1", CVAR_LATCH | CVAR_ARCHIVE ); // float. like rollingshuttermultiplier.
	mme_rollingShutterBlur = ri.Cvar_Get ( "mme_rollingShutterBlur", "0.5", CVAR_LATCH | CVAR_ARCHIVE ); // float. like rollingshuttermultiplier.
	mme_rollingShutterPixels = ri.Cvar_Get ( "mme_rollingShutterPixels", "1", CVAR_LATCH | CVAR_ARCHIVE );
	mme_rollingShutterMultiplier = ri.Cvar_Get ( "mme_rollingShutterMultiplier", "1", CVAR_LATCH | CVAR_ARCHIVE );
	mme_mvShaderLoadOrder = ri.Cvar_Get ( "mme_mvShaderLoadOrder", "1", CVAR_LATCH | CVAR_ARCHIVE );

	mme_worldShader->modified = qtrue;
	mme_skyShader->modified = qtrue;
	mme_worldDeform->modified = qtrue;
	mme_worldBlend->modified = qtrue;
	mme_skyColor->modified = qtrue;
	mme_skyTint->modified = qtrue;
	mme_fboImageTint->modified = qtrue;

	Com_Memset( &shotData, 0, sizeof(shotData));
	//CANATODO, not exactly the best way to do this probably, but it works
	if (!workAlloc) {
		workSize = mme_workMegs->integer;
		if (workSize < 64)
			workSize = 64;
		else if (workSize > 512)
			workSize = 512;
		workSize *= 1024 * 1024 / 2; //dividing by 2 because other half is used in stereo
		workAlloc = (char *)calloc( workSize + 16, 1 );
		if (!workAlloc) {
			ri.Printf(PRINT_ALL, "Failed to allocate %d bytes for mme work buffer\n", workSize );
			allocFailed = qtrue;
			return;
		}
		workAlign = (char *)(((int)workAlloc + 15) & ~15);
	}
}
