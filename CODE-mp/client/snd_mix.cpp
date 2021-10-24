// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "snd_local.h"
#include "snd_mix.h"
#include <renderer\tr_mme.h>

static	mixSound_t		*mixSounds[SFX_SOUNDS];

extern shotData_t shotData;

#define DEF_COMSOUNDMEGS "32"
static  mixSound_t		*mixAllocSounds = 0;
static  mixSound_t		mixEmptySound;

static cvar_t	*s_mixSame;
static cvar_t	*s_mixSameTime;
cvar_t			*s_effects;

#define		SOUND_FULLVOLUME	256
#define		SOUND_ATTENUATE		0.0008f

static mixSound_t *S_MixAllocSound( int samples ) {
	int allocSize, lastUsed = (1 << 30);
	mixSound_t *sound, *allocSound;
	int countUsed;
		
	/* Make sure the size contains the header and is aligned on a pointer size boundary */
	allocSize = sizeof( mixSound_t ) + samples * 2;
	allocSize = (allocSize + (sizeof(void *) -1)) & ~(sizeof(void *)-1);
	/* First pass we just scan for a free block and find the oldest sound */
	allocSound = 0;
	countUsed = 0;
	for (sound = mixAllocSounds; sound; sound = sound->next) {
		countUsed += sound->size;
		if (sound->handle) {
			if (sound->lastUsed < lastUsed) {
				lastUsed = sound->lastUsed;
				allocSound = sound;
			}
			continue;
		} else {
			if ( allocSize <= sound->size ) {
				allocSound = sound;
				break;
			}
		}
	}
	/* Still haven't found a block, this is bad... */
	if (!allocSound)
		return 0;
	mixSounds[allocSound->handle] = 0;
	allocSound->handle = 0;
	/* Do we need to allocate any extra blocks to make the right size */
	while (allocSound->size < allocSize) {
		/* First always check if we're surrounded by an empty block and take that first */
		if (allocSound->prev && !allocSound->prev->handle) {
takePrev:
			mixSounds[allocSound->prev->handle] = 0;
			/* Free block before this, take it! */
			allocSound->prev->size += allocSound->size;
			if (allocSound->next)
				allocSound->next->prev = allocSound->prev;
			allocSound->prev->next = allocSound->next;
			allocSound = allocSound->prev;
		} else if (allocSound->next && !allocSound->next->handle) {
takeNext:
			/* Free block after this, take it! */
			mixSounds[allocSound->next->handle] = 0;
			allocSound->size += allocSound->next->size;
			if (allocSound->next->next)
				allocSound->next->next->prev = allocSound;
			allocSound->next = allocSound->next->next;
		} else if (allocSound->prev && allocSound->next) {
			/* See if the one before or after this is older and take that one */
			if (allocSound->prev->lastUsed < allocSound->next->lastUsed)
				goto takePrev;
			else
				goto takeNext;
		} else if (allocSound->prev) {
			goto takePrev;
		} else if (allocSound->next) {
			goto takeNext;
		} else {
			Com_Error( ERR_FATAL, "Mix allocation block is corrupt\n");
		}
	}
	/* Is our new block bigger than we need, put it at the end */
	if (allocSound->size > ( 256 + allocSize + sizeof( mixSound_t ))) {
		int remainSize = allocSound->size - allocSize;
		sound = allocSound;
		
		allocSound = (mixSound_t*)(((byte*)sound) + remainSize);
		if (sound->next)
			sound->next->prev = allocSound;
		allocSound->next = sound->next;
		allocSound->prev = sound;
		sound->next = allocSound;
		sound->handle = 0;
		sound->size = remainSize;
		allocSound->size = allocSize;
	}
	return allocSound;
}

const mixSound_t *S_MixGetSound( sfxHandle_t sfxHandle ) {
	sfxEntry_t *entry;
	mixSound_t *sound;
	openSound_t *openSound;
	char *isMP3;

	sound = mixSounds[sfxHandle];
	if ( sound ) {
		sound->lastUsed = com_frameTime;	
		return sound;
	}

	entry = sfxEntries + sfxHandle;
	openSound = S_SoundOpen( entry->name );
	if (!openSound) {
		return mixSounds[sfxHandle] = &mixEmptySound;
	}
	/* Jedi Outcast does not limit sounds at all */
/*	if (openSound->totalSamples > (1 << (30 - MIX_SHIFT))) {
		S_SoundClose( openSound );
		Com_Printf( "Mixer:Sound file too large\n" );
		return mixSounds[sfxHandle] = &mixEmptySound;
	}
*/	sound = S_MixAllocSound( openSound->totalSamples );
	if (!sound) {
		S_SoundClose( openSound );
		Com_Printf( "Mixer:Failed to alloc memory for sound, try to increase com_soundMegs\n" );
		return mixSounds[sfxHandle] = &mixEmptySound;
	}
	mixSounds[ sfxHandle ] = sound;
	sound->speed = (openSound->rate << MIX_SHIFT) / MIX_SPEED;
	sound->handle = sfxHandle;
	sound->lastUsed = com_frameTime;
	sound->samples = S_SoundRead( openSound, qfalse, openSound->totalSamples, sound->data );
	isMP3 = strchr(entry->name, '.mp3'); // MP3s aren't being opened fully sometimes, could spam the console
	if (sound->samples != openSound->totalSamples && !isMP3) {
		Com_Printf( "Mixer:Failed to load %s fully\n", entry->name );
	}
	sound->samples <<= MIX_SHIFT;
	S_SoundClose( openSound );
	return sound;
}

void S_MixClipOutput(int count, const int *input, short *output, int outStart, int outMask) {
	int		i;
	int		val;

	for (i=0 ; i<count ; i++) {
		val = input[i*2+0] >> MIX_SHIFT;
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < -32768)
			val = -32768;
		output[outStart*2 + 0] = val;

		val = input[i*2+1] >> MIX_SHIFT;
		if (val > 0x7fff)
			val = 0x7fff;
		else if (val < -32768)
			val = -32768;
		output[outStart*2 + 1] = val;
		outStart = (outStart+1) & outMask;
	}
}

static void S_MixSpatialize(const vec3_t origin, float volume, int *left_vol, int *right_vol, vec3_t admPosition=nullptr ) {
    vec_t		dot;
    vec_t		dist;
    vec_t		lscale, rscale, scale;
    vec3_t		source_vec;
    vec3_t		vec;

	const float dist_mult = SOUND_ATTENUATE * s_attenuate->value;
	
	// calculate stereo seperation and distance attenuation
	VectorSubtract(origin, s_listenOrigin, source_vec);

	dist = VectorNormalize(source_vec);
	vec_t absoluteDist = dist;
	dist -= SOUND_FULLVOLUME;
	if (dist < 0)
		dist = 0;			// close enough to be at full volume
	dist *= dist_mult;		// different attenuation levels
	
	VectorRotate( source_vec, s_listenAxis, vec );

	dot = -vec[1];
	rscale = 0.5 * (1.0 + dot);
	lscale = 0.5 * (1.0 - dot);

	if (admPosition) { // Todo: Figure out if I have the correct signedness here or if I need to invert some.
		admPosition[0] = -vec[1]* absoluteDist; // left/right
		admPosition[1] = vec[0]* absoluteDist; // front/back
		admPosition[2] = vec[2]* absoluteDist; // up/down
	}

	//rscale = s_separation->value + ( 1.0 - s_separation->value ) * dot;
	//lscale = s_separation->value - ( 1.0 - s_separation->value ) * dot;
//	if ( rscale < 0 )
//		rscale = 0;
//	if ( lscale < 0 ) 
//		lscale = 0;
	// add in distance effect
	scale = (1.0 - dist) * rscale;
	*right_vol = (volume * scale);
	if (*right_vol < 0)
		*right_vol = 0;

	scale = (1.0 - dist) * lscale;
	*left_vol = (volume * scale);
	if (*left_vol < 0)
		*left_vol = 0;
}

static void S_MixChannel( mixChannel_t *ch, int speed, int count, int *output, short* outputADM = nullptr, int outputADMOffsetPerSample =0, vec3_t admPosition = nullptr) {
	const mixSound_t *sound;
	int i, leftVol, rightVol;
	int64_t index, indexAdd, indexLeft;
	float *origin;
	const short *data;
	float volume;

	if (ch->entChan == CHAN_VOICE)
		volume = s_volumeVoice->value * (1 << MIX_SHIFT) * 0.5;
	else
		volume = s_volume->value * (1 << MIX_SHIFT) * 0.5;

	origin = (!ch->hasOrigin) ? s_entitySounds[ch->entNum].origin : ch->origin;
	if (!ch->hasOrigin && s_entitySounds[ch->entNum].origin[0] == 0 && s_entitySounds[ch->entNum].origin[1] == 0 && s_entitySounds[ch->entNum].origin[2] == 0)
		origin = s_listenOrigin;

	S_MixSpatialize( origin, volume , &leftVol, &rightVol,admPosition );
	sound = S_MixGetSound( ch->handle );

	index = ch->index;
	indexAdd = (sound->speed * speed) >> MIX_SHIFT;
	indexLeft = sound->samples - index;
	ch->wasMixed = (leftVol | rightVol) > 0;
	/*if (!ch->wasMixed) {
		indexAdd *= count;
		if ( indexAdd >= indexLeft ) {
			ch->handle = 0;
		} else {
			ch->index += indexAdd;
		}
		return;
	}*/ // We want ADM to get data even if JK deems it too quiet.
	data = sound->data;
	if (!indexAdd)
		return;
	indexLeft /= indexAdd;
	if ( indexLeft <= count) {
		count = indexLeft;
		ch->handle = 0;
	}

	short* outputADMDisplacedPointer = outputADM;
	for (i = 0; i < count;i++) {
		int sample;
		sample = data[index >> MIX_SHIFT];
		if (!!outputADM) {

			*outputADMDisplacedPointer = data[index >> MIX_SHIFT];
			outputADMDisplacedPointer += outputADMOffsetPerSample;
		}
		output[i*2+0] += sample * leftVol;
		output[i*2+1] += sample * rightVol;
		index += indexAdd;
	}

	ch->index = index;
}

void S_MixChannels( mixChannel_t *ch, int channels, int speed, int count, int *output, short *outputADM, int admTotalChannelCount, mmeADMChannelInfo_t* admChannelInfoArray, long admAbsoluteTime) {
	int queueLeft, freeLeft = channels;
	int activeCount;
	mixChannel_t *free = ch;
	int channelIndex = 0; // For ADM
	const channelQueue_t *q = s_channelQueue;

	bool* isNew = new bool[channels] {false}; // For ADM 
	long long minBlockDurationSamples = 0; // We set this according to fps so as not to bloat the ADM metadata in slow capturing modes like rolling shutter
	if (shotData.fps) { // I hope it's (still?) set here
		double timeInSeconds = 1.0 / shotData.fps;
		minBlockDurationSamples = (long long)(0.5 + timeInSeconds * (float)MME_SAMPLERATE);
		// In short, we don't necessarily want more than one position update per frame. What's the point after all? There's interpolation anyway.
		// We do this in the final ADM generation too but with too short frametimes during capture, we get a memory overflow in the end if we don't thin out.
		minBlockDurationSamples /= 4; // We will actually allow 4*fps here in the initial data gathering so that there's no lack of precision later. Think Nyquist etc, source data should be at least 2x final sampling.
		// Maybe im misunderstanding Nyquist, but I think it can't hurt.
	}

	/* Go through the sound queue and add new channels */
	for (queueLeft = s_channelQueueCount; queueLeft > 0; queueLeft--, q++) {
		int scanCount, foundCount;
		mixChannel_t *scanChan;
		if (freeLeft <= 0) {
//			Com_Printf("No more free channels.\n");
			break;
		}
		foundCount = 0;
		scanChan = ch;
		for (scanCount = channels;scanCount > 0;scanCount--, scanChan++ ) {
            /* Large group of tests to see if this one should be counted as a same sound */
			/* Same sound effect ? */
			if ( q->handle != scanChan->handle )
				continue;
			/* Reasonably same start ? */
			if ( scanChan->index > (MIX_SPEED * s_mixSameTime->value))
				continue;
			if ( q->hasOrigin ) {
				vec3_t dist;
				/* Same or close origin */
				if (!scanChan->hasOrigin)
					continue;
				VectorSubtract( q->origin, scanChan->origin, dist );
				if (VectorLengthSquared( dist ) > 50*50)
					continue;
			} else {
				/* Coming from the same entity */
				if (q->entNum != scanChan->entNum )
					continue;
			}
			foundCount++;
			if (foundCount > s_mixSame->integer)
				goto skip_alloc;
		}
		for (;freeLeft > 0;free++, freeLeft--,channelIndex++) {
			if (!free->handle) {
				free->handle = q->handle;
				free->entChan = q->entChan;
				free->entNum = q->entNum;
				free->index = 0;
				VectorCopy( q->origin, free->origin );
				free->hasOrigin = q->hasOrigin;
				freeLeft--;
				free++;
				isNew[channelIndex] = true; // For ADM
				break;
			}
		}
skip_alloc:;
	}
	activeCount = 0;
	short* admDisplacedPointer = outputADM;
	mmeADMChannelInfo_t* displacedAdmChannelInfoArray = admChannelInfoArray;
	channelIndex = 0; // For ADM
	for (;channels>0;channels--, ch++, channelIndex++) {
		if (ch->handle <= 0 )
			continue;
		activeCount++;
		if (!!outputADM) {

			admDisplacedPointer = outputADM+channelIndex;
			displacedAdmChannelInfoArray= admChannelInfoArray+channelIndex;
			if (!isNew[channelIndex] && displacedAdmChannelInfoArray->objects.size() >0) { // Means it's still the same sound as before
					// We only let it stay the same object if both parent and sound are the same
				mmeADMObject_t* lastObject = &displacedAdmChannelInfoArray->objects.back();
				mmeADMBlock_t* lastBlock = &lastObject->blocks.back();
				if (lastBlock->duration >= minBlockDurationSamples) {

					mmeADMBlock_t newBlock;
					newBlock.starttime = admAbsoluteTime;
					newBlock.duration = count;
					newBlock.gain = 0.5;
					//newBlock.gain = ((float)mixLoops[i].queueItem->volume) / 255.0f; // Not sure if the division is exactly correct. But as long as everything is right relative to each other it should be ok.
					// TODO Figure out gain AND make a special note in object for voices! To mark it for that thingie.
					lastObject->blocks.push_back(newBlock);
				}
				else {
					// If time delta during capture is too small, we don't want a single block for each single capture
					// because that will bloat memory usage and at x86 that's a serious problem.
					// We will end up with hundreds of megabytes of metadata for just a few seconds of video
					// Therefore we limit. It's a bit ugly because technically we're possibly missing the end position
					// but maybe we can fix that someday somehow.

					if ((lastBlock->starttime + lastBlock->duration) != admAbsoluteTime) {

						Com_Printf("Channelmixer: ADM block misalignment. This should never happen.\n");
						lastBlock->duration = admAbsoluteTime- lastBlock->starttime;
					}
					else {

						lastBlock->duration += count;
					}

				}
			}
			else {
				mmeADMObject_t newObject;
				//newObject.soundName = (sfxEntries + ch->handle)->name;
				newObject.sfxHandle = ch->handle;
				newObject.channelHandle = (soundChannel_t)ch->entChan;
				mmeADMBlock_t newBlock;
				newBlock.starttime = admAbsoluteTime;
				newBlock.duration = count;
				//newBlock.gain = ((float)mixLoops[i].queueItem->volume) / 255.0f; // Not sure if the division is exactly correct. But as long as everything is right relative to each other it should be ok.
				newBlock.gain = 0.5;
				// TODO Figure out gain AND make a special note in object for voices! To mark it for that thingie.
				newObject.blocks.push_back(newBlock);
				displacedAdmChannelInfoArray->objects.push_back(newObject);
			}

			//S_MixLoop(&mixLoops[i], mixLoops[i].queueItem, speed, count, output, displacedADMOutput, admTotalChannelCount, admPosition);
			

			vec3_t admPosition;
			S_MixChannel(ch, speed, count, output, admDisplacedPointer, admTotalChannelCount, admPosition);
			mmeADMBlock_t* currentBlock = &displacedAdmChannelInfoArray->objects.back().blocks.back();
			VectorCopy(admPosition, currentBlock->position);

			//admDisplacedPointer++;
			//displacedAdmChannelInfoArray++;
		}
		else {
			S_MixChannel(ch, speed, count, output, admDisplacedPointer, admTotalChannelCount);
		}
	}

	delete[] isNew;
}

#define MAX_DOPPLER_SCALE 50			//arbitrary
static int S_MixDopplerOriginal( int speed, const vec3_t origin, const vec3_t velocity ) {
	vec3_t	out;
	float	lena, lenb, scale;

	lena = DistanceSquared( s_listenOrigin, origin);
	VectorMA( origin, s_dopplerFactor->value, velocity, out);
	lenb = DistanceSquared( s_listenOrigin, out);
	scale = lenb/(lena*100);
	if (scale > MAX_DOPPLER_SCALE) {
		speed *= MAX_DOPPLER_SCALE;
	} else if (scale > 1) {
		speed *= scale;
	}
	return speed;
}

static int S_MixDopplerFull( int speed, const vec3_t origin, const vec3_t velocity ) {
	vec3_t	delta;
	float	vL, vO, ratio;

	VectorSubtract( origin, s_listenOrigin, delta );
	VectorNormalize( delta );

	vL = DotProduct( delta,  s_listenVelocity );
	vO = DotProduct( delta,  velocity );
	
	vL *= s_dopplerFactor->value;
	vO *= s_dopplerFactor->value;

	vL = s_dopplerSpeed->value - vL;
	vO = s_dopplerSpeed->value + vO;
    
	ratio = vL / vO;
	return speed * ratio;
}

static void S_MixLoop( mixLoop_t *loop, const loopQueue_t *lq, int speed, int count, int *output, short* outputADM=nullptr, int outputADMOffsetPerSample = 0,vec3_t admPosition = nullptr) {
	const mixSound_t *sound;
	int i, leftVol, rightVol;
	int64_t index, indexAdd, indexTotal;
	const short *data;
	float volume;

	if (s_doppler->integer == 2)
		speed = S_MixDopplerFull( speed, lq->origin, lq->velocity );
	else if (s_doppler->integer )
		speed = S_MixDopplerOriginal( speed, lq->origin, lq->velocity );

	volume = s_volume->value * lq->volume;
	S_MixSpatialize( lq->origin, volume, &leftVol, &rightVol,admPosition );
	sound = S_MixGetSound( loop->handle );

	index = loop->index;
	indexAdd = (sound->speed * speed) >> MIX_SHIFT;
	indexTotal = sound->samples;
	data = sound->data;
	if ( (leftVol | rightVol) <= 0 ) {
		index += count * indexAdd;
		index %= indexTotal;
	}
	else {

		short* displacedADMOutput = outputADM;
		for (i = 0; i < count; i++) {
			int sample;
			while (index >= indexTotal) {
				index -= indexTotal;
			}
			sample = data[index >> MIX_SHIFT];
			if (outputADM) {
				*displacedADMOutput = data[index >> MIX_SHIFT];
				displacedADMOutput += outputADMOffsetPerSample;
			}
			output[i * 2 + 0] += sample * leftVol;
			output[i * 2 + 1] += sample * rightVol;
			index += indexAdd;
		}
	}
	loop->index = index;
}

void S_MixLoops( mixLoop_t *mixLoops, int loopCount, int speed, int count, int *output, short* outputADM, int admTotalChannelCount, mmeADMChannelInfo_t* admChannelInfoArray, long admAbsoluteTime) {
	
	
	bool* isAlreadyInLoops = new bool[s_loopQueueCount] {false};

	// For ADM:
	bool* isOngoing = new bool[loopCount] {false}; // Need this pretty much only for the ADM metadata creation
	bool* isSoundChanged = new bool[loopCount] {false}; // Need this pretty much only for the ADM metadata creation
	long long minBlockDurationSamples = 0; // We set this according to fps so as not to bloat the ADM metadata in slow capturing modes like rolling shutter
	if (shotData.fps) { // I hope it's (still?) set here
		double timeInSeconds = 1.0 / shotData.fps;
		minBlockDurationSamples = (long long)(0.5 + timeInSeconds * (float)MME_SAMPLERATE);
		// In short, we don't necessarily want more than one position update per frame. What's the point after all? There's interpolation anyway.
		// We do this in the final ADM generation too but with too short frametimes during capture, we get a memory overflow in the end if we don't thin out.
		minBlockDurationSamples /= 4; // We will actually allow 4*fps here in the initial data gathering so that there's no lack of precision later. Think Nyquist etc, source data should be at least 2x final sampling.
		// Maybe im misunderstanding Nyquist, but I think it can't hurt.
	}

	// Step 1: Decativate loops that arent in the queue anymore:
	int freeLoopCount = 0;
	for (int i = 0; i < loopCount;i++) {
		
		// This one has an empty parent, no need to search for a match
		if (!mixLoops[i].parent) {
			freeLoopCount++;
			continue;
		}

		// Find match
		bool parentIsStillInQueue = false;
		for (int b = 0; b < s_loopQueueCount; b++) {

			// Same parent. Match.
			if (s_loopQueue[b].parent == mixLoops[i].parent) {

				parentIsStillInQueue = true; // Indicate that we have found a match.

				// check if we're still playing the same sound 
				// if not, update to new sound and reset index (playing offset)
				if (s_loopQueue[b].handle != mixLoops[i].handle) {
					mixLoops[i].handle = s_loopQueue[b].handle;
					mixLoops[i].index = 0;
					isSoundChanged[i] = true;
				}
				mixLoops[i].queueItem = &s_loopQueue[b];
				isOngoing[i] = true; // Indicate that this should be the same object
				isAlreadyInLoops[b] = true; // Indicate that we do not need to add this later as it's already added.

				break;
			}
		}

		// If there is no match, deactivate this one.
		if (!parentIsStillInQueue) {
			mixLoops[i].parent = 0;
			freeLoopCount++;
		}
	}

	// Step 2: Fill empty positions with new loops
	for (int b = 0; b < s_loopQueueCount; b++) {
		

		// Shouldn't happen but eh, safe is safe
		if (!s_loopQueue[b].parent) {
			continue;
		}
		// No more free loop positions
		if (freeLoopCount <= 0) {
			break; // I guess those remaining queued loops are shit out of luck!
		}
		// Is already in the loop array, skip
		if (isAlreadyInLoops[b]) {

			continue;
		}
		// Find free place to put it and put it there.
		for (int i = 0; i < loopCount; i++) {

			if (!mixLoops[i].parent) {
				mixLoops[i].parent = s_loopQueue[b].parent;
				mixLoops[i].handle = s_loopQueue[b].handle;
				mixLoops[i].index = 0;
				mixLoops[i].queueItem = &s_loopQueue[b];
				freeLoopCount--;
				break;
			}
		}
	}

	// Step 3: Do the mixing
	short* displacedADMOutput = outputADM;
	mmeADMChannelInfo_t* displacedAdmChannelInfoArray = admChannelInfoArray;
	for (int i = 0; i < loopCount; i++) {

		if (outputADM) {

			if (!!mixLoops[i].parent) {
				
				
				if (isOngoing[i] && !isSoundChanged[i] && displacedAdmChannelInfoArray->objects.size() > 0) { // Means it's still the same parent and same sound as before
					// We only let it stay the same object if both parent and sound are the same
					mmeADMObject_t* lastObject = &displacedAdmChannelInfoArray->objects.back();
					mmeADMBlock_t* lastBlock = &lastObject->blocks.back();
					if (lastBlock->duration >= minBlockDurationSamples) {

						mmeADMBlock_t newBlock;
						newBlock.starttime = admAbsoluteTime;
						newBlock.duration = count;
						newBlock.gain = ((float)mixLoops[i].queueItem->volume) / 256.0f; // Not sure if the division is exactly correct. But as long as everything is right relative to each other it should be ok.
						lastObject->blocks.push_back(newBlock);
					}
					else {
						// If time delta during capture is too small, we don't want a single block for each single capture
						// because that will bloat memory usage and at x86 that's a serious problem.
						// We will end up with hundreds of megabytes of metadata for just a few seconds of video
						// Therefore we limit. It's a bit ugly because technically we're possibly missing the end position
						// but maybe we can fix that someday somehow.

						// Quick sanity check for the impossible case...
						if ((lastBlock->starttime + lastBlock->duration) != admAbsoluteTime) {

							Com_Printf("Loopmixer: ADM block misalignment. This should never happen.\n");
							lastBlock->duration = admAbsoluteTime - lastBlock->starttime;
						}
						else {

							lastBlock->duration += count;
						}

					}
					
					
				}
				else {
					mmeADMObject_t newObject;
					//newObject.soundName = (sfxEntries+mixLoops[i].handle)->name;
					newObject.sfxHandle = mixLoops[i].handle;
					mmeADMBlock_t newBlock;
					newBlock.starttime = admAbsoluteTime;
					newBlock.duration = count; 
					newBlock.gain = ((float)mixLoops[i].queueItem->volume) / 256.0f; // Not sure if the division is exactly correct. But as long as everything is right relative to each other it should be ok.
					newObject.blocks.push_back(newBlock);
					displacedAdmChannelInfoArray->objects.push_back(newObject);
				}

				vec3_t admPosition;
				S_MixLoop(&mixLoops[i], mixLoops[i].queueItem, speed, count, output, displacedADMOutput, admTotalChannelCount, admPosition);
				mmeADMBlock_t* currentBlock = &displacedAdmChannelInfoArray->objects.back().blocks.back();
				VectorCopy(admPosition, currentBlock->position);
			}
			displacedADMOutput++;
			displacedAdmChannelInfoArray++;
		}
		else {
			if (!!mixLoops[i].parent) {

				S_MixLoop(&mixLoops[i], mixLoops[i].queueItem, speed, count, output, displacedADMOutput, admTotalChannelCount);
			}
		}
		
	}

	delete[] isAlreadyInLoops;
	delete[] isOngoing;
	delete[] isSoundChanged;


}

void S_MixBackground( mixBackground_t *background, int speed, int count, int *output ) {
	int		volumeMul;
	short	buf[2048][2];

	speed = (MIX_SPEED << MIX_SHIFT) / dma.speed;
	if ( s_background.playing ) {
		/* Do we need a reload */
		if ( s_background.reload ) {
			/* Already playing, check if we already have the right one open */
			if ( background->sound && Q_stricmp( background->soundName, s_background.startName )) {
				S_SoundClose( background->sound );
				background->sound = 0;
			}
			/* Do we have to load a sound */
			if ( !background->sound ) {
                background->sound = S_SoundOpen( s_background.startName );
				Q_strncpyz( background->soundName, s_background.startName, sizeof( background->soundName));
				if ( !background->sound && !s_background.override) {
					/* Regular try the loop sound */
	                background->sound = S_SoundOpen( s_background.loopName );
					Q_strncpyz( background->soundName, s_background.loopName, sizeof( background->soundName));
				}
			}
			if ( background->sound ) {
				/* Should we try a seek with an override */
				if ( s_background.override ) {
					if ( s_background.seekTime >= 0 ) {
						int sample = background->sound->rate * s_background.seekTime;
						S_SoundSeek( background->sound, sample );
						background->length = background->sound->rate * s_background.length;
					} else {
						background->length = 0;
					}
				} else {
					S_SoundSeek( background->sound, 0 );
					background->length = 0;
				}
			}
			background->index = 1 << MIX_SHIFT;
		}
		if (!count)
			return;
		//Special state used to signal stopped music
		if ( s_background.seekTime < 0 && s_background.override ) {
			Com_Memset( output, 0, count * sizeof(int) * 2 );
			return;
		}
		volumeMul = s_musicVolume->value * ( 1 << MIX_SHIFT);
		background->done += count;
		while ( count > 0 ) {
			openSound_t *sound;
			int indexAdd, indexLast, read, need;

			sound = background->sound;
			/* Fill the rest of the block with silence */
			if ( !sound ) {
				Com_Memset( output, 0, count * sizeof(int) * 2 );
				return;
			}
			indexAdd = (sound->rate * speed ) / MIX_SPEED;
			if ( background->length ) {
				background->length -= indexAdd;
				if ( background->length < 0 ) {
					background->length = 0;
					s_background.seekTime = -1;
				}
			}
			indexLast = background->index + count * indexAdd;
			need = (indexLast >> MIX_SHIFT);
			if ( need > (sizeof( buf ) / 4) - 1)
				need = (sizeof( buf ) / 4) - 1;

			/* Is this read gonna take us to the end of the sound stream */
			read = S_SoundRead( sound, qtrue, need, buf[1] );
			if ( read == 0) {
				if ( !s_background.override) {
					if (!strcmp( s_background.loopName, background->soundName))
						S_SoundSeek( sound, 0 );
					else {
						S_SoundClose( background->sound );
						background->sound = S_SoundOpen( s_background.loopName );
						Q_strncpyz( background->soundName, s_background.loopName, sizeof( background->soundName));
					}
					background->index = 1 << MIX_SHIFT;
					if (background->sound)
						continue;
				}
				background->length = 0;
				//Fill the rest of the buffer with 0 with music stopped
				Com_Memset( output, 0, count * sizeof(int) * 2 );
				return;
			} else {
				/* Copy back original last sample if it needs to be processed again */
				buf[0][0] = background->last[0];
				buf[0][1] = background->last[1];

				if ( ((1 + read) << MIX_SHIFT ) < indexLast)
					indexLast = (1 + read) << MIX_SHIFT;
				while ( background->index < indexLast ) {
					int index = background->index >> MIX_SHIFT;
					output[0] = buf[index][0] * volumeMul;
					output[1] = buf[index][1] * volumeMul;
					count--;output+=2;
					background->index += indexAdd;
				}
				indexAdd = (background->index >> MIX_SHIFT);
				background->last[0] = buf[ indexAdd ][0];
				background->last[1] = buf[ indexAdd ][1];
				background->index &= (1 << MIX_SHIFT) - 1;
			}
		}
	} else {
		//Close up the sound file if no longer playing
		if ( background->sound ) {
			S_SoundClose( background->sound );
			background->sound = 0;
		}
		Com_Memset( output, 0, count * sizeof(int) * 2 );
	}
}

void S_MixInit( void ) {
	cvar_t *cv;
	int allocSize;

	if (mixAllocSounds) {
		Com_Memset( mixSounds, 0, sizeof( mixSounds ));
		free( mixAllocSounds );
		mixAllocSounds = 0;
	}
	mixEmptySound.speed = (MIX_SPEED << MIX_SHIFT) / MIX_SPEED;;
	mixEmptySound.samples = 1 << MIX_SHIFT;

	cv = Cvar_Get( "com_soundMegs", DEF_COMSOUNDMEGS, CVAR_LATCH | CVAR_ARCHIVE );
	allocSize = cv->integer * 1024 * 1024;
	/* Sneakily force the soundmegs at atleast 32 mb */
	if (allocSize < 32 * 1024 * 1024)
		allocSize = 32 * 1024 * 1024;
	//Use calloc, seems faster when debugging
	mixAllocSounds = (mixSound_t *)calloc( allocSize, 1 );
	if (!mixAllocSounds) {
		Com_Error (ERR_FATAL, "Failed to allocate memory for sound system\n");
	}
	/* How many similar sounding close to eachother sound effects */
	s_mixSame = Cvar_Get( "s_mixSame", "2", CVAR_ARCHIVE );
	s_mixSameTime = Cvar_Get( "s_mixSameTime", "10", CVAR_ARCHIVE );

	s_effects = Cvar_Get( "s_effects", "1", CVAR_ARCHIVE );
	S_EffectInit();

	/* Init the first block */
	mixAllocSounds->prev = 0;
	mixAllocSounds->next = 0;
	mixAllocSounds->handle = 0;
	mixAllocSounds->size = allocSize;
	mixAllocSounds->lastUsed = 0;
}
