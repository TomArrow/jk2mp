// Copyright (C) 2005 Eugene Bujak.
//
// cl_demos.c -- Enhanced client-side demo player

#define DEMO_MAX_INDEX	36000 // Controls how much of the demo positions get precached (after that performance is terrible). Default was 3600
#define DEMO_PLAY_CMDS  256
#define DEMO_SNAPS  32
#define DEMOCONVERTFRAMES 16

#define DEMO_CLIENT_UPDATE 0x1
#define DEMO_CLIENT_ACTIVE 0x2

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

typedef struct {
	int			offsets[MAX_CONFIGSTRINGS];
	int			used;
	char		data[MAX_GAMESTATE_CHARS];
} demoString_t;

typedef struct {
	// To check if the frame is already cached, in which case don't redo the work. Is an ugly workaround. Fix someday.
	int			frameNumber; 
	int			filePosAfter;

	int			serverTime;
	playerState_t clients[MAX_CLIENTS];
	byte		clientData[MAX_CLIENTS];
	entityState_t entities[MAX_GENTITIES];
	byte		entityData[MAX_GENTITIES];
	int			commandUsed;
	char		commandData[2048*MAX_CLIENTS];
	byte		areaUsed;
	byte		areamask[MAX_MAP_AREA_BYTES];
	demoString_t string;
} demoFrame_t;

typedef struct {
	demoFrame_t		frames[DEMOCONVERTFRAMES];
	int				frameIndex;
	clSnapshot_t	snapshots[PACKET_BACKUP];
	entityState_t	entityBaselines[MAX_GENTITIES];
	entityState_t	parseEntities[MAX_PARSE_ENTITIES];
	int				messageNum, lastMessageNum;
} demoConvert_t;

#define FRAME_BUF_SIZE 200
typedef struct {
	fileHandle_t		fileHandle;
	char				fileName[MAX_QPATH];
	int					fileSize, filePos;
	int					totalFrames;
	int					startTime, endTime;			//serverTime from first snapshot 

	qboolean			lastFrame;

	int					frameNumber;

	char				commandData[256*1024];
	int					commandFree;
	int					commandStart[DEMO_PLAY_CMDS];
	int					commandCount;
	int					clientNum;
	demoFrame_t			storageFrame[FRAME_BUF_SIZE];
	demoFrame_t			*frame, *nextFrame;
	qboolean			nextValid;
	struct	{
		int				pos, time, frame;
	} fileIndex[DEMO_MAX_INDEX];
	int					fileIndexCount;
} demoPlay_t;

typedef struct {
	int						nextNum, currentNum;
	struct {
		demoPlay_t			*handle;
		int					snapCount;
		int					messageCount;
		int					oldDelay;
		int					oldTime;
		int					oldFrameNumber;
		int					serverTime;
	} play;
	qboolean				del;
	struct {
		clientConnection_t	Clc;
		clientActive_t		Cl;
	} cut;
} demo_t;

extern demo_t demo;