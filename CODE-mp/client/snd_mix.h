#pragma once

#include <vector>
#include <string>
#include <memory>
#include "snd_resample.hpp"

//All mixing speeds have this as 1.0
#define		MIX_SPEED		44100//22050
#define		MIX_SHIFT		8

#define		FIRSCALE		4
#define		ONETHIRD		342*FIRSCALE


typedef struct {
	long starttime;
	long duration;
	vec3_t position;
	float gain;
} mmeADMBlock_t;
typedef struct {
	std::vector<mmeADMBlock_t> blocks;
	//std::string soundName;
	sfxHandle_t sfxHandle;
	soundChannel_t channelHandle;
} mmeADMObject_t;
typedef struct {
	std::vector<mmeADMObject_t> objects;

} mmeADMChannelInfo_t;

typedef struct mixSound_s {
	int			lastUsed;
	int			speed;
	int64_t		samples;
	int			size;
	sfxHandle_t	handle;
	struct		mixSound_s *prev, *next;
	short		data[0];
} mixSound_t;

typedef struct {
	sfxHandle_t		handle;
	int				index;
	vec3_t			origin;
	short			entNum;
	char			entChan;
	char			hasOrigin;
	char			wasMixed;
	std::unique_ptr<piecewiseResample> resampler;
} mixChannel_t;

typedef struct {
	const void		*parent;
	sfxHandle_t		handle;
	int				index;
	loopQueue_t*	queueItem;
	std::unique_ptr<piecewiseResample> resampler;
} mixLoop_t;

typedef struct {
	openSound_t		*sound;
	char			soundName[MAX_QPATH];
	int				index;
	int				length;
	int				done;
	short			last[2];
} mixBackground_t;

typedef struct {
	int				statel, stater;
	double			zl[ONETHIRD], zr[ONETHIRD];
	int				index;
} mixEffect_t;

extern	cvar_t		*s_effects;

extern void S_DMAInit(void);
extern void S_DMA_Update( float scale );
void S_DMAClearBuffer(void);
void S_DMAStopSound(int entityNum, int entchannel, sfxHandle_t sfxHandle);
void S_MMEStopSound(int entityNum, int entchannel, sfxHandle_t sfxHandle);

void S_MixInit(void);
void S_MixBackground( mixBackground_t *background, int speed, int count, int *output );
void S_MixChannels( mixChannel_t *ch, int channels, int speed, int count, int *output,  short* outputADM=nullptr, int admTotalChannelCount=0, mmeADMChannelInfo_t* admChannelInfoArray=nullptr, long admAbsoluteTime=0);
void S_MixLoops( mixLoop_t *loop, int loopCount, int speed, int count, int *output, short* outputADM=nullptr, int admTotalChannelCount = 0, mmeADMChannelInfo_t* admChannelInfoArray=nullptr, long admAbsoluteTime=0);
void S_MixClipOutput (int count, const int *input, short *output, int outStart, int outMask);
const mixSound_t *S_MixGetSound( sfxHandle_t sfxHandle );

void S_MixEffects(mixEffect_t *effect, int speed, const int count, int *output);
void S_EffectInit(void);
