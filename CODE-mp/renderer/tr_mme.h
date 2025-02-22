#pragma once

#include "tr_local.h"
#include <xmmintrin.h>


typedef struct {
	float fov;
	vec3_t viewAngles;
	vec3_t viewOrg;
	vec3_t viewAxis[3];
} AECamPosition;

typedef struct {
	vec3_t origin;
} AEPlayerPosition;

#define AVI_MAX_FRAMES	2000000
#ifdef SMALLAVIDEBUG
// Need to debug error that occurs when writing new avi file. doesnt happen often especially in slow debug mode
#define AVI_MAX_SIZE	1920*1080*4*5 
#else
#define AVI_MAX_SIZE	((2*1024-10)*1024*1024)
#endif
#define AVI_HEADER_SIZE	2048
#define AVI_MAX_FILES	1000



#define BLURMAX 8192





typedef struct mmeAviFile_s {
	char name[MAX_OSPATH];
	fileHandle_t f;
	float fps;
	int	width, height;
	unsigned int frames, aframes, iframes;
	int index[2*AVI_MAX_FRAMES];
	int aindex[2*AVI_MAX_FRAMES];
	int	written, awritten, maxSize;
	int header;
	int format;
	qboolean audio;
	mmeShotType_t type;
} mmeAviFile_t;

typedef struct {
	char name[MAX_OSPATH];
	int	 counter;
	mmeShotFormat_t format;
	mmeShotType_t type;
	mmeAviFile_t avi;
} mmeShot_t;

typedef struct {
	qboolean		take;
	float			fps;
	float			dofFocus, dofRadius;
	mmeShot_t		main, stencil, depth;
	float			jitter[BLURMAX][2];
} shotData_t;


typedef struct {
//	int		pixelCount;
	int		totalFrames;
	int		totalIndex;
	int		overlapFrames;
	int		overlapIndex;

	short	MMX[BLURMAX];
	short	SSE[BLURMAX];
	float	Float[BLURMAX];
} mmeBlurControl_t;

typedef struct {
	__m64	*accum;
	__m64	*overlap;
	int		count;
	mmeBlurControl_t *control;
} mmeBlurBlock_t;

typedef struct {
	mmeBlurControl_t control;
	mmeBlurBlock_t shot, depth, stencil;
	float	jitter[BLURMAX][2];
} blurData_t;

//void R_MME_GetShot( void* output, int rollingShutterFactor=1, int rollingShutterProgress = 0, int rollingShutterPixels=1,int rollingShutterBufferIndex =0 );
void R_MME_GetShot( void* output, int rollingShutterFactor, int rollingShutterProgress , int rollingShutterPixels,int rollingShutterBufferIndex  );
void R_MME_GetShot(void* output); // Non rolling shutter version
void R_MME_GetStencil( void *output );
void R_MME_GetDepth( byte *output );
void R_MME_SaveShot( mmeShot_t *shot, int width, int height, float fps, byte *inBuf, qboolean audio, int aSize, byte *aBuf );

void mmeAviShot( mmeAviFile_t *aviFile, const char *name, mmeShotType_t type, int width, int height, float fps, byte *inBuf, qboolean audio );
void mmeAviSound( mmeAviFile_t *aviFile, const char *name, mmeShotType_t type, int width, int height, float fps, const byte *soundBuf, int size );
void aviClose( mmeAviFile_t *aviFile );

void MME_AccumClearSSE( void *w, const void *r, short int mul, int count );
void MME_AccumAddSSE( void* w, const void* r, short int mul, int count );
void MME_AccumShiftSSE( const void *r, void *w, int count );

void R_MME_BlurAccumAdd( mmeBlurBlock_t *block, const __m64 *add );
void R_MME_BlurOverlapAdd( mmeBlurBlock_t *block, int index );
void R_MME_BlurAccumShift( mmeBlurBlock_t *block  );
void blurCreate( mmeBlurControl_t* control, const char* type, int frames );
void R_MME_JitterTable(float *jitarr, int num);

float R_MME_FocusScale(float focus);
void R_MME_ClampDof(float *focus, float *radius);

extern cvar_t	*mme_aviFormat;

extern cvar_t	*mme_blurJitter;
extern cvar_t	*mme_dofFrames;
extern cvar_t	*mme_dofRadius;
extern cvar_t	*mme_dofQuick;
extern cvar_t	*mme_dofQuickRandom;
extern cvar_t	* mme_dofQuickRandomMod;
extern cvar_t	*mme_dofMask;
extern cvar_t	* mme_dofMaskInvert;

ID_INLINE byte * R_MME_BlurOverlapBuf( mmeBlurBlock_t *block ) {
	mmeBlurControl_t* control = block->control;
	int index = control->overlapIndex % control->overlapFrames;
	return (byte *)( block->overlap + block->count * index );
}
