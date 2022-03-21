// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "bg_demos.h"
#include "cg_local.h"
#include "cg_demos_math.h"

#define LOGLINES 8
#define MAX_DEMO_COMMAND_LENGTH 1024
#define MAX_DEMO_COMMAND_LAYERS 10
#define MAX_DEMO_COMMAND_VARIABLE_LENGTH 256
#define MAX_DEMO_COMMAND_VARIABLES 10
#define MAX_DEMO_OBJECT_PARAM_LENGTH 1024

typedef enum {
	editNone,
	editCamera,
	editChase,
	editLine,
	editDof,
	editLast,
	editCommands,
	editObjects
} demoEditType_t;

typedef enum {
	viewCamera,
	viewChase,
	viewLast
} demoViewType_t;

typedef enum {
	findNone,
	findObituary,
	findDirect,
} demofindType_t;

typedef struct demoLinePoint_s {
	struct			demoLinePoint_s *next, *prev;
	int				time, demoTime;
} demoLinePoint_t;

typedef enum {
	DEMO_COMMAND_VARIABLE_VALUE,
	DEMO_COMMAND_VARIABLE_WAVEFORM
} demoCommandVariableType_t;

typedef char demoCommandVariableRaw_t[MAX_DEMO_COMMAND_VARIABLE_LENGTH];
typedef demoCommandVariableRaw_t demoCommandVariableRawCollection_t[MAX_DEMO_COMMAND_VARIABLES];

typedef struct demoCommandVariable_s {
	demoCommandVariableRaw_t	raw;
	demoCommandVariableType_t	type;
	qboolean					interpolate;
	qboolean					isValid;
	waveForm_t					waveForm;
	float						value;
} demoCommandVariable_t;

typedef struct demoCommandPoint_s {
	char					command[MAX_DEMO_COMMAND_LENGTH];
	demoCommandVariable_t	variables[MAX_DEMO_COMMAND_VARIABLES]; // 0-9
	struct					demoCommandPoint_s *next, *prev;
	int						time;
	int						layer;
} demoCommandPoint_t;

typedef struct demoObject_s {
	char					param1[MAX_DEMO_OBJECT_PARAM_LENGTH]; // For polys: Shader name. For models (not implemented), this could be model name.
	float					size1; // Width for polys. For models (not implemented), this could be scaling factor.
	float					size2; // Height
	vec3_t					angles;
	vec3_t					origin;
	vec4_t					modulate; // 0...1 0...1 0...1 0...1 RGBA
	vec3_t					axes[3]; // Will be quicker to compute polyvert coordinats with. Will be generated from angles.
	polyVert_t				verts[4]; // Since this doesn't ever change, we can calculate it once and reuse.
	qhandle_t				shader; // So we don't have to look it up every time.
	struct					demoObject_s *next, *prev;
	int						timeIn;		// At what time does this start? demotime.
	int						timeOut;	// At what time does this end? 0 = never, >0 = demotime.
} demoObject_t;

typedef struct demoCameraPoint_s {
	struct			demoCameraPoint_s *next, *prev;
	vec3_t			origin, angles;
	float			fov;
	int				time, flags;
	float            len, anglesLen, step, curLen;
} demoCameraPoint_t;

typedef struct demoChasePoint_s {
	struct			demoChasePoint_s *next, *prev;
	vec_t			distance;
	vec3_t			angles;
	int				target;
	vec3_t			origin;
	int				time;
	float			len;
} demoChasePoint_t;

typedef struct demoDofPoint_s {
	struct			demoDofPoint_s *next, *prev;
	float			focus, radius;
	int				time;
} demoDofPoint_t;

typedef struct {
	char lines[LOGLINES][1024];
	int	 times[LOGLINES];
	int	 lastline;
} demoLog_t;

typedef struct demoMain_s {
	int				serverTime;
	float			serverDeltaTime;
	struct {
		int			start, end;
		qboolean	locked;
		float		speed;
		int			offset;
		float		timeShift;
		int			shiftWarn;
		int			time;
		demoLinePoint_t *points;
	} line;
	struct {
		int			start, end;
		qboolean	locked;
		float		timeShift;
		int			shiftWarn;
		demoCommandPoint_t *points;

		// Used to determine if a new command must be executed or not:
		demoCommandPoint_t *lastPoint[MAX_DEMO_COMMAND_LAYERS];
		float lastValue[MAX_DEMO_COMMAND_VARIABLES];
	} commands;
	struct {
		qboolean	locked;
		demoObject_t* objects;
	} objects;
	struct {
		int			start;
		float		range;
		int			index, total;
		int			lineDelay;
	} loop;
	struct {
		int			start, end;
		qboolean	locked;
		int			target;
		vec3_t		angles, origin, velocity;
		vec_t		distance;
		float		timeShift;
		int			shiftWarn;
		demoChasePoint_t *points;
		centity_t	*cent;
	} chase;
	struct {
		int			start, end;
		int			target, flags;
		int			shiftWarn;
		float		timeShift;
		float		fov;
		qboolean	locked;
		vec3_t		angles, origin, velocity;
		demoCameraPoint_t *points;
		posInterpolate_t	smoothPos;
		angleInterpolate_t	smoothAngles;
	} camera;
	struct {
		int			start, end;
		int			target;
		int			shiftWarn;
		float		timeShift;
		float		focus, radius;
		qboolean	locked;
		demoDofPoint_t *points;
	} dof;
	struct {
		int			time;
		int			oldTime;
		int			lastTime;
		float		fraction;
		float		speed;
		qboolean	paused;
	} play;
	struct {
		float speed, acceleration, friction;
	} move;
	struct {
		vec3_t		angles;
		float		size, precision;
		qboolean	active;
	} sun;
	struct {
		int			time, number;
		float		range;
		qboolean	active, back;
	} rain;
	struct {
		int			start, end;
	} cut;
	vec3_t			viewOrigin, viewAngles;
	demoViewType_t	viewType;
	vec_t			viewFov;
	int				viewTarget;
	float			viewFocus, viewFocusOld, viewRadius;
	demoEditType_t	editType;

	vec3_t		cmdDeltaAngles;
	usercmd_t	cmd, oldcmd;
	int			nextForwardTime, nextRightTime, nextUpTime;
	int			deltaForward, deltaRight, deltaUp;
	struct	{
		int		start, end;
		qboolean active, locked;
	} capture;
	struct {
		qhandle_t additiveWhiteShader;
		qhandle_t mouseCursor;
		qhandle_t switchOn, switchOff;
		sfxHandle_t heavyRain, regularRain, lightRain;
	} media;
	demofindType_t find;
	qboolean	seekEnabled;
	qboolean	initDone;
	qboolean	autoLoad;
	demoLog_t	log;
} demoMain_t;

extern demoMain_t demo;

void demoPlaybackInit(void);
centity_t *demoTargetEntity( int num );
int demoHitEntities( const vec3_t start, const vec3_t forward );
qboolean demoCentityBoxSize( const centity_t *cent, vec3_t container );
void demoDrawCrosshair( void );

void CG_DemosAddLog(const char *fmt, ...);

//CAMERA
void cameraMove( void );
void cameraMoveDirect( void );
void cameraUpdate( int time, float timeFraction );
void cameraDraw( int time, float timeFraction );
void cameraPointReset( demoCameraPoint_t *point );
void demoCameraCommand_f(void);
void cameraSave( fileHandle_t fileHandle );
qboolean cameraParse( BG_XMLParse_t *parse, const struct BG_XMLParseBlock_s *fromBlock, void *data);
demoCameraPoint_t *cameraPointSynch( int time );

//MOVE
void demoMovePoint( vec3_t origin, vec3_t velocity, vec3_t angles);
void demoMoveDirection( vec3_t origin, vec3_t angles );
void demoMoveUpdateAngles( void );
void demoMoveDeltaCmd( void );

//CHASE
void demoMoveChase( void );
void demoMoveChaseDirect( void );
void demoChaseCommand_f( void );
void chaseUpdate( int time, float timeFraction );
void chaseDraw( int time, float timeFraction );
void chaseEntityOrigin( centity_t *cent, vec3_t origin );
void chaseEntityOrigin( centity_t *cent, vec3_t origin );
void chaseSave( fileHandle_t fileHandle );
qboolean chaseParse( BG_XMLParse_t *parse, const struct BG_XMLParseBlock_s *fromBlock, void *data);
demoChasePoint_t *chasePointSynch(int time );

//LINE
void demoMoveLine( void );
void demoLineCommand_f(void);
void lineAt(int playTime, float playFraction, int *demoTime, float *demoFraction, float *demoSpeed );
void lineSave( fileHandle_t fileHandle );
qboolean lineParse( BG_XMLParse_t *parse, const struct BG_XMLParseBlock_s *fromBlock, void *data);
demoLinePoint_t *linePointSynch(int playTime);

//COMMANDS
void evaluateDemoCommand();
void demoCommandsCommand_f(void);
void commandsSave(fileHandle_t fileHandle);
qboolean commandsParse(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data);
demoCommandPoint_t* commandPointSynch(int playTime);

// Objects
void objectsSave(fileHandle_t fileHandle);
qboolean objectsParse(BG_XMLParse_t* parse, const struct BG_XMLParseBlock_s* fromBlock, void* data);
void drawDemoObjects(qboolean drawHUD);
void demoObjectsCommand_f(void);
demoObject_t* closestObject(vec3_t origin);

//DOF
demoDofPoint_t *dofPointSynch( int time );
void dofMove(void);
void dofUpdate( int time, float timeFraction );
void dofDraw( int time, float timeFraction );
qboolean dofParse( BG_XMLParse_t *parse, const struct BG_XMLParseBlock_s *fromBlock, void *data);
void dofSave( fileHandle_t fileHandle );
void demoDofCommand_f(void);

//HUD
void hudInitTables(void);
void hudToggleInput(void);
void hudDraw(void);
const char *demoTimeString( int time );

//CAPTURE
void demoCaptureCommand_f( void );
void demoLoadCommand_f( void );
void demoSaveCommand_f( void );
void demoSaveLine( fileHandle_t fileHandle, const char *fmt, ...);
qboolean demoProjectLoad( const char *fileName );

//DISMEMBERMENT
void demoSaberDismember(centity_t *cent, vec3_t dir);
void demoCheckDismember(vec3_t saberhitorg);
void demoPlayerDismember(centity_t *cent);

//WEATHER
void demoDrawRain(void);
void demoDrawSun(void);
qboolean weatherParse( BG_XMLParse_t *parse, const struct BG_XMLParseBlock_s *fromBlock, void *data);
void weatherSave( fileHandle_t fileHandle );
void demoSunCommand_f(void);
void demoRainCommand_f(void);

void demoCutCommand_f(void);

#define CAM_ORIGIN	0x001
#define CAM_ANGLES	0x002
#define CAM_FOV		0x004
#define CAM_TIME	0x100
