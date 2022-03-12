// Copyright (C) 2009 Sjoerd van der Berg ( harekiet @ gmail.com )

#include "cg_demos.h"
#include "cg_lights.h"
#include "../game/bg_saga.h"

demoMain_t demo;

extern void CG_DamageBlendBlob( void );
extern int CG_DemosCalcViewValues( void );
extern void CG_CalcScreenEffects( void );
extern void CG_PlayBufferedSounds( void );
extern void CG_PowerupTimerSounds( void );
extern void CG_UpdateSoundTrackers();
extern void CG_Clear2DTintsTimes(void);
extern void CG_Draw2D( void );
extern void CG_SaberClashFlare( void );
extern float CG_DrawFPS( float y );
extern void CG_InterpolatePlayerState( qboolean grabAngles );

extern void trap_FX_Reset ( void );
extern void trap_MME_BlurInfo( int* total, int * index );
extern void trap_MME_Capture( const char *baseName, float fps, float focus, float radius);
extern int trap_MME_SeekTime( int seekTime );
extern int trap_MME_FakeAdvanceFrame(int count);
extern void trap_MME_Music( const char *musicName, float time, float length );
extern void trap_MME_Time( int time );
extern void trap_MME_TimeFraction( float timeFraction );
extern qboolean trap_MME_Demo15Detection( void );
extern void trap_R_RandomSeed( int time, float timeFraction );
extern void trap_FX_RandomSeed( int time, float timeFraction );
extern void trap_S_UpdateScale( float scale );

int lastMusicStart;
static void demoSynchMusic( int start, float length ) {
	if ( length > 0 ) {
		lastMusicStart = -1;
	} else {
		length = 0;
		lastMusicStart = start;
	}
	trap_MME_Music( mov_musicFile.string , start * 0.001f, length );
}

static void CG_DemosUpdatePlayer( void ) {
	demo.oldcmd = demo.cmd;
	trap_GetUserCmd( trap_GetCurrentCmdNumber(), &demo.cmd );
	demoMoveUpdateAngles();
	demoMoveDeltaCmd();

	if ( demo.seekEnabled ) {
		int delta;
		demo.play.fraction -= demo.serverDeltaTime * 1000 * demo.cmdDeltaAngles[YAW];
		delta = (int)demo.play.fraction;
		demo.play.time += delta;
		demo.play.fraction -= delta;
	
		if (demo.deltaRight) {
			int interval = mov_seekInterval.value * 1000;
			int rem = demo.play.time % interval;
			if (demo.deltaRight > 0) {
                demo.play.time += interval - rem;
			} else {
                demo.play.time -= interval + rem;
			}
			demo.play.fraction = 0;
		}
		if (demo.play.fraction < 0) {
			demo.play.fraction += 1;
			demo.play.time -= 1;
		}
		if (demo.play.time < 0) {
			demo.play.time = demo.play.fraction = 0;
		}
		return;
	}
	switch ( demo.editType ) {
	case editCamera:
		cameraMove();
		break;
	case editChase:
		demoMoveChase();
		break;
	case editDof:
		dofMove();
		break;
	case editLine:
		demoMoveLine();
		break;
	}
}

#define RADIUS_LIMIT 701.0f
#define EFFECT_LENGTH 400 //msec

static void VibrateView( const float range, const int eventTime, const float fxTime, const float wc, float scale, vec3_t origin, vec3_t angles) {
	float shadingTime;
	int sign;

	if (range > 1 || range < 0) {
		return;
	}
	scale *= fx_Vibrate.value * 100 * wc * range;

	shadingTime = (fxTime - (float)EFFECT_LENGTH) / 1000.0f;

	if (shadingTime < -(float)EFFECT_LENGTH / 1000.0f)
		shadingTime = -(float)EFFECT_LENGTH / 1000.0f;
	else if (shadingTime > 0)
		shadingTime = 0;

	sign = fxRandomSign(eventTime);
	origin[2] += shadingTime * shadingTime * sinf(shadingTime * M_PI * 23.0f) * scale;
	angles[ROLL] += shadingTime * shadingTime * sinf(shadingTime * M_PI * 1.642f) * scale * 0.7f * sign;
}

static void FX_VibrateView( const float scale, vec3_t origin, vec3_t angles ) {
	float range = 1.0f - (cg.eventRadius / RADIUS_LIMIT);
	float oldRange = 1.0f - (cg.eventOldRadius / RADIUS_LIMIT);
	if (cg.eventRadius > cg.eventOldRadius
		&& cg.eventOldRadius != 0
		&& (cg.eventOldTime + EFFECT_LENGTH) > cg.eventTime
		&& cg.eventCoeff * range < cg.eventOldCoeff * oldRange) {
		cg.eventRadius = cg.eventOldRadius;
		cg.eventTime = cg.eventOldTime;
		cg.eventCoeff = cg.eventOldCoeff;
	}
	if ((cg.time >= cg.eventTime) && (cg.time < (cg.eventTime + EFFECT_LENGTH))) {
		float fxTime = (cg.time - cg.eventTime) + cg.timeFraction;
		range = 1.0f - (cg.eventRadius / RADIUS_LIMIT);
		VibrateView(range, cg.eventTime, fxTime, cg.eventCoeff, scale, origin, angles);
		cg.eventOldRadius = cg.eventRadius;
		cg.eventOldTime = cg.eventTime;
		cg.eventOldCoeff = cg.eventCoeff;
	}
}

void CG_SetPredictedThirdPerson(void) {
	cg.renderingThirdPerson = (qboolean) (((cg_thirdPerson.integer
		|| (cg.snap->ps.stats[STAT_HEALTH] <= 0)

		|| (cg.playerCent->currentState.weapon == WP_SABER && !cg.trueView)
		
		|| (cg.predictedPlayerState.forceHandExtend == HANDEXTEND_KNOCKDOWN 
		&& !cg.trueView)

		|| cg.predictedPlayerState.fallingToDeath
		|| cg.predictedPlayerState.usingATST
		
		|| ((CG_InKnockDown(cg.predictedPlayerState.torsoAnim)
		|| CG_InKnockDown(cg.predictedPlayerState.legsAnim))
		&& !cg.trueView)
		)

		&& !(cg_fpls.integer && cg.predictedPlayerState.weapon == WP_SABER))
					      && !cg.zoomMode);

	if (cg.predictedPlayerState.pm_type == PM_SPECTATOR) { //always first person for spec
		cg.renderingThirdPerson = qfalse;
	}
	if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR) {
		cg.renderingThirdPerson = qfalse;
	}
}

static int demoSetupView( void) {
	vec3_t forward;
	qboolean zoomFix = qfalse;	//to see disruptor zoom when we are chasing a player
	int inwater = qfalse;
	int	contents = 0;
	qboolean behindView = qfalse;

	cg.zoomMode = qfalse;
	cg.trueView = qfalse;
	cg.playerPredicted = qfalse;
	cg.playerCent = 0;
	demo.viewFocus = 0;
	demo.viewRadius = 0;
	demo.viewTarget = -1;

	switch (demo.viewType) {
	case viewChase:
		// this is the case for normal third person view (i think)
		if ( demo.chase.cent && demo.chase.distance < mov_chaseRange.value ) {
			centity_t *cent = demo.chase.cent;
			
			if ( cent->currentState.number < MAX_CLIENTS ) {
				int weapon = cent->currentState.weapon;
				cg.trueView = (qboolean) ( (weapon == WP_SABER && cg_trueSaber.integer)
							   || (weapon != WP_SABER && cg_trueGuns.integer) );
				cg.playerCent = cent;
				cg.playerPredicted = (qboolean) (cent == &cg_entities[cg.snap->ps.clientNum]);
				if (!cg.playerPredicted ) {
					//Make sure lerporigin of playercent is val
					CG_CalcEntityLerpPositions( cg.playerCent );
				}
				if (cg.playerPredicted) {
					cg.zoomMode = (qboolean) (cg.snap->ps.zoomMode || cg.predictedPlayerState.zoomMode);
					CG_SetPredictedThirdPerson();
				} else {
					int torsoAnim = cg.playerCent->currentState.torsoAnim & ~ANIM_TOGGLEBIT;
					cg.zoomMode = (qboolean) (((torsoAnim == TORSO_WEAPONREADY4
						|| torsoAnim == BOTH_ATTACK4) && !demo15detected)
						||
						((torsoAnim == TORSO_WEAPONREADY4_15
						  || torsoAnim == BOTH_ATTACK4_15) && demo15detected));
					cg.renderingThirdPerson = (qboolean) ((cg_thirdPerson.integer || cent->currentState.eFlags & EF_DEAD
						|| (weapon == WP_SABER && !cg.trueView) || cg.fallingToDeath) && !cg.zoomMode
									      && !(cg_fpls.integer && weapon == WP_SABER));
				}
				inwater = CG_DemosCalcViewValues();
				// first person blend blobs, done after AnglesToAxis
				if (!cg.renderingThirdPerson) {
					CG_DamageBlendBlob();
				}
				VectorCopy( cg.refdef.vieworg, demo.viewOrigin );
				VectorCopy( cg.refdefViewAngles, demo.viewAngles );
				zoomFix = qtrue;
			} else {
				VectorCopy( cent->lerpOrigin, demo.viewOrigin );
				VectorCopy( cent->lerpAngles, demo.viewAngles );
			}
			demo.viewFov = cg_fov.value;
		} else {
			memset( &cg.refdef, 0, sizeof(refdef_t));
			AngleVectors( demo.chase.angles, forward, 0, 0 );
			VectorMA( demo.chase.origin , -demo.chase.distance, forward, demo.viewOrigin );
			VectorCopy( demo.chase.angles, demo.viewAngles );
			demo.viewFov = cg_fov.value;
			demo.viewTarget = demo.chase.target;
			cg.renderingThirdPerson = qtrue;
			gCGHasFallVector = qfalse;
		}
		break;
	case viewCamera:
		memset( &cg.refdef, 0, sizeof(refdef_t));
		VectorCopy( demo.camera.origin, demo.viewOrigin );
		VectorCopy( demo.camera.angles, demo.viewAngles );
		demo.viewFov = demo.camera.fov + cg_fov.value;
		demo.viewTarget = demo.camera.target;
		cg.renderingThirdPerson = qtrue;
		cameraMove();
		gCGHasFallVector = qfalse;
		break;
	default:
		return inwater;
	}

	demo.viewAngles[YAW]	+= mov_deltaYaw.value;
	demo.viewAngles[PITCH]	+= mov_deltaPitch.value;
	demo.viewAngles[ROLL]	+= mov_deltaRoll.value;

	if (fx_Vibrate.value > 0
		&& cg.eventCoeff > 0
/*		&&  demo.viewType == viewCamera
		|| ( demo.viewType == viewChase
		&& ((cg.renderingThirdPerson && demo.chase.distance < mov_chaseRange.value)
		|| demo.chase.distance > mov_chaseRange.value))*/) {
			FX_VibrateView( 1.0f, demo.viewOrigin, demo.viewAngles );
	}
	VectorCopy( demo.viewOrigin, cg.refdef.vieworg );
	VectorCopy( demo.viewAngles, cg.refdefViewAngles );
	AnglesToAxis( demo.viewAngles, cg.refdef.viewaxis );

	/* find focus ditance to certain target but don't apply if dof is not locked, use for drawing */
	if ( demo.dof.target >= 0 ) {
		centity_t* targetCent = demoTargetEntity( demo.dof.target );
		if ( targetCent ) {
			vec3_t targetOrigin;
			chaseEntityOrigin( targetCent, targetOrigin );
			//Find distance betwene plane of camera and this target
			demo.viewFocus = DotProduct( cg.refdef.viewaxis[0], targetOrigin ) - DotProduct( cg.refdef.viewaxis[0], cg.refdef.vieworg  );
			demo.dof.focus = demo.viewFocusOld = demo.viewFocus;
		} else {
			demo.dof.focus = demo.viewFocus = demo.viewFocusOld;
		}
		if (demo.dof.focus < 0.001f) {
			behindView = qtrue;
		}
	}
	if ( demo.dof.locked ) {
		if (!behindView) {
			demo.viewFocus = demo.dof.focus;		
			demo.viewRadius = demo.dof.radius;
		} else {
			demo.viewFocus = 0.002f;		// no matter what value, just not less or equal zero
			demo.viewRadius = 0.0f;
		}
	} else if ( demo.viewTarget >= 0 ) {
		centity_t* targetCent = demoTargetEntity( demo.viewTarget );
		if ( targetCent ) {
			vec3_t targetOrigin;
			chaseEntityOrigin( targetCent, targetOrigin );
			//Find distance betwene plane of camera and this target
			demo.viewFocus = DotProduct( cg.refdef.viewaxis[0], targetOrigin ) - DotProduct( cg.refdef.viewaxis[0], cg.refdef.vieworg  );
			demo.viewRadius = CG_Cvar_Get( "mme_dofRadius" );
		}
	} else if ( demo.dof.target >= 0 ) {
		demo.viewFocus = 0;
		demo.viewRadius = 0;
	}

	cg.refdef.width = cgs.glconfig.vidWidth*cg_viewsize.integer/100;
	cg.refdef.width &= ~1;

	cg.refdef.height = cgs.glconfig.vidHeight*cg_viewsize.integer/100;
	cg.refdef.height &= ~1;

	cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width)/2;
	cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height)/2;
	if (!zoomFix) {
		cg.refdef.fov_x = demo.viewFov;
		cg.refdef.fov_y = atan2( cg.refdef.height, (cg.refdef.width / tan( demo.viewFov / 360 * M_PI )) ) * 360 / M_PI;
		contents = CG_PointContents( cg.refdef.vieworg, -1 );
		if ( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ){
			double v = WAVE_AMPLITUDE * sin(((double)cg.time + (double)cg.timeFraction) / 1000.0 * WAVE_FREQUENCY * M_PI * 2);
			cg.refdef.fov_x += v;
			cg.refdef.fov_y -= v;
			inwater = qtrue;
		} else {
			inwater = qfalse;
		}
	}
	return inwater;
}

extern snapshot_t *CG_ReadNextSnapshot(void);
extern void CG_SetNextSnap(snapshot_t *snap);
extern void CG_SetNextNextSnap(snapshot_t* snap);	
extern void CG_TransitionSnapshot(void);
extern void CG_AddToHistory(int serverTime, entityState_t* state, centity_t* cent);
extern void CG_UpdateTps(snapshot_t* snap, qboolean isTeleport);

void demoProcessSnapShots(qboolean hadSkip) {
	int i;
	snapshot_t		*snap;

	// see what the latest snapshot the client system has is
	trap_GetCurrentSnapshotNumber( &cg.latestSnapshotNum, &cg.latestSnapshotTime );
	if (hadSkip || !cg.snap) {
		cgs.processedSnapshotNum = max(cg.latestSnapshotNum - 3, -1);
		if (cg.nextSnap)
			cgs.serverCommandSequence = cg.nextSnap->serverCommandSequence;
		else if (cg.snap)
			cgs.serverCommandSequence = cg.snap->serverCommandSequence;
		cg.snap = 0;
		cg.nextSnap = 0;
		cg.nextNextSnap = 0;

		for (i=-1;i<MAX_GENTITIES;i++) {
			centity_t *cent = i < 0 ? &cg_entities[cg.predictedPlayerState.clientNum] : &cg_entities[i];
			cent->trailTime = cg.time;
			cent->snapShotTime = cg.time;
			cent->currentValid = qfalse;
			cent->interpolate = qfalse;
			cent->muzzleFlashTime = cg.time - MUZZLE_FLASH_TIME - 1;
			cent->previousEvent = 0;
			if (cent->currentState.eType == ET_PLAYER) {
				memset( &cent->pe, 0, sizeof( cent->pe ) );
				cent->pe.legs.yawAngle = cent->lerpAngles[YAW];
				cent->pe.torso.yawAngle = cent->lerpAngles[YAW];
				cent->pe.torso.pitchAngle = cent->lerpAngles[PITCH];
			}
		}
	}

	/* Check if we have some transition between snapsnots */
	if (!cg.snap) {
		entityState_t pes;
		snap = CG_ReadNextSnapshot();
		if (!snap)
			return;
		cg.snap = snap;
		if ((cg_entities[snap->ps.clientNum].ghoul2 == NULL)
			&& snap->ps.clientNum > 0 && trap_G2_HaveWeGhoul2Models(cgs.clientinfo[snap->ps.clientNum].ghoul2Model)) { // snap->ps.clientNum>0 added because when ending a demo we sometimes get access violation here
			trap_G2API_DuplicateGhoul2Instance(cgs.clientinfo[snap->ps.clientNum].ghoul2Model, &cg_entities[snap->ps.clientNum].ghoul2);
			CG_CopyG2WeaponInstance(&cg_entities[snap->ps.clientNum], FIRST_WEAPON, cg_entities[snap->ps.clientNum].ghoul2);
		}
		BG_PlayerStateToEntityState( &snap->ps, &cg_entities[ snap->ps.clientNum ].currentState, qfalse );
		CG_BuildSolidList();
		CG_ExecuteNewServerCommands( snap->serverCommandSequence );
		CG_UpdateTps(snap, qtrue);
		BG_PlayerStateToEntityStateExtraPolate(&snap->ps, &pes, snap->ps.commandTime, qfalse);
		CG_AddToHistory(snap->serverTime, &pes, &cg_entities[snap->ps.clientNum]);
		for (i = 0; i < cg.snap->numEntities; i++) {
			entityState_t *state = &cg.snap->entities[ i ];
			centity_t *cent = &cg_entities[ state->number ];
			memcpy(&cent->currentState, state, sizeof(entityState_t));
			cent->interpolate = qfalse;
			cent->currentValid = qtrue;
			CG_AddToHistory(snap->serverTime, state, cent);
			if (cent->currentState.eType > ET_EVENTS)
				cent->previousEvent = 1;
			else 
				cent->previousEvent = cent->currentState.event;
		}
	}
	do {
		if (!cg.nextSnap) {
			snap = CG_ReadNextSnapshot();
			if (!snap)
				break;
			CG_SetNextSnap( snap );
		}
		if (!cg.nextNextSnap) {
			snap = CG_ReadNextSnapshot();
			if (!snap)
				break;
			CG_SetNextNextSnap(snap);
		}
		if (cg.timeFraction >= cg.snap->serverTime - cg.time && cg.timeFraction < cg.nextSnap->serverTime - cg.time)
			break;
		//Todo our own transition checking if we wanna hear certain sounds
		CG_TransitionSnapshot();
	} while (1);
}

void CG_DemosDrawActiveFrame(int serverTime, stereoFrame_t stereoView) {
	int deltaTime;
	qboolean hadSkip;
	qboolean captureFrame;
	//int rollingShutterFactor = 108;
	int mme_rollingShutterPixels = CG_Cvar_GetInt("mme_rollingShutterPixels");
	float mme_rollingShutterMultiplier = CG_Cvar_Get("mme_rollingShutterMultiplier");
	int bufferCountNeededForRollingshutter = (int)(ceil(mme_rollingShutterMultiplier) + 0.5f); // ceil bc if value is 1.1 we need 2 buffers. +.5 to avoid float issues..
	int rollingShutterFactor = cgs.glconfig.vidHeight/ mme_rollingShutterPixels;
	float captureFPS;
	float frameSpeed;
	int blurTotal, blurIndex;
	float blurFraction;
	static qboolean intermission = qfalse;
	float stereoSep = CG_Cvar_Get( "r_stereoSeparation" );

	int inwater, entityNum;
	vec4_t hcolor = {0, 0, 0, 0};

	if (!demo.initDone) {
		//super duper mega hack
		demo15detected = trap_MME_Demo15Detection();
		if ( !cg.snap ) {
			demoProcessSnapShots( qtrue );
		}
		if ( !cg.snap ) {
			CG_Error( "No Initial demo snapshot found" );
		}
		demoPlaybackInit();
	}

	cg.demoPlayback = 2;

	if (cg.snap && ui_myteam.integer != cg.snap->ps.persistant[PERS_TEAM]) {
		trap_Cvar_Set ( "ui_myteam", va("%i", cg.snap->ps.persistant[PERS_TEAM]) );
	}
	// update cvars
	CG_UpdateCvars();
	// if we are only updating the screen as a loading
	// pacifier, don't even try to read snapshots
	if ( cg.infoScreenText[0] != 0 ) {
		CG_DrawInformation();
		return;
	}

	captureFrame = (qboolean) (demo.capture.active && !demo.play.paused);
	if ( captureFrame ) {
		trap_MME_BlurInfo( &blurTotal, &blurIndex );
		captureFPS = mov_captureFPS.value*(float)rollingShutterFactor/ mme_rollingShutterMultiplier;
		//captureFPS = mov_captureFPS.value*(float)rollingShutterFactor/ (float)bufferCountNeededForRollingshutter; // Wrong. Must get frames at correct speed for rolling shutter.
		if ( blurTotal > 0) {
			captureFPS *= blurTotal;
			blurFraction = blurIndex / (float)blurTotal;
		} else {
			blurFraction = 0;
		}
	}

	/* Forward the demo */
	deltaTime = serverTime - demo.serverTime;
	if (deltaTime > 50)
		deltaTime = 50;
	demo.serverTime = serverTime;
	demo.serverDeltaTime = 0.001 * deltaTime;
	cg.oldTime = cg.time;
	cg.oldTimeFraction = cg.timeFraction;

	if (demo.play.time < 0) {
		demo.play.time = demo.play.fraction = 0;
	}

	demo.play.oldTime = demo.play.time;


	/* Handle the music */
	if ( demo.play.paused ) {
		if ( lastMusicStart >= 0)
			demoSynchMusic( -1, 0 ); 
	} else {
		int musicStart = (demo.play.time - mov_musicStart.value * 1000 );
		if ( musicStart <= 0 ) {
			if (lastMusicStart >= 0 )
				demoSynchMusic( -1, 0 );
		} else {
			if ( demo.play.time != demo.play.lastTime || lastMusicStart < 0)
				demoSynchMusic( musicStart, 0 );
		}
	}
	/* forward the time a bit till the moment of capture */
	if ( captureFrame && demo.capture.locked && demo.play.time < demo.capture.start) {
		int left = demo.capture.start - demo.play.time;
		if ( left > 2000) {
			left -= 1000;
			captureFrame = qfalse;
		} else if (left > 5) {
			captureFrame = qfalse;
			left = 5;
		}
		demo.play.time += left;
	} else if ( captureFrame && demo.loop.total && blurTotal ) {
		float loopFraction = demo.loop.index / (float)demo.loop.total;
		demo.play.time = demo.loop.start;
		demo.play.fraction = demo.loop.range * loopFraction;
		demo.play.time += (int)demo.play.fraction;
		demo.play.fraction -= (int)demo.play.fraction;
	} else if ( captureFrame ) {
		float frameDelay = 1000.0f / captureFPS;
		demo.play.fraction += frameDelay * demo.play.speed;
		demo.play.time += (int)demo.play.fraction;
		demo.play.fraction -= (int)demo.play.fraction;
	} else if ( demo.find ) {
		demo.play.time = demo.play.oldTime + 20;
		demo.play.fraction = 0;
		if ( demo.play.paused )
			demo.find = findNone;
	} else if ( !demo.play.paused ) {
		float delta = demo.play.fraction + deltaTime * demo.play.speed;
		demo.play.time += (int)delta;
		demo.play.fraction = delta - (int)delta;
	}

	demo.play.lastTime = demo.play.time;

	if ( demo.loop.total && captureFrame && blurTotal ) {
		//Delay till we hit the right part at the start
		int time;
		float timeFraction;
		if ( demo.loop.lineDelay && !blurIndex ) {
			time = demo.loop.start - demo.loop.lineDelay;
			timeFraction = 0;
			if ( demo.loop.lineDelay > 8 )
				demo.loop.lineDelay -= 8;
			else
				demo.loop.lineDelay = 0;
			captureFrame = qfalse;
		} else {
			if ( blurIndex == blurTotal - 1 ) {
				//We'll restart back to the start again
				demo.loop.lineDelay = 2000;
				if ( ++demo.loop.index >= demo.loop.total ) {
					demo.loop.total = 0;
				}
			}
			time = demo.loop.start;
			timeFraction = demo.loop.range * blurFraction;
		}
		time += (int)timeFraction;
		timeFraction -= (int)timeFraction;
		lineAt( time, timeFraction, &demo.line.time, &cg.timeFraction, &frameSpeed );
	} else {
		lineAt( demo.play.time, demo.play.fraction, &demo.line.time, &cg.timeFraction, &frameSpeed );
	}

	/* Set the correct time */
	cg.time = trap_MME_SeekTime( demo.line.time );
	/* cg.time is shifted ahead a bit to correct some issues.. */
	frameSpeed *= demo.play.speed;

	// Sending correct time over a bit earlier so the commands are evaluated correctly, especially waveforms
	trap_MME_Time(cg.time);
	trap_MME_TimeFraction(cg.timeFraction);

	// Demo project commands
	evaluateDemoCommand();


	cg.frametime = (cg.time - cg.oldTime) + (cg.timeFraction - cg.oldTimeFraction);
	if (cg.frametime < 0) {
		int i;
		cg.frametime = 0;
		hadSkip = qtrue;
		cg.oldTime = cg.time;
		cg.oldTimeFraction = cg.timeFraction;
		CG_InitLocalEntities();
		CG_InitMarkPolys();
		CG_ClearParticles ();
		CG_Clear2DTintsTimes();
		trap_FX_Reset();
		
		cg.eventRadius = cg.eventOldRadius = 0;
		cg.eventTime = cg.eventOldTime = 0;
		cg.eventCoeff = cg.eventOldCoeff = 0;

		cg.centerPrintTime = 0;
        cg.damageTime = 0;
		cg.powerupTime = 0;
		cg.rewardTime = 0;
		cg.scoreFadeTime = 0;
		cg.lastKillTime = 0;
		cg.attackerTime = 0;
		cg.soundTime = 0;
		cg.itemPickupTime = 0;
		cg.itemPickupBlendTime = 0;
		cg.weaponSelectTime = 0;
		cg.headEndTime = 0;
		cg.headStartTime = 0;
		cg.v_dmg_time = 0;
		cg.fallingToDeath = 0;
		cg.weapFrame = 0;
		cg.weapFrameTime = 0;
		cgScreenEffects.shake_duration = 0;
		demo.rain.time = 0;
		trap_S_ClearLoopingSounds(qtrue);
		
		for (i = 0; i < MAX_CHATBOX_ITEMS; i++)
			cg.chatItems[i].time = 0;
		for (i = 0; i < MAX_CLIENTS && mov_dismember.integer; i++)
			CG_ReattachLimb(&cg_entities[i]);
		CG_LoadDeferredPlayers();
	} else if (cg.frametime > 100) {
		hadSkip = qtrue;
		CG_LoadDeferredPlayers();
	} else {
		hadSkip = qfalse;
	}

	/* Make sure the random seed is the same each time we hit this frame */
	srand(cg.time % 10000000 + cg.timeFraction * 1000);
	trap_R_RandomSeed(cg.time % 10000000, cg.timeFraction * 1000);
	trap_FX_RandomSeed(cg.time % 10000000, cg.timeFraction * 1000);

	//silly hack :s
	if (demo.play.paused || !frameSpeed) {
		static float lastTimeFraction = 0.0f;
		if (!frameSpeed) {
			trap_FX_AdjustTime(cg.time, cg.frametime, lastTimeFraction, cg.refdef.vieworg, cg.refdef.viewaxis);
		} else {
			trap_FX_AdjustTime(cg.time, cg.frametime, 0, cg.refdef.vieworg, cg.refdef.viewaxis);
			lastTimeFraction = cg.timeFraction;
		}
	} else {
		trap_FX_AdjustTime(cg.time, cg.frametime, cg.timeFraction, cg.refdef.vieworg, cg.refdef.viewaxis);
	}

	CG_RunLightStyles();
	/* Prepare to render the screen */		
	trap_S_ClearLoopingSounds(qfalse);
	trap_R_ClearScene();
	
	/* Update demo related information */
	demoProcessSnapShots( hadSkip );
	trap_ROFF_UpdateEntities();
	if ( !cg.snap ) {
		CG_DrawInformation();
		return;
	}
	CG_PreparePacketEntities( );
	CG_DemosUpdatePlayer( );
	chaseUpdate( demo.play.time, demo.play.fraction );
	cameraUpdate( demo.play.time, demo.play.fraction );
	dofUpdate( demo.play.time, demo.play.fraction );
	cg.clientFrame++;
	// update cg.predictedPlayerState
	CG_InterpolatePlayerState( qfalse );
	CG_CheckPlayerG2Weapons(&cg.predictedPlayerState, &cg_entities[cg.predictedPlayerState.clientNum]);
	BG_PlayerStateToEntityState(&cg.predictedPlayerState, &cg_entities[cg.snap->ps.clientNum].currentState, qfalse);
	cg_entities[cg.snap->ps.clientNum].currentValid = qtrue;
	VectorCopy( cg_entities[cg.snap->ps.clientNum].currentState.pos.trBase, cg_entities[cg.snap->ps.clientNum].lerpOrigin );
	VectorCopy( cg_entities[cg.snap->ps.clientNum].currentState.apos.trBase, cg_entities[cg.snap->ps.clientNum].lerpAngles );

	inwater = demoSetupView();
	
	if (cg.playerPredicted)
		cg.fallingToDeath = cg.snap->ps.fallingToDeath;
	else if ((cg.playerCent && (cg.fallingToDeath + 5000) < cg.time
		&& !(cg.playerCent->currentState.eFlags & EF_DEAD)) || !cg.playerCent)
		cg.fallingToDeath = 0;

	if (demo.rain.active) { // transfer this to CG_DoSaber
		cg.rainNumber = demo.rain.number;
		cg.rainTime = demo.rain.time;
	} else {
		cg.rainNumber = 0;
		cg.rainTime = INT_MAX;
	}
	
	VectorClear(cg.lastFPFlashPoint);

	CG_CalcScreenEffects();
		
	CG_AddPacketEntities();			// adter calcViewValues, so predicted player state is correct
	CG_AddMarks();
	CG_AddParticles ();
	CG_AddLocalEntities();

	if ( cg.playerCent == &cg_entities[cg.predictedPlayerState.clientNum] ) {
		// warning sounds when powerup is wearing off
		CG_PowerupTimerSounds();
		CG_AddViewWeapon( &cg.predictedPlayerState  );
	} else if ( !(cg.predictedPlayerState.pm_type == PM_INTERMISSION) &&
		cg.playerCent && cg.playerCent->currentState.number < MAX_CLIENTS )  {
		CG_AddViewWeaponDirect( cg.playerCent );
	}
	trap_S_UpdateEntityPosition(ENTITYNUM_NONE, cg.refdef.vieworg);
	CG_PlayBufferedSounds();
	CG_PlayBufferedVoiceChats();
		
	trap_FX_AddScheduledEffects();

	cg.refdef.time = cg.time;
	memcpy( cg.refdef.areamask, cg.snap->areamask, sizeof( cg.refdef.areamask ) );
	/* Render some extra demo related stuff */
	if (!captureFrame) {
		switch (demo.editType) {
		case editCamera:
			cameraDraw( demo.play.time, demo.play.fraction );
			break;
		case editChase:
			chaseDraw( demo.play.time, demo.play.fraction );
			break;
		case editDof:
			dofDraw( demo.play.time, demo.play.fraction );
			break;
		}
		/* Add bounding boxes for easy aiming */
		if (demo.editType && (demo.cmd.buttons & BUTTON_ATTACK) && (demo.cmd.buttons & BUTTON_ALT_ATTACK)) {
			int i;
			centity_t *targetCent;
			for (i = 0;i<MAX_GENTITIES;i++) {
				targetCent = demoTargetEntity( i );
				if (targetCent) {
					vec3_t container, traceStart, traceImpact, forward;
					const float *color;

					demoCentityBoxSize( targetCent, container );
					VectorSubtract( demo.viewOrigin, targetCent->lerpOrigin, traceStart );
					AngleVectors( demo.viewAngles, forward, 0, 0 );
					if (BoxTraceImpact( traceStart, forward, container, traceImpact )) {
						color = colorRed;
					} else {
						color = colorYellow;
					}
					demoDrawBox( targetCent->lerpOrigin, container, color );
				}
			}

		}
	}
	
	CG_UpdateSoundTrackers();

	if (gCGHasFallVector) {
		vec3_t lookAng;
		cg.renderingThirdPerson = qtrue;
		VectorSubtract(cg.playerCent->lerpOrigin, cg.refdef.vieworg, lookAng);
		VectorNormalize(lookAng);
		vectoangles(lookAng, lookAng);
		VectorCopy(gCGFallVector, cg.refdef.vieworg);
		AnglesToAxis(lookAng, cg.refdef.viewaxis);
	}

	if (frameSpeed > 5)
		frameSpeed = 5;

	trap_S_UpdateScale( frameSpeed );
	if (cg.playerCent && cg.predictedPlayerState.pm_type == PM_INTERMISSION) {
		entityNum = cg.snap->ps.clientNum;
	} else if (cg.playerCent) {
		entityNum = cg.playerCent->currentState.number;
	} else {
		entityNum = ENTITYNUM_NONE;
	}
	trap_S_Respatialize( entityNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater);
	
	demoDrawSun();
	demoDrawRain();

	//Always!!! start with negative
	if (captureFrame && stereoSep > 0.0f)
		trap_Cvar_Set("r_stereoSeparation", va("%f", -stereoSep));
	CG_TileClear();
	trap_MME_TimeFraction(cg.timeFraction);

	VectorCopy(cg.refdefViewAngles,cg.refdef.viewAngles); // For MME so we can export AE cam paths
	for (int i = 0; i < MAX_CLIENTS; i++) { // For MME so we can export paths for all players
		centity_t* cent = &cg_entities[i];
		clientInfo_t* ci = cgs.clientinfo + i;
		if (ci->bolt_head) {
			mdxaBone_t boneMatrix;
			if (trap_G2API_GetBoltMatrix(cent->ghoul2, 0, ci->bolt_head, &boneMatrix, cent->turAngles, cent->lerpOrigin, cg.time, cgs.gameModels, cent->modelScale)) {
				vec3_t betterOrigin;
				trap_G2API_GiveMeVectorFromMatrix(&boneMatrix, ORIGIN, betterOrigin);
				VectorCopy(betterOrigin, cg.refdef.playerPositions[i]);
			}
		}
	}
	trap_R_RenderScene( &cg.refdef );

	CG_FillRect(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, hcolor);
	if (demo.viewType == viewChase && cg.playerCent && (cg.playerCent->currentState.number < MAX_CLIENTS))
		CG_Draw2D();
	else if (cg_draw2D.integer) {
		CG_SaberClashFlare();
		if (cg_drawFPS.integer)
			CG_DrawFPS(0.0f);
		if (mov_drawChatbox.integer)
			CG_ChatBox_DrawStrings();
	}
	
	//those looping sounds in intermission are annoying
	if (cg.predictedPlayerState.pm_type == PM_INTERMISSION && !intermission) {
		int i;
		for (i = 0; i < MAX_GENTITIES; i++)
			trap_S_StopLoopingSound(i);
		intermission = qtrue;
	}
	if (cg.predictedPlayerState.pm_type != PM_INTERMISSION) {
		intermission = qfalse;
	}

	Cam_Draw2d(); //2D
		
	if (captureFrame) {
		char fileName[MAX_OSPATH];
		Com_sprintf( fileName, sizeof( fileName ), "capture/%s/%s", mme_demoFileName.string, mov_captureName.string );
		trap_MME_Capture( fileName, captureFPS/(float)rollingShutterFactor* mme_rollingShutterMultiplier, demo.viewFocus, demo.viewRadius);
	} else {
		if (demo.editType && !cg.playerCent)
			demoDrawCrosshair();
		hudDraw();
	}

	if ( demo.capture.active && demo.capture.locked && demo.play.time > demo.capture.end  ) {
		Com_Printf( "Capturing ended\n" );
		if (demo.autoLoad) {
			trap_SendConsoleCommand( "disconnect\n" );
		} 
		demo.capture.active = qfalse;
	}

	doFX = qfalse;
}

void CG_DemosAddLog(const char *fmt, ...) {
	char *dest;
	va_list		argptr;

	demo.log.lastline++;
	if (demo.log.lastline >= LOGLINES)
		demo.log.lastline = 0;
	
	demo.log.times[demo.log.lastline] = demo.serverTime;
	dest = demo.log.lines[demo.log.lastline];

	va_start ( argptr, fmt );
	Q_vsnprintf( dest, sizeof(demo.log.lines[0]), fmt, argptr );
	va_end (argptr);
//	Com_Printf("%s\n", dest);
}

static void demoViewCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (!Q_stricmp(cmd, "chase")) {
		demo.viewType = viewChase;
	} else if (!Q_stricmp(cmd, "camera")) {
		demo.viewType = viewCamera;
	} else if (!Q_stricmp(cmd, "prev")) {
		switch(demo.viewType) {
		case viewCamera:
			demo.viewType = viewLast;
			break;
		case viewChase:
			demo.viewType = viewCamera;
			break;
		case viewLast:
			demo.viewType = viewChase;
			break;
		}
	} else if (!Q_stricmp(cmd, "next")) {
		switch(demo.viewType) {
		case viewCamera:
			demo.viewType = viewChase;
			break;
		case viewChase:
			demo.viewType = viewLast;
			break;
		case viewLast:
			demo.viewType = viewCamera;
			break;
		}
	} else {
		Com_Printf("view usage:\n" );
		Com_Printf("view camera, Change to camera view.\n" );
		Com_Printf("view chase, Change to chase view.\n" );
		Com_Printf("view next/prev, Change to next or previous view.\n" );
		return;
	}

	switch (demo.viewType) {
	case viewCamera:
		CG_DemosAddLog("View set to camera" );
		break;
	case viewChase:
		CG_DemosAddLog("View set to chase" );
		break;
	}
}

static void demoEditCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (!Q_stricmp(cmd, "none"))  {
		demo.editType = editNone;
		CG_DemosAddLog("Not editing anything");
	} else if (!Q_stricmp(cmd, "chase")) {
		if ( demo.cmd.upmove > 0 ) {
			demoViewCommand_f();
			return;
		}
		demo.editType = editChase;
		CG_DemosAddLog("Editing chase view");
	} else if (!Q_stricmp(cmd, "camera")) {
		if ( demo.cmd.upmove > 0 ) {
			demoViewCommand_f();
			return;
		}
		demo.editType = editCamera;
		CG_DemosAddLog("Editing camera");
	} else if (!Q_stricmp(cmd, "dof")) {
		demo.editType = editDof;
		CG_DemosAddLog("Editing depth of field");
	} else if (!Q_stricmp(cmd, "line")) {
		demo.editType = editLine;
		CG_DemosAddLog("Editing timeline");
	}  if (!Q_stricmp(cmd, "commands")) {
		demo.editType = editCommands;
		CG_DemosAddLog("Editing commands");
	} else {
		switch ( demo.editType ) {
		case editCamera:
			demoCameraCommand_f();
			break;
		case editChase:
			demoChaseCommand_f();
			break;
		case editLine:
			demoLineCommand_f();
			break;
		case editCommands:
			demoCommandsCommand_f();
			break;
		case editDof:
			demoDofCommand_f();
			break;
		}
	}
}

static void demoSeekTwoCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (isdigit( cmd[0] )) {
		//teh's parser for time MM:SS.MSEC, thanks *bow*
		int i;
		char *sec, *min;;
		min = (char *)cmd;
		for( i=0; min[i]!=':'&& min[i]!=0; i++ );
		if(cmd[i]==0)
			sec = 0;
		else
		{
			min[i] = 0;
			sec = min+i+1;
		}
		demo.play.time = ( atoi( min ) * 60000 + ( sec ? atof( sec ) : 0 ) * 1000 );
		demo.play.fraction = 0;
	}
}
#ifdef RELDEBUG
#pragma optimize("", off)
#endif
static void demoSeekSyncCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (isdigit( cmd[0] )  ) {
		//teh's parser for time MM:SS.MSEC, thanks *bow*
		int i;
		char *sec, *min;;
		min = (char *)cmd;
		for( i=0; min[i]!=':'&& min[i]!=0; i++ );
		if(cmd[i]==0)
			sec = 0;
		else
		{
			min[i] = 0;
			sec = min+i+1;
		}


		float desiredTime = (atoi(min) * 60000 + (sec ? atof(sec) : 0) * 1000);

		if (demo.play.time + demo.play.fraction >= desiredTime) {
			return; // Makes no sense. This is for seeking in sync for recording. Can't record backwards.
		}

		const char* fpsString = CG_Argv(2);
		float fps = (fpsString && isdigit(fpsString[0])) ? atof(fpsString) : mov_captureFPS.value;

		// Simulate normal adding during capture.
		int mme_rollingShutterPixels = CG_Cvar_GetInt("mme_rollingShutterPixels");
		float mme_rollingShutterMultiplier = CG_Cvar_Get("mme_rollingShutterMultiplier");
		int bufferCountNeededForRollingshutter = (int)(ceil(mme_rollingShutterMultiplier) + 0.5f); // ceil bc if value is 1.1 we need 2 buffers. +.5 to avoid float issues..
		int rollingShutterFactor = cgs.glconfig.vidHeight / mme_rollingShutterPixels;
		float captureFPS = fps * (float)rollingShutterFactor / mme_rollingShutterMultiplier;

		int frameAdvanceCount = 0;
		while (((float)demo.play.time + demo.play.fraction) < desiredTime) {
			float frameDelay = 1000.0f / captureFPS;
			demo.play.fraction += frameDelay * demo.play.speed;
			demo.play.time += (int)demo.play.fraction;
			demo.play.fraction -= (int)demo.play.fraction;
			frameAdvanceCount++;
		}
		trap_MME_FakeAdvanceFrame(frameAdvanceCount);
	}
}
#ifdef RELDEBUG
#pragma optimize("", on)
#endif

// seek by server time, not demotime
static void demoSeekThreeCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (isdigit( cmd[0] )) {
		//teh's parser for time MM:SS.MSEC, thanks *bow*
		int i;
		char *sec, *min;;
		min = (char *)cmd;
		for( i=0; min[i]!=':'&& min[i]!=0; i++ );
		if(cmd[i]==0)
			sec = 0;
		else
		{
			min[i] = 0;
			sec = min+i+1;
		}
		int quickDelta = cg.snap->serverTime - demo.play.time;
		demo.play.time = ( atoi( min ) * 60000 + ( sec ? atof( sec ) : 0 ) * 1000 ) - quickDelta;
		demo.play.fraction = 0;
	}
}

// seek by game time, not demotime
static void demoSeekFourCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (isdigit( cmd[0] )) {
		//teh's parser for time MM:SS.MSEC, thanks *bow*
		int i;
		char *sec, *min;;
		min = (char *)cmd;
		for( i=0; min[i]!=':'&& min[i]!=0; i++ );
		if(cmd[i]==0)
			sec = 0;
		else
		{
			min[i] = 0;
			sec = min+i+1;
		}
		int gameTime = cg.time - cgs.levelStartTime;
		int quickDelta = gameTime - demo.play.time; // TODO make this handle timeline.
		demo.play.time = ( atoi( min ) * 60000 + ( sec ? atof( sec ) : 0 ) * 1000 ) - quickDelta;
		demo.play.fraction = 0;
	}
}

static void demoSeekCommand_f(void) {
	const char *cmd = CG_Argv(1);
	if (cmd[0] == '+') {
		if (isdigit( cmd[1])) {
			demo.play.time += atof( cmd + 1 ) * 1000;
			demo.play.fraction = 0;
		}
	} else if (cmd[0] == '-') {
		if (isdigit( cmd[1])) {
			demo.play.time -= atof( cmd + 1 ) * 1000;
			demo.play.fraction = 0;
		}
	} else if (isdigit( cmd[0] )) {
		demo.play.time = atof( cmd ) * 1000;
		demo.play.fraction = 0;
	}
}

static void musicPlayCommand_f(void) {
	float length = 2;
	int musicStart;

	if ( trap_Argc() > 1 ) {
		length = atof( CG_Argv( 1 ) );
		if ( length <= 0)
			length = 2;
	}
	musicStart = (demo.play.time - mov_musicStart.value * 1000 );
	demoSynchMusic( musicStart, length );
}

static void stopLoopingSounds_f(void) {
	int i;
	for (i = 0; i < MAX_GENTITIES; i++)
		trap_S_StopLoopingSound(i);
}

static void demoFindCommand_f(void) {
	const char *cmd = CG_Argv(1);

	if (!Q_stricmp(cmd, "death")) {
		demo.find = findObituary;
	} else if (!Q_stricmp(cmd, "direct")) {
		demo.find = findDirect;
	} else{
		demo.find = findNone;
	}
	if ( demo.find )
		demo.play.paused = qfalse;
}

void demoPlaybackInit(void) {	
	char projectFile[MAX_OSPATH];

	demo.initDone = qtrue;
	demo.autoLoad = qfalse;
	demo.play.time = 0;
	demo.play.lastTime = 0;
	demo.play.fraction = 0;
	demo.play.speed = 1.0f;
	demo.play.paused = qfalse;

	demo.move.acceleration = 8;
	demo.move.friction = 8;
	demo.move.speed = 400;

	demo.line.locked = qfalse;
	demo.line.offset = 0;
	demo.line.speed = 1.0f;
	demo.line.points = 0;

	demo.loop.total = 0;

	demo.editType = editCamera;
	demo.viewType = viewChase;

	demo.camera.flags = CAM_ORIGIN | CAM_ANGLES;
	demo.camera.target = -1;
	demo.camera.fov = 0;
	demo.camera.smoothPos = posBezier;
	demo.camera.smoothAngles = angleQuat;

	VectorClear( demo.chase.origin );
	VectorClear( demo.chase.angles );
	VectorClear( demo.chase.velocity );
	demo.chase.distance = 0;
	demo.chase.locked = qfalse;
	demo.chase.target = -1;
	
	demo.dof.focus = 256.0f;
	demo.dof.radius = 5.0f;
	demo.dof.target = -1;

	demo.sun.active = qfalse;
	demo.sun.size = 1.0f;
	demo.sun.precision = 10.0f;
	demo.sun.angles[YAW] = 45.0f;
	demo.sun.angles[PITCH] = -45.0f;
	demo.sun.angles[ROLL] = 0.0f;

	demo.rain.active = qfalse;
	demo.rain.number = 100;
	demo.rain.range = 1000.0f;
	demo.rain.back = qfalse;

	hudInitTables();
	demoSynchMusic( -1, 0 );

	trap_AddCommand("camera");
	trap_AddCommand("edit");
	trap_AddCommand("view");
	trap_AddCommand("chase");
	trap_AddCommand("dof");
	trap_AddCommand("speed");
	trap_AddCommand("seek");
	trap_AddCommand("demoSeek");
	trap_AddCommand("demoSeekSync");
	trap_AddCommand("demoSeekGame");
	trap_AddCommand("demoSeekAbs");
	trap_AddCommand("find");
	trap_AddCommand("pause");
	trap_AddCommand("capture");
	trap_AddCommand("hudInit");
	trap_AddCommand("hudToggle");
	trap_AddCommand("line");
	trap_AddCommand("commands");
	trap_AddCommand("save");
	trap_AddCommand("load");
	trap_AddCommand("+seek");
	trap_AddCommand("-seek");
	trap_AddCommand("clientOverride");
	trap_AddCommand("musicPlay");
	trap_AddCommand("stopLoop");
	trap_AddCommand("sun");
	trap_AddCommand("rain");
	trap_AddCommand("cut");

	demo.media.additiveWhiteShader = trap_R_RegisterShader( "mme_additiveWhite" );
	demo.media.mouseCursor = trap_R_RegisterShaderNoMip( "cursor" );
	demo.media.switchOn = trap_R_RegisterShaderNoMip( "mme_message_on" );
	demo.media.switchOff = trap_R_RegisterShaderNoMip( "mme_message_off" );
	
	// Weather
	demo.media.heavyRain = trap_S_RegisterSound("sound/ambient/rain_hard");
	demo.media.regularRain = trap_S_RegisterSound("sound/ambient/rain_mid");
	demo.media.lightRain = trap_S_RegisterSound("sound/ambient/rain_light");

	trap_SetUserCmdValue( 0.0f, 1.0f, 0.0f, 0.0f );

	trap_SendConsoleCommand("exec mmedemos.cfg\n");
//	trap_Cvar_Set( "mov_captureName", "" );
	trap_Cvar_VariableStringBuffer( "mme_demoStartProject", projectFile, sizeof( projectFile ));
	if (projectFile[0]) {
		trap_Cvar_Set( "mme_demoStartProject", "" );
		demo.autoLoad = demoProjectLoad( projectFile );
		if (demo.autoLoad) {
			if (!demo.capture.start && !demo.capture.end) {
				trap_Error( "Loaded project file with empty capture range\n");
			}
			/* Check if the project had a cvar for the name else use project */
			if (!mov_captureName.string[0]) {
				trap_Cvar_Set( "mov_captureName", projectFile );
				trap_Cvar_Update( &mov_captureName );
			}
			trap_SendConsoleCommand("exec mmelist.cfg\n");
			demo.play.time = demo.capture.start - 1000;
			demo.capture.locked = qtrue;
			demo.capture.active = qtrue;
		} else {
			trap_Error( va("Couldn't load project %s\n", projectFile ));
		}
	}
}

void CG_DemoEntityEvent( const centity_t* cent ) {
	switch ( cent->currentState.event ) {
	case EV_OBITUARY:
		if ( demo.find == findObituary ) {
			demo.play.paused = qtrue;
			demo.find = findNone;
		} else if ( demo.find == findDirect ) {
			int mod = cent->currentState.eventParm;
			switch (mod) {
			case MOD_BRYAR_PISTOL:
			case MOD_BRYAR_PISTOL_ALT:
			case MOD_BLASTER:
			case MOD_BOWCASTER:
			case MOD_REPEATER:
			case MOD_REPEATER_ALT:
			case MOD_DEMP2:
			case MOD_FLECHETTE:
			case MOD_ROCKET:
			case MOD_ROCKET_HOMING:
			case MOD_THERMAL:
				demo.play.paused = qtrue;
				demo.find = findNone;
			break;
			}
		}
		break;
	}
}

// Nerevar's way to produce dismemberment
void CG_DemoDismembermentEvent( centity_t *cent, vec3_t position ) {
	entityState_t *es = &cent->currentState;
	if (!mov_dismember.integer)
		return;
	switch (es->event) {
		case EV_OBITUARY:
			{
				int meansOfDeath = es->eventParm;

				vec3_t dir;
				int target = es->otherEntityNum;
				int attacker = es->otherEntityNum2;
				//int mod = es->eventParm;
				centity_t* targetent = &cg_entities[target];

				cg_entities[target].dism.lastkiller = attacker;
				cg_entities[target].dism.deathtime = cg.time;

				VectorCopy(targetent->currentState.pos.trDelta, dir);

				dir[0] = dir[0] * (0.8 + random() * 0.4);
				dir[1] = dir[1] * (0.8 + random() * 0.4);
				dir[2] = dir[2] * (0.8 + random() * 0.4);
				demoSaberDismember(targetent, dir);

#ifdef GIB
				// Gib on explosive deaths
				if (meansOfDeath == MOD_FLECHETTE ||
					meansOfDeath == MOD_FLECHETTE_ALT_SPLASH ||
					meansOfDeath == MOD_ROCKET ||
					meansOfDeath == MOD_ROCKET_SPLASH ||
					meansOfDeath == MOD_ROCKET_HOMING ||
					meansOfDeath == MOD_ROCKET_HOMING_SPLASH ||
					meansOfDeath == MOD_THERMAL ||
					meansOfDeath == MOD_THERMAL_SPLASH ||
					meansOfDeath == MOD_TRIP_MINE_SPLASH ||
					meansOfDeath == MOD_TIMED_MINE_SPLASH ||
					meansOfDeath == MOD_DET_PACK_SPLASH) {

					int target = es->otherEntityNum;
					//int attacker = es->otherEntityNum2;
					//int mod = es->eventParm;
					centity_t* targetent = &cg_entities[target];

					trap_S_StartSound(NULL, es->number, CHAN_BODY, cgs.media.gibSound);
					
					VectorScale(dir, cg_gibDirectional.value, dir);
					
					CG_GibPlayer(cent->lerpOrigin, dir);
					cent->isGibbing = qtrue;
					// Make them all invisible. lol
					// head,torso,l_arm,r_arm,l_hand,r_hand,l_leg,r_leg
					//trap_G2API_SetSurfaceOnOff(cg_entities[es->number].ghoul2, "head", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "hips", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "head", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "torso", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "l_arm", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "r_arm", 0x00000100);
					//trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "l_hand", 0x00000100);
					//trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "r_hand", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "l_leg", 0x00000100);
					trap_G2API_SetSurfaceOnOff(targetent->ghoul2, "r_leg", 0x00000100);
					cg_entities[targetent->currentState.number].dism.cut[DISM_HEAD] =
						//cg_entities[targetent->currentState.number].dism.cut[DISM_LHAND] =
						//cg_entities[targetent->currentState.number].dism.cut[DISM_RHAND] =
						cg_entities[targetent->currentState.number].dism.cut[DISM_LARM] =
						cg_entities[targetent->currentState.number].dism.cut[DISM_RARM] =
						cg_entities[targetent->currentState.number].dism.cut[DISM_LLEG] =
						cg_entities[targetent->currentState.number].dism.cut[DISM_RLEG] =
						cg_entities[targetent->currentState.number].dism.cut[DISM_WAIST] =
						//cg_entities[targetent->currentState.number].dism.cut[DISM_TOTAL] = 
							qtrue;
					cg_entities[targetent->currentState.number].torsoBolt = 1;
					cg_entities[targetent->currentState.number].ghoul2weapon = NULL;
					//trap_G2API_SetSurfaceOnOff(cent->ghoul2, stubCapName, 0);
				}
#endif
					
			}
			break;
		case EV_SABER_HIT:
			if (es->eventParm) //Hit a person
				demoCheckDismember(es->origin);
			break;
		default:
			break;
	}	
}
void CG_DemoDismembermentPlayer( centity_t *cent ) {
	demoPlayerDismember(cent);
}

qboolean CG_DemosConsoleCommand( void ) {
	const char *cmd = CG_Argv(0);
	if (!Q_stricmp(cmd, "camera")) {
		demoCameraCommand_f();
	} else if (!Q_stricmp(cmd, "view")) {
		demoViewCommand_f();
	} else if (!Q_stricmp(cmd, "edit")) {
		demoEditCommand_f();
	} else if (!Q_stricmp(cmd, "capture")) {
		demoCaptureCommand_f();
	} else if (!Q_stricmp(cmd, "seek")) {
		demoSeekCommand_f();
	} else if (!Q_stricmp(cmd, "demoSeek")) {
		demoSeekTwoCommand_f();
	} else if (!Q_stricmp(cmd, "demoSeekSync")) {
		demoSeekSyncCommand_f();
	} else if (!Q_stricmp(cmd, "demoSeekAbs")) {
		demoSeekThreeCommand_f();
	} else if (!Q_stricmp(cmd, "demoSeekGame")) {
		demoSeekFourCommand_f();
	} else if (!Q_stricmp(cmd, "find")) {
		demoFindCommand_f();
	} else if (!Q_stricmp(cmd, "speed")) {
		cmd = CG_Argv(1);
		if (cmd[0]) {
			demo.play.speed = atof(cmd);
		}
		CG_DemosAddLog("Play speed %f", demo.play.speed );
	} else if (!Q_stricmp(cmd, "pause")) {
		demo.play.paused = (qboolean) !demo.play.paused;
		if ( demo.play.paused )
			demo.find = findNone;
	} else if (!Q_stricmp(cmd, "dof")) {
		demoDofCommand_f();
	} else if (!Q_stricmp(cmd, "chase")) {
		demoChaseCommand_f();
	} else if (!Q_stricmp(cmd, "hudInit")) {
		hudInitTables();
	} else if (!Q_stricmp(cmd, "hudToggle")) {
		hudToggleInput();
	} else if (!Q_stricmp(cmd, "+seek")) {
		demo.seekEnabled = qtrue;
	} else if (!Q_stricmp(cmd, "-seek")) {
		demo.seekEnabled = qfalse;
	} else if (!Q_stricmp(cmd, "line")) {
		demoLineCommand_f();
	} else if (!Q_stricmp(cmd, "commands")) {
		demoCommandsCommand_f();
	} else if (!Q_stricmp(cmd, "load")) {
		demoLoadCommand_f();
	} else if (!Q_stricmp(cmd, "save")) {
		demoSaveCommand_f();
	} else if (!Q_stricmp(cmd, "clientOverride")) {
		CG_ClientOverride_f();
	} else if (!Q_stricmp(cmd, "musicPlay")) {
		musicPlayCommand_f();
	} else if (!Q_stricmp(cmd, "stopLoop")) {
		stopLoopingSounds_f();
	} else if (!Q_stricmp(cmd, "sun")) {
		demoSunCommand_f();
	} else if (!Q_stricmp(cmd, "rain")) {
		demoRainCommand_f();
	} else if (!Q_stricmp(cmd, "cut")) {
		demoCutCommand_f();
	} else {
		return CG_ConsoleCommand();
	}
	return qtrue;
}
