// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "cg_demos.h" 
//#include "../../ui/menudef.h"
#include "../ui/keycodes.h"

#define MAX_HUD_ITEMS	128

#define HUD_TEXT_WIDTH 10
#define HUD_TEXT_HEIGHT 13
#define HUD_TEXT_SPACING 13
#define HUD_FLOAT "%.3f"

typedef enum {
	hudTypeNone,
	hudTypeHandler,
	hudTypeValue,
	hudTypeFloat,
	hudTypeButton,
	hudTypeCvar,
	hudTypeText,
	hudTypeCheck
} hudType_t;

typedef struct {
	float		x,y;
	hudType_t	type;
	const		char *text, *cvar;
	int			textLen;
	float		*value;
	int			*time;
	int			handler;
	int			showMask;
} hudItem_t;

static hudItem_t hudItems [MAX_HUD_ITEMS];
static int hudItemsUsed;

typedef enum {
	hudPlayTime,
	//hudServerTime,
	hudEditName,
	hudViewName,
	hudDemoName,
	hudCGTime,

	hudCommandLayerHere,
	hudCommandHere,
	hudCommandVariablesHere,

	hudCamCheckPos,
	hudCamCheckAngles,
	hudCamCheckFov,
	hudCamLength,
	hudCamSpeed,
	hudCamSmoothPos,
	hudCamSmoothAngles,
	hudCamPosX, hudCamPosY, hudCamPosZ,
	hudCamPitch, hudCamYaw, hudCamRoll,
	hudCamFov,
	hudCamTarget,

	hudChaseDistance,
	hudChaseTarget,
	hudChasePosX, hudChasePosY, hudChasePosZ,
	hudChasePitch, hudChaseYaw, hudChaseRoll,
	hudChaseFov,

	hudLineTime, 
	hudLineOffset,
	hudLineSpeed,
	hudLineStart,
	hudLineEnd,
	
	hudDofFocus,
	hudDofRadius,

	hudLogBase
} hudHandler_t;

#define MASK_CAM				0x00010
#define MASK_LINE				0x00020
#define MASK_CHASE				0x00040
#define MASK_DOF				0x00400
#define MASK_CMDS				0x00800

#define MASK_HUD				0x1
#define MASK_POINT				0x2
#define MASK_EDIT				0x4
#define MASK_ACTIVE				0x8

#define MASK_CAM_POINT			( MASK_CAM | MASK_POINT )
#define MASK_CAM_HUD			( MASK_CAM | MASK_HUD )
#define MASK_CAM_EDIT			( MASK_CAM | MASK_EDIT )
#define MASK_CAM_EDITHUD		( MASK_CAM | MASK_EDIT | MASK_HUD )

#define MASK_CHASE_EDIT			( MASK_CHASE | MASK_EDIT )

#define MASK_LINE_HUD			( MASK_LINE | MASK_HUD )

#define MASK_DOF_EDIT			( MASK_DOF | MASK_EDIT )

static struct {
	float cursorX, cursorY;
	int showMask;
	int keyCatcher;
	struct {
		int  cursor;
		hudItem_t *item;
		char line[512];
	} edit;
	struct {
		float *origin, *angles, *fov;
		int *flags;
		demoCameraPoint_t *point;
	} cam;
	struct {
		float *origin, *angles, *distance;
		int *flags;
		demoChasePoint_t *point;
	} chase;
	struct {
		float *focus, *radius;
		demoDofPoint_t *point;
	} dof;
	demoLinePoint_t *linePoint;
	demoCommandPoint_t *commandPoint;
	const char	*logLines[LOGLINES];
} hud;

static void hudDrawText( float x, float y, const char *buf, const vec4_t color) {
	if (!buf)
		return;
	CG_DrawStringExt( x, y, buf, color, qfalse, qtrue, HUD_TEXT_WIDTH*cgs.widthRatioCoef, HUD_TEXT_HEIGHT , -1 );
//	CG_Text_Paint( x, y, 0.3f, color, buf, 0,0,ITEM_TEXTSTYLE_SHADOWED );
}

static void hudMakeTarget( int targetNum, char *dst, int dstSize) {
	centity_t *cent = demoTargetEntity( targetNum );
	if (cent) {
		const char *type;
		switch (cent->currentState.eType) {
		case ET_PLAYER:
			type = cgs.clientinfo[cent->currentState.number].name;
			break;
		case ET_MISSILE:
			type = "missile";
			break;
		case ET_ITEM:
			type = "item";
			break;
		case ET_MOVER:
			type = "mover";
			break;
		case ET_SPECIAL:
			type = "special";
			break;
		case ET_HOLOCRON:
			type = "holocron";
			break;
		case ET_GENERAL:
			type = "general";
			break;
		default:
			type = "other";
			break;
		}
		Com_sprintf(dst, dstSize, "%d %s", targetNum, type );
	} else {
		Com_sprintf(dst, dstSize, "%d invalid", targetNum );
	}
}

const char *demoTimeString( int time ) {
	static char retBuf[32];
	int msec = time % 1000;
	int secs = (time / 1000);
	int mins = (secs / 60);
	secs %= 60;
	Com_sprintf( retBuf, sizeof(retBuf), "%d:%02d.%03d", mins, secs, msec  );
	return retBuf;
}

static int hudGetChecked( hudItem_t *item, vec4_t color ) {
	Vector4Copy( colorWhite, color );
	switch ( item->handler ) {
	case hudCamCheckPos:
		return (hud.cam.flags[0] & CAM_ORIGIN);
	case hudCamCheckAngles:
		return (hud.cam.flags[0] & CAM_ANGLES);
	case hudCamCheckFov:
		return (hud.cam.flags[0] & CAM_FOV);
	}
	return 0;
}

static void hudToggleButton( hudItem_t *item, int change ) {
	demoCameraPoint_t *point;

	if ( change ) {
		switch ( item->handler ) {
		case hudCamSmoothAngles:
			switch ( demo.camera.smoothAngles ) {
			case angleLinear:
				demo.camera.smoothAngles = angleQuat;
				return;
			case angleQuat:
				demo.camera.smoothAngles = angleLinear;
				return;
			}
			return;
		case hudCamSmoothPos:
			switch ( demo.camera.smoothPos ) {
			case posLinear:
				demo.camera.smoothPos = posCatmullRom;
				return;
			case posCatmullRom:
				demo.camera.smoothPos = posBezier;
				return;
			case posBezier:
				demo.camera.smoothPos = posLinear;
				return;
			}
			point = demo.camera.points;
			while (point) {
				cameraPointReset( point );
				point = point->next;
			}
			return;
		}
	}
}

static void hudToggleChecked( hudItem_t *item ) {
	switch ( item->handler ) {
	case hudCamCheckPos:
		hud.cam.flags[0] ^= CAM_ORIGIN;
		break;
	case hudCamCheckAngles:
		hud.cam.flags[0] ^= CAM_ANGLES;
		break;
	case hudCamCheckFov:
		hud.cam.flags[0] ^= CAM_FOV;
		break;
	}
}

static float *hudGetFloat( hudItem_t *item ) {
	switch ( item->handler ) {
	case hudCamFov:
		return hud.cam.fov;
	case hudCamPosX:
	case hudCamPosY:
	case hudCamPosZ:
		return hud.cam.origin + item->handler - hudCamPosX;
	case hudCamPitch:
	case hudCamYaw:
	case hudCamRoll:
		return hud.cam.angles + item->handler - hudCamPitch;
	case hudChasePosX:
	case hudChasePosY:
	case hudChasePosZ:
		return hud.chase.origin + item->handler - hudChasePosX;
	case hudChasePitch:
	case hudChaseYaw:
	case hudChaseRoll:
		return hud.chase.angles + item->handler - hudChasePitch;
	case hudChaseDistance:
		return hud.chase.distance;
	case hudDofFocus:
		return hud.dof.focus;
	case hudDofRadius:
		return hud.dof.radius;
	default:
		break;
	}
	return 0;
}

static void hudGetHandler( hudItem_t *item, char *buf, int bufSize ) {
	char tmp[32];
	char tmp2[32];
	int i, highestSlashIndex,count;
	char* onlyFilenameTmp;

	buf[0] = 0;
	if (item->handler >= hudLogBase && item->handler < hudLogBase + LOGLINES) {
		const char *l = hud.logLines[item->handler - hudLogBase];
		if (!l)
			return;
		Q_strncpyz( buf, l, bufSize );
		return;
    }
	switch ( item->handler ) {
	case hudPlayTime:

		sprintf_s(tmp, sizeof(tmp), "%s", demoTimeString(cg.snap->serverTime));
		sprintf_s(tmp2, sizeof(tmp2), "%s", demoTimeString(cg.time - cgs.levelStartTime));
		Com_sprintf( buf, bufSize, "%s (%s/%s)", demoTimeString(demo.play.time ), tmp,tmp2);
		return;
	case hudDemoName:
		onlyFilenameTmp = mme_demoFileName.string;
		i = 0;
		highestSlashIndex = 0;
		while (onlyFilenameTmp[i] != '\0') {
			if (onlyFilenameTmp[i] == '\\' || onlyFilenameTmp[i] == '/') {
				highestSlashIndex = i;
			}
			i++;
		}
		if (highestSlashIndex > 0 && highestSlashIndex+1 < i) {
			onlyFilenameTmp += highestSlashIndex+1;
		}
		Com_sprintf( buf, bufSize, "%s", onlyFilenameTmp);
		return;
	case hudCGTime:
		Com_sprintf( buf, bufSize, "%d (demotime %d)", cg.time, demo.play.time);
		return;
	case hudEditName:
		switch (demo.editType) {
		case editCamera:
			Com_sprintf( buf, bufSize, "Camera%s", demo.camera.locked ? " locked" : "" );
			break;
		case editChase:
			Com_sprintf( buf, bufSize, "Chase%s", demo.chase.locked ? " locked" : "" );
			break;
		case editLine:
			Com_sprintf( buf, bufSize, "Line%s", demo.line.locked ? " locked" : "" );
			break;
		case editDof:
			Com_sprintf( buf, bufSize, "Dof%s", demo.dof.locked ? " locked" : "" );
			break;
		case editCommands:
			Com_sprintf( buf, bufSize, "Commands%s", demo.commands.locked ? " locked" : "" );
			break;
		}
		return;
	case hudViewName:
		switch (demo.viewType) {
		case viewCamera:
			Com_sprintf( buf, bufSize, "Camera");
			break;
		case viewChase:
			Com_sprintf( buf, bufSize, "Chase");
			break;
		default:
			return;
		}
		return;
	case hudCamLength:
		Com_sprintf( buf, bufSize, "%.01f", hud.cam.point->len );
		return;
	case hudCommandHere:
		if (hud.commandPoint) {
			Com_sprintf(buf, bufSize, "%s", hud.commandPoint->command);
		}
		return;
	case hudCommandVariablesHere:
		if (hud.commandPoint) {
			buf[0] = 0;
			count = 0;
			for (i = 0; i < MAX_DEMO_COMMAND_VARIABLES; i++) {
				if (hud.commandPoint->variables[i].isValid) {
					char tmpVar[MAX_DEMO_COMMAND_VARIABLE_LENGTH + 5];
					if (count++ == 0) {
						Com_sprintf(tmpVar, sizeof(tmpVar), "%d:%s", i, hud.commandPoint->variables[i].raw);
					}
					else {
						Com_sprintf(tmpVar, sizeof(tmpVar), " | %d:%s", i, hud.commandPoint->variables[i].raw);
					}
					strcat_s(buf, bufSize, tmpVar);
				}
			}
		}
		return;
	case hudCommandLayerHere:
		if (hud.commandPoint) {
			Com_sprintf(buf, bufSize, "%d", hud.commandPoint->layer);
		}
		return;
	case hudCamSpeed:
		if (!hud.cam.point->prev)
			Q_strncpyz( buf, "First, ", bufSize );
		else
			Com_sprintf( buf, bufSize, "%.01f, ", 1000 * hud.cam.point->prev->len / (hud.cam.point->time - hud.cam.point->prev->time));
		bufSize -= strlen( buf );
		buf += strlen( buf );
		if (!hud.cam.point->next)
			Q_strncpyz( buf, "Last", bufSize );
		else 
			Com_sprintf( buf, bufSize, "%.01f", 1000 * hud.cam.point->len / (hud.cam.point->next->time - hud.cam.point->time));
		return;
	case hudCamSmoothPos:
		switch ( demo.camera.smoothPos ) {
		case posLinear:
			Q_strncpyz( buf, "Linear", bufSize );
			break;
		case posCatmullRom:
			Q_strncpyz( buf, "CatmullRom", bufSize );
			break;
		default:
		case posBezier:
			Q_strncpyz( buf, "Bezier", bufSize );
			break;

		}
		return;
	case hudCamSmoothAngles:
		switch ( demo.camera.smoothAngles ) {
		case angleLinear:
			Q_strncpyz( buf, "Linear", bufSize );
			break;
		default:
		case angleQuat:
			Q_strncpyz( buf, "Quat", bufSize );
			break;
		}
		return;
	case hudCamTarget:
		hudMakeTarget( demo.camera.target, buf, bufSize );
		return;
	case hudChaseTarget:
		hudMakeTarget( demo.chase.target, buf, bufSize );
		return;
	case hudLineTime:
		Com_sprintf( buf, bufSize, "%s", demoTimeString( demo.line.time ));
		return;
	case hudLineOffset:
		Com_sprintf( buf, bufSize, "%d.%03d", demo.line.offset / 1000, demo.line.offset % 1000 );
		return;
	case hudLineSpeed:
		if (demo.line.locked && hud.linePoint) {
			demoLinePoint_t *point = hud.linePoint;
			if (!point->prev) {
				Com_sprintf( buf, bufSize, "First" );
			} else {
				Com_sprintf( buf, bufSize, HUD_FLOAT, (point->demoTime - point->prev->demoTime) / (float)(point->time - point->prev->time));
			}
		} else {
			Com_sprintf( buf, bufSize, HUD_FLOAT, demo.line.speed );
		}
		return;
	case hudLineStart:
		if ( demo.capture.start > 0 )
			Com_sprintf( buf, bufSize, "%s", demoTimeString( demo.capture.start ));
		else
			Com_sprintf( buf, bufSize, "None" );
		return;
	case hudLineEnd:
		if (demo.capture.end > 0 )
			Com_sprintf( buf, bufSize, "%s", demoTimeString( demo.capture.end ));
		else 
			Com_sprintf( buf, bufSize, "None" );
		return;
	}
}

static float hudItemWidth( hudItem_t *item  ) {
	char buf[512];
	float w, *f;

	w = item->textLen * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
	switch (item->type ) {
	case hudTypeHandler:
	case hudTypeButton:
		hudGetHandler( item, buf, sizeof(buf) );
		w += strlen( buf ) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		break;
/*	case hudTypeText:
		hudGetText( item, buf, sizeof(buf), qfalse );
		w += strlen( buf ) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		break;
*/	case hudTypeValue:
		Com_sprintf( buf, sizeof( buf ), HUD_FLOAT, item->value[0]);
		w += strlen( buf ) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		break;
	case hudTypeFloat:
		f = hudGetFloat( item );
		if (!f)
			break;
		Com_sprintf( buf, sizeof( buf ), HUD_FLOAT, f[0] );
		w += strlen( buf ) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		break;
	case hudTypeCheck:
		w += HUD_TEXT_SPACING*cgs.widthRatioCoef;
		break;
	case hudTypeCvar:
		trap_Cvar_VariableStringBuffer( item->cvar, buf, sizeof( buf ));
		w += strlen( buf ) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		break;
	}
	return w;
}

static void hudDrawItem( hudItem_t *item ) {
	char buf[512];
	int checked;
	float x,y, *f;

	x = (item->x/HUD_TEXT_WIDTH)*(int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
	y = item->y;

	if ( item == hud.edit.item ) {
		if ( item->textLen ) {
			hudDrawText( x, y, item->text, colorRed );
			x += item->textLen * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		}
		hudDrawText( x, y, hud.edit.line, colorRed );
		if ( demo.serverTime & 512 ) {
			float x = ((item->x/HUD_TEXT_WIDTH)*(int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef) + (item->textLen + hud.edit.cursor) * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef));
			float y = item->y;
			if ( trap_Key_GetOverstrikeMode()) {
				CG_FillRect( x, y + HUD_TEXT_SPACING - 3 , HUD_TEXT_WIDTH*cgs.widthRatioCoef, 3, colorRed );
			} else {
				CG_FillRect( x, y , HUD_TEXT_WIDTH*cgs.widthRatioCoef, HUD_TEXT_SPACING, colorRed );
			}
		}
		return;
	}
	if ( item->textLen ) {
		float *color = colorWhite;
		if ( hud.keyCatcher & KEYCATCH_CGAME ) {
			switch ( item->type ) {
			case hudTypeFloat:
			case hudTypeValue:
			case hudTypeCvar:
			case hudTypeText:
				color = colorYellow;
				break;
			case hudTypeCheck:
			case hudTypeButton:
				color = colorGreen;
				break;
			}
		} 
		hudDrawText( (item->x/HUD_TEXT_WIDTH)*(int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef), item->y, item->text, color );
		x += item->textLen * (int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
	}
	switch (item->type ) {
	case hudTypeButton:
	case hudTypeHandler:
		hudGetHandler( item, buf, sizeof( buf ));
		hudDrawText( x, y, buf, colorWhite );
		break;
/*	case hudTypeText:
		hudGetText( item, buf, sizeof(buf), qfalse );
		hudDrawText( x, y, buf, colorWhite );
		break;
*/	case hudTypeValue:
		Com_sprintf( buf, sizeof( buf ), HUD_FLOAT, item->value[0] );
		hudDrawText( x, y, buf, colorWhite );
		break;
	case hudTypeFloat:
		f = hudGetFloat( item );
		if (!f)
			break;
		Com_sprintf( buf, sizeof( buf ), HUD_FLOAT, f[0] );
		hudDrawText( x, y, buf, colorWhite );
		break;
	case hudTypeCvar:
		trap_Cvar_VariableStringBuffer( item->cvar, buf, sizeof( buf ));
		hudDrawText( x, y, buf, colorWhite );
		break;
	case hudTypeCheck:
		checked = hudGetChecked( item, colorWhite );
		CG_DrawPic( (x + (int)(5*cgs.widthRatioCoef)), y + 2, HUD_TEXT_SPACING*cgs.widthRatioCoef, HUD_TEXT_SPACING, checked ? 
			demo.media.switchOn : demo.media.switchOff ); 
		break;
	}
}

void hudDraw( void ) {
	int i;int logIndex;
	hudItem_t *item;

	if (demo.editType == editNone)
		return;

	switch (demo.editType) {
	case editCamera:
		hud.showMask = MASK_CAM;
		if ( demo.camera.locked ) {
			hud.cam.point = cameraPointSynch(  demo.play.time );
			if (!hud.cam.point || hud.cam.point->time != demo.play.time || demo.play.fraction) {
				hud.cam.point = 0;
			} else {
				hud.cam.angles = hud.cam.point->angles;
				hud.cam.origin = hud.cam.point->origin;
				hud.cam.fov = &hud.cam.point->fov;
				hud.cam.flags = &hud.cam.point->flags;
				hud.showMask |= MASK_EDIT | MASK_POINT;
			}
		} else {
			hud.cam.angles = demo.camera.angles;
			hud.cam.origin = demo.camera.origin;
			hud.cam.fov = &demo.camera.fov;
			hud.cam.flags = &demo.camera.flags;
			hud.cam.point = 0;
			hud.showMask |= MASK_EDIT;
		}
		break;
	case editLine:
		hud.showMask = MASK_LINE;
		break;
	case editCommands:
		hud.showMask = 0;
		if (demo.commands.locked) {
			hud.commandPoint = commandPointSynch(demo.play.time);
			if (!hud.commandPoint || hud.commandPoint->time != demo.play.time || demo.play.fraction) {
				hud.commandPoint = 0;
			}
			else {
				hud.showMask |= MASK_CMDS;
			}
		}
		else {
			hud.commandPoint = 0;
		}
		break;
	case editChase:
		hud.showMask = MASK_CHASE;
		if ( demo.chase.locked ) {
			hud.chase.point = chasePointSynch(  demo.play.time );
			if (!hud.chase.point || hud.chase.point->time != demo.play.time || demo.play.fraction) {
				hud.chase.point = 0;
			} else {
				hud.showMask |= MASK_EDIT | MASK_POINT;
				hud.chase.angles = hud.chase.point->angles;
				hud.chase.origin = hud.chase.point->origin;
				hud.chase.distance = &hud.chase.point->distance;
			}
		} else {
			hud.chase.angles = demo.chase.angles;
			hud.chase.origin = demo.chase.origin;
			hud.chase.distance = &demo.chase.distance;
			hud.chase.point = 0;
			hud.showMask |= MASK_EDIT;
		}
		break;
	case editDof:
		hud.showMask = MASK_DOF;
		if ( demo.dof.locked ) {
			hud.dof.point = dofPointSynch(  demo.play.time );
			if (!hud.dof.point || hud.dof.point->time != demo.play.time || demo.play.fraction) {
				hud.dof.point = 0;
			} else {
				hud.dof.focus = &hud.dof.point->focus;
				hud.dof.radius = &hud.dof.point->radius;
				hud.showMask |= MASK_EDIT | MASK_POINT;
			}
		} else {
			hud.dof.focus = &demo.dof.focus;
			hud.dof.radius = &demo.dof.radius;
			hud.dof.point = 0;
			hud.showMask |= MASK_EDIT;
		}
		break;
	default:
		hud.showMask = 0;
		break;
	}	
	hud.keyCatcher = trap_Key_GetCatcher();
	if ( hud.keyCatcher & KEYCATCH_CGAME ) {
		hud.showMask |= MASK_HUD;
	}
	/* Prepare the camera control information */

	hud.linePoint = linePointSynch( demo.play.time );
	logIndex = LOGLINES - 1;
	for (i=0; i < LOGLINES;	i++) {
		int which = (demo.log.lastline + LOGLINES - i) % LOGLINES;
		hud.logLines[LOGLINES - 1 - i] = 0;
		if (demo.log.times[which] + 3000 > demo.serverTime)
			hud.logLines[logIndex--] = demo.log.lines[which];
	}
	// Check if the selected edit item is still valid
	if ( hud.edit.item ) {
		if ( !(hud.keyCatcher & KEYCATCH_CGAME ))
			hud.edit.item = 0;
		else if ( (hud.edit.item->showMask & hud.showMask) != hud.edit.item->showMask )
			hud.edit.item = 0;
	}
	for (i=0; i < hudItemsUsed; i++) {
		item = &hudItems[i];
		if ((hud.showMask & item->showMask) != item->showMask )
			continue;
		hudDrawItem( item );
	}

	if ( hud.keyCatcher & KEYCATCH_CGAME ) {
		float x,y,w,h;
		x = hud.cursorX;
		y = hud.cursorY;
		w = 32;
		h = 32;
		trap_R_DrawStretchPic( x,y,w*cgs.widthRatioCoef,h, 0,0,1,1, demo.media.mouseCursor );
	} else {
		hud.edit.item = 0;
	}
}

static hudItem_t *hudAddItem( float x, float y, int showMask, const char *text ) {
	hudItem_t *item;
	if (hudItemsUsed >= MAX_HUD_ITEMS ) 
		CG_Error( "Demo too many hud items" );
	item = &hudItems[hudItemsUsed];
	item->x = x * (HUD_TEXT_WIDTH);
	item->y = y * HUD_TEXT_SPACING + 50;
	item->type = hudTypeNone;
	item->showMask = showMask;
	item->handler = 0;
	item->text = text;
	if (item->text )
		item->textLen = strlen( text );
	else
		item->textLen = 0;
	hudItemsUsed++;
	return item;
}

static void hudAddHandler( float x, float y, int showMask, const char *text, int handler ) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->handler = handler;
	item->type = hudTypeHandler;
}

static void hudAddValue( float x, float y, int showMask, const char *text, float *value) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->value = value;
	item->type = hudTypeValue;
}
static void hudAddFloat( float x, float y, int showMask, const char *text, int handler ) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->handler = handler;
	item->type = hudTypeFloat;
}

static void hudAddCheck( float x, float y, int showMask, const char *text, int handler ) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->handler = handler;
	item->type = hudTypeCheck;
}
static void hudAddCvar( float x, float y, int showMask, const char *text, const char *cvar ) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->cvar = cvar;
	item->type = hudTypeCvar;
}
static void hudAddButton( float x, float y, int showMask, const char *text, int handler ) {
	hudItem_t *item = hudAddItem( x, y, showMask, text );
	item->handler = handler;
	item->type = hudTypeButton;
}

void hudToggleInput(void) {
	int oldCatcher = trap_Key_GetCatcher();
	if ( oldCatcher & KEYCATCH_CGAME ) {
		oldCatcher &= ~(KEYCATCH_CGAME | KEYCATCH_CGAMEEXEC );
	} else {
		oldCatcher |= (KEYCATCH_CGAME | KEYCATCH_CGAMEEXEC );
	}
	trap_Key_SetCatcher( oldCatcher );
}

void hudInitTables(void) {
	int i;
	memset( hudItems, 0, sizeof( hudItems ));
	hudItemsUsed = 0;

	/* Setup the hudItems */
	hudAddHandler(   0,  0,  0, "Time:", hudPlayTime );
	//hudAddHandler(   0,  0,  0, "Servertime:", hudServerTime );
	hudAddValue(     0,  1,  0, "Speed:", &demo.play.speed );
	hudAddHandler(   0,  2,  0, "View:", hudViewName );
	hudAddHandler(   0,  3,  0, "Edit:", hudEditName );
	hudAddHandler(   0,  22,  0, "Demoname:", hudDemoName );
	hudAddHandler(   0,  23,  0, "CG.Time:", hudCGTime);

	for (i = 0; i < LOGLINES; i++) 
		hudAddHandler(   0,  25+i, 0, 0, hudLogBase+i );

	// Command items
	hudAddHandler(0, 4, MASK_CMDS, "Layer:", hudCommandLayerHere);
	hudAddHandler(0, 5, MASK_CMDS, "Command:", hudCommandHere);
	hudAddHandler(0, 6, MASK_CMDS, "Variables:", hudCommandVariablesHere);

	// Camera Items
	hudAddFloat(   0,  4, MASK_CAM_EDIT, "PosX:",  hudCamPosX );
	hudAddFloat(   0,  5, MASK_CAM_EDIT, "PosY:",  hudCamPosY );
	hudAddFloat(   0,  6, MASK_CAM_EDIT, "PosZ:",  hudCamPosZ );
	hudAddFloat(   0,  7, MASK_CAM_EDIT, "Pitc:",  hudCamPitch );
	hudAddFloat(   0,  8, MASK_CAM_EDIT, "Yaw :",  hudCamYaw );
	hudAddFloat(   0,  9, MASK_CAM_EDIT, "Roll:",  hudCamRoll );
	hudAddFloat(   0, 10, MASK_CAM_EDIT, "Fov :",  hudCamFov );
	hudAddHandler( 0, 11, MASK_CAM_EDIT, "Target:", hudCamTarget );
	hudAddHandler( 0, 12, MASK_CAM_POINT, "Length:", hudCamLength );
	hudAddHandler( 0, 13, MASK_CAM_POINT, "Speed:", hudCamSpeed );
	hudAddCheck(  0,  15, MASK_CAM_EDITHUD, "Pos", hudCamCheckPos );
	hudAddCheck(  5,  15, MASK_CAM_EDITHUD, "Ang", hudCamCheckAngles );
	hudAddCheck(  10, 15, MASK_CAM_EDITHUD, "Fov", hudCamCheckFov );
	hudAddButton(  0, 16, MASK_CAM_HUD, "SmoothPos:", hudCamSmoothPos );
	hudAddButton(  0, 17, MASK_CAM_HUD, "SmoothAngles:", hudCamSmoothAngles );

	// Chase items
	hudAddFloat(   0,  4, MASK_CHASE_EDIT, "PosX:",  hudChasePosX );
	hudAddFloat(   0,  5, MASK_CHASE_EDIT, "PosY:",  hudChasePosY );
	hudAddFloat(   0,  6, MASK_CHASE_EDIT, "PosZ:",  hudChasePosZ );
	hudAddFloat(   0,  7, MASK_CHASE_EDIT, "Pitc:",  hudChasePitch);
	hudAddFloat(   0,  8, MASK_CHASE_EDIT, "Yaw :",  hudChaseYaw );
	hudAddFloat(   0,  9, MASK_CHASE_EDIT, "Roll:",  hudChaseRoll );
	hudAddFloat(   0, 10, MASK_CHASE_EDIT, "Distance:", hudChaseDistance );
	hudAddHandler( 0, 11, MASK_CHASE, "Target:", hudChaseTarget );

	//Line offset items
	hudAddHandler(   0,   4, MASK_LINE, "Time:", hudLineTime );
	hudAddHandler(   0,   5, MASK_LINE, "Offset:", hudLineOffset );
	hudAddHandler(   0,   6, MASK_LINE, "Speed:", hudLineSpeed );

	// Capture items
	hudAddHandler(   0,   8, MASK_LINE, "Start:", hudLineStart );
	hudAddHandler(   0,   9, MASK_LINE, "End:", hudLineEnd );
	hudAddCvar(   0,  10, MASK_LINE_HUD, "Fps:", "mov_captureFPS" );
	hudAddCvar(   0,  11, MASK_LINE_HUD, "BlurFrames:", "mme_blurFrames" );
	hudAddCvar(   0,  12, MASK_LINE_HUD, "BlurOverlap:", "mme_blurOverlap" );
	hudAddCvar(   0,  13, MASK_LINE_HUD, "saveDepth:", "mme_saveDepth" );
	hudAddCvar(   0,  14, MASK_LINE_HUD, "depthFocus:", "mme_depthFocus" );
	hudAddCvar(   0,  15, MASK_LINE_HUD, "depthRange:", "mme_depthRange" );
	hudAddCvar(   0,  16, MASK_LINE_HUD, "saveStencil:", "mme_saveStencil" );
	hudAddCvar(   0,  17, MASK_LINE_HUD, "MusicFile:", "mov_musicFile" );
	hudAddCvar(   0,  18, MASK_LINE_HUD, "MusicStart:", "mov_musicStart" );
	hudAddCvar(   0,  19, MASK_LINE_HUD, "Rolling shutter pixels:", "mme_rollingShutterPixels" );
	hudAddCvar(   0,  20, MASK_LINE_HUD, "Rolling shutter multiplier:", "mme_rollingShutterMultiplier" );
	hudAddCvar(   0,  21, MASK_LINE_HUD, "Rolling shutter blur:", "mme_rollingShutterBlur" );
	
	// Depth of field Items
	hudAddFloat(   0,  4, MASK_DOF_EDIT, "Focus:",  hudDofFocus );
	hudAddFloat(   0,  5, MASK_DOF_EDIT, "Radius:",  hudDofRadius );

}

static hudItem_t *hudItemAt( float x, float y ) {
	int i;
	for ( i = 0; i< hudItemsUsed; i++ ) {
		float w, h;
		hudItem_t *item = hudItems + i;
		if ((hud.showMask & item->showMask) != item->showMask )
			continue;
		if ( (item->x/HUD_TEXT_WIDTH)*(int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef) > x || item->y > y )
			continue;
		w = x - (item->x/HUD_TEXT_WIDTH)*(int)(HUD_TEXT_WIDTH*cgs.widthRatioCoef);
		h = y - item->y;
		if ( h > HUD_TEXT_HEIGHT )
			continue;
		if ( w > hudItemWidth( item ))
			continue;
		return item;
	}
	return 0;
}

static void hudEditItem( hudItem_t *item, const char *buf ) {
	float *f;
	switch ( item->type ) {
	case hudTypeFloat:
		f = hudGetFloat( item );
		if (!f)
			return;
		*f = atof( buf );
		break;
	case hudTypeValue:
		item->value[0] = atof( buf );
		break;
	case hudTypeCvar:
		trap_Cvar_Set( item->cvar, buf );
		break;
/*	case hudTypeText:
		hudSetText( item, buf );
		break;
*/	}
}

qboolean CG_KeyEvent(int key, qboolean down) {
	int catchMask;
	int len;

	if (!down)
		return qfalse;

	catchMask = trap_Key_GetCatcher();

	len = strlen( hud.edit.line );
	if ( key == A_MOUSE1 ) {
		hudItem_t *item;

		item = hudItemAt( hud.cursorX, hud.cursorY );
		hud.edit.item = 0;
		if ( item ) {
			float *f;
			switch ( item->type ) {
			case hudTypeValue:
				Com_sprintf( hud.edit.line, sizeof( hud.edit.line ), HUD_FLOAT, item->value[0] );
				hud.edit.cursor = strlen( hud.edit.line );
				hud.edit.item = item;
				trap_Key_SetCatcher( KEYCATCH_CGAME | (catchMask &~KEYCATCH_CGAMEEXEC));
				break;
			case hudTypeCvar:
				trap_Cvar_VariableStringBuffer( item->cvar, hud.edit.line, sizeof( hud.edit.line ));
				hud.edit.cursor = strlen( hud.edit.line );
				hud.edit.item = item;
				trap_Key_SetCatcher( KEYCATCH_CGAME | (catchMask &~KEYCATCH_CGAMEEXEC));
				break;
			case hudTypeFloat:
				f = hudGetFloat( item );
				if (!f)
					break;
				Com_sprintf( hud.edit.line, sizeof( hud.edit.line ), HUD_FLOAT, *f );
				hud.edit.cursor = strlen( hud.edit.line );
				hud.edit.item = item;
				trap_Key_SetCatcher( KEYCATCH_CGAME | (catchMask &~KEYCATCH_CGAMEEXEC));
				break;
/*			case hudTypeText:
				hudGetText( item, hud.edit.line, sizeof( hud.edit.line ), qtrue );
				hud.edit.cursor = strlen( hud.edit.line );
				hud.edit.item = item;
				trap_Key_SetCatcher( KEYCATCH_CGAME | (catchMask &~KEYCATCH_CGAMEEXEC));
				break;
*/			case hudTypeButton:
				hudToggleButton( item, 1 );
				break;
			case hudTypeCheck:
				hudToggleChecked( item );
				break;
			}
		}
		return qtrue;
	//Further keypresses only handled when waiting for input
	} else if ( catchMask & KEYCATCH_CGAMEEXEC ) {
		return qfalse;
	} else if ( key == A_DELETE || key == A_KP_PERIOD ) {
		if ( hud.edit.cursor < len ) {
			memmove( hud.edit.line + hud.edit.cursor, 
				hud.edit.line + hud.edit.cursor + 1, len - hud.edit.cursor );
		}
	} else if ( key == A_CURSOR_RIGHT || key == A_KP_6 ) {
		if ( hud.edit.cursor < len ) {
			hud.edit.cursor++;
		}
	} else if ( key == A_CURSOR_LEFT || key == A_KP_4 ) {
		if ( hud.edit.cursor > 0 ) {
			hud.edit.cursor--;
		}
	} else if ( key == A_HOME || key == A_KP_7 || ( tolower(key) == 'a' && trap_Key_IsDown( A_CTRL ) ) ) {
		hud.edit.cursor = 0;
	} else if ( key == A_END || key == A_KP_1 || ( tolower(key) == 'e' && trap_Key_IsDown( A_CTRL ) ) ) {
		hud.edit.cursor = len;
	} else if ( key == A_TAB) {
		hud.edit.item = 0;
		trap_Key_SetCatcher( trap_Key_GetCatcher() & ~(KEYCATCH_CGAME|KEYCATCH_CGAMEEXEC) );
	} else if ( key == A_INSERT || key == A_KP_0 ) {
		trap_Key_SetOverstrikeMode( !trap_Key_GetOverstrikeMode() );
	} else if ( key == A_ENTER || key == A_KP_ENTER ) {
		hudEditItem( hud.edit.item, hud.edit.line );
		hud.edit.item = 0;
		trap_Key_SetCatcher( catchMask | KEYCATCH_CGAME  | KEYCATCH_CGAMEEXEC );
	} else if ( key & K_CHAR_FLAG ) {
		key &= ~K_CHAR_FLAG;
		if ( key == 'h' - 'a' + 1 )	{	// ctrl-h is backspace
			if ( hud.edit.cursor > 0 ) {
				memmove( hud.edit.line + hud.edit.cursor - 1, 
					hud.edit.line + hud.edit.cursor, len + 1 - hud.edit.cursor );
				hud.edit.cursor--;
			} 
		} else if ( key == 'a' - 'a' + 1 ) {	// ctrl-a is home
			hud.edit.cursor = 0;
		} else if ( key== 'e' - 'a' + 1 ) {	// ctrl-e is end
			hud.edit.cursor = len;
		} else if ( key >= 32 &&  len < sizeof( hud.edit.line) - 1 ) {
			if ( trap_Key_GetOverstrikeMode() ) {	
				memmove( hud.edit.line + hud.edit.cursor + 1, hud.edit.line + hud.edit.cursor, len + 1 - hud.edit.cursor );
			}
			hud.edit.line[hud.edit.cursor] = key;
			if ( hud.edit.cursor < sizeof( hud.edit.line) - 1)
				hud.edit.cursor++;
			hud.edit.line[len + 1] = 0;
		} else {
			return qfalse;
		}
	}
	return qtrue;
}

void CG_MouseEvent(int dx, int dy) {
	// update mouse screen position
	hud.cursorX += dx*cgs.widthRatioCoef;
	if (hud.cursorX < 0)
		hud.cursorX = 0;
	else if (hud.cursorX > SCREEN_WIDTH)
		hud.cursorX = SCREEN_WIDTH;

	hud.cursorY += dy;
	if (hud.cursorY < 0)
		hud.cursorY = 0;
	else if (hud.cursorY > SCREEN_HEIGHT)
		hud.cursorY = SCREEN_HEIGHT;
}
