// Copyright (C) 1999-2000 Id Software, Inc.
//

// cg_localents.c -- every frame, generate renderer commands for locally
// processed entities, like smoke puffs, gibs, shells, etc.

#include "cg_local.h"

#define	MAX_LOCAL_ENTITIES	2048 //ent: Raz: was 512
localEntity_t	cg_localEntities[MAX_LOCAL_ENTITIES];
localEntity_t	cg_activeLocalEntities;		// double linked list
localEntity_t	*cg_freeLocalEntities;		// single linked list

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
void	CG_InitLocalEntities( void ) {
	int		i;

	memset( cg_localEntities, 0, sizeof( cg_localEntities ) );
	cg_activeLocalEntities.next = &cg_activeLocalEntities;
	cg_activeLocalEntities.prev = &cg_activeLocalEntities;
	cg_freeLocalEntities = cg_localEntities;
	for ( i = 0 ; i < MAX_LOCAL_ENTITIES - 1 ; i++ ) {
		cg_localEntities[i].next = &cg_localEntities[i+1];
	}
}


/*
==================
CG_FreeLocalEntity
==================
*/
void CG_FreeLocalEntity( localEntity_t *le ) {
	if ( !le->prev ) {
		CG_Error( "CG_FreeLocalEntity: not active" );
	}

	// remove from the doubly linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// the free list is only singly linked
	le->next = cg_freeLocalEntities;
	cg_freeLocalEntities = le;
}

/*
===================
CG_AllocLocalEntity

Will allways succeed, even if it requires freeing an old active entity
===================
*/
localEntity_t	*CG_AllocLocalEntity( void ) {
	localEntity_t	*le;

	if ( !cg_freeLocalEntities ) {
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		CG_FreeLocalEntity( cg_activeLocalEntities.prev );
	}

	le = cg_freeLocalEntities;
	cg_freeLocalEntities = cg_freeLocalEntities->next;

	memset( le, 0, sizeof( *le ) );

	// link into the active list
	le->next = cg_activeLocalEntities.next;
	le->prev = &cg_activeLocalEntities;
	cg_activeLocalEntities.next->prev = le;
	cg_activeLocalEntities.next = le;
	return le;
}


/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
void CG_BloodTrail( localEntity_t *le ) {
	if (1) { // I dont understand why this was set to !mov_dismebmer??!
		int		t;
		int		t2;
		int		step;
		vec3_t	newOrigin;
		localEntity_t	*blood;

		step = 150;
		t = step * ( (cg.time - cg.frametime + step ) / step );
		t2 = step * ( cg.time / step );

		for ( ; t <= t2; t += step ) {
			BG_EvaluateTrajectory( &le->pos, t, newOrigin );

			blood = CG_SmokePuff( newOrigin, vec3_origin, 
						  20,		// radius
						  1, 1, 1, 1,	// color
						  2000,		// trailTime
						  t,		// startTime
						  0,		// fadeInTime
						  0,		// flags
						  cgs.media.bloodTrailShader );
			// use the optimized version
			blood->leType = LE_FALL_SCALE_FADE;
			// drop a total of 40 units over its lifetime
			blood->pos.trDelta[2] = 40;
		}
	} 
	
	if(mov_dismember.integer){
		int newBolt;
		char *limbTagName;
	
		if (le->limbpart == DISM_HEAD) {
			limbTagName = "*head_cap_torso";
		} else if (le->limbpart == DISM_WAIST) {
			limbTagName = "*torso_cap_hips";
		} else if (le->limbpart == DISM_LARM) {
			limbTagName = "*l_arm_cap_torso";
		} else if (le->limbpart == DISM_RARM) {
			limbTagName = "*r_arm_cap_torso";
		} else if (le->limbpart == DISM_LHAND) {
			limbTagName = "*l_hand_cap_l_arm";
		} else if (le->limbpart == DISM_RHAND) {
			limbTagName = "*r_hand_cap_r_arm";
		} else if (le->limbpart == DISM_LLEG) {
			limbTagName = "*l_leg_cap_hips";
		}	else {
			limbTagName = "*r_leg_cap_hips";
		}	

	
		newBolt = trap_G2API_AddBolt( le->refEntity.ghoul2, 0, limbTagName );
		if ( newBolt != -1 ) {
			vec3_t boltOrg, boltAng;
			mdxaBone_t			matrix;

			trap_G2API_GetBoltMatrix(le->refEntity.ghoul2, 0, newBolt, &matrix, le->refEntity.angles, le->refEntity.origin, cg.time, cgs.gameModels, le->refEntity.modelScale);

			trap_G2API_GiveMeVectorFromMatrix(&matrix, ORIGIN, boltOrg);
			trap_G2API_GiveMeVectorFromMatrix(&matrix, NEGATIVE_Y, boltAng);

			trap_FX_PlayEffectID(trap_FX_RegisterEffect("smoke_bolton"), boltOrg, boltAng);
		}
	}
}


/*
================
CG_FragmentBounceMark
================
*/
void CG_FragmentBounceMark( localEntity_t *le, trace_t *trace ) {
	int			radius;

	if ( le->leMarkType == LEMT_BLOOD ) {

		radius = 16 + (rand()&31);
		CG_ImpactMark( cgs.media.bloodMarkShader, trace->endpos, trace->plane.normal, random()*360,
			1,1,1,1, qtrue, radius, qfalse, qfalse );
	} else if ( le->leMarkType == LEMT_BURN ) {

		radius = 8 + (rand()&15);
		CG_ImpactMark( cgs.media.burnMarkShader, trace->endpos, trace->plane.normal, random()*360,
			1,1,1,1, qtrue, radius, qfalse, qfalse);
	}


	// don't allow a fragment to make multiple marks, or they
	// pile up while settling
	le->leMarkType = LEMT_NONE;
}

#define SABERBOUNCETIME 400   //time between LE bounce sounds 
#define GIBBOUNCETIME 300
/*
================
CG_FragmentBounceSound
================
*/
void CG_FragmentBounceSound( localEntity_t *le, trace_t *trace ) {
	if (cg_gibs.integer) {
		if ( le->leBounceSoundType == LEBS_BLOOD ) {
			// half the gibs will make splat sounds
#ifdef GIB
			if ( rand() & 1 ) {
				int r = rand()&3;
				sfxHandle_t	s;

				if ( r == 0 ) {
					s = cgs.media.gibBounce1Sound;
				} else if ( r == 1 ) {
					s = cgs.media.gibBounce2Sound;
				} else {
					s = cgs.media.gibBounce3Sound;
				}
				trap_S_StartSound( trace->endpos, ENTITYNUM_WORLD, CHAN_AUTO, s );
			
			}
#endif
			
		} else if ( le->leBounceSoundType == LEBS_BRASS ) {

		}

		// don't allow a fragment to make multiple bounce sounds,
		// or it gets too noisy as they settle
		le->leBounceSoundType = LEBS_NONE;
	} 
	if(mov_dismember.integer){
		sfxHandle_t s = -1;
	
		if ( le->leFragmentType == LEFT_GIB) {
			if (le->limbpart == DISM_WAIST) {   ///WAIST
				int r = rand()&3;

				if ( r == 0 ) {
					s = trap_S_RegisterSound("sound/player/bodyfall_human1.wav");
				} else if ( r == 1 ) {
					s = trap_S_RegisterSound("sound/player/bodyfall_human2.wav");
				} else {
					s = trap_S_RegisterSound("sound/player/bodyfall_human3.wav");
				}
			
			} else if (le->limbpart == DISM_RARM) { ///////ARM
				s = trap_S_RegisterSound("sound/effects/desann_hand.wav");	
			} else {
				s = trap_S_RegisterSound("sound/movers/objects/objecthit.wav");
			}
		
			le->bouncetime = cg.time + GIBBOUNCETIME;
		} else if ( le->leFragmentType == LEFT_SABER ) {
			int r = rand()&3;

			if ( r == 0 ) {
				s = trap_S_RegisterSound("sound/weapons/saber/bounce1.wav");
			} else if ( r == 1 ) {
				s = trap_S_RegisterSound("sound/weapons/saber/bounce2.wav");
			} else {
				s = trap_S_RegisterSound("sound/weapons/saber/bounce3.wav");
			}		
		
			le->bouncetime = cg.time + SABERBOUNCETIME;
		}
		if (s != -1) trap_S_StartSound( trace->endpos, ENTITYNUM_WORLD, CHAN_BODY, s );
	}
}


/*
================
CG_ReflectVelocity
================
*/
void CG_ReflectVelocity( localEntity_t *le, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = cg.time - cg.frametime + cg.frametime * trace->fraction;
	BG_EvaluateTrajectoryDelta( &le->pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, le->pos.trDelta );

	VectorScale( le->pos.trDelta, le->bounceFactor, le->pos.trDelta );

	VectorCopy( trace->endpos, le->pos.trBase );
	le->pos.trTime = cg.time;

	// check for stop, making sure that even on low FPS systems it doesn't bobble
	if ( trace->allsolid || 
		( trace->plane.normal[2] > 0 && 
		( le->pos.trDelta[2] < 40 || le->pos.trDelta[2] < -cg.frametime * le->pos.trDelta[2] ) ) ) {
		le->pos.trType = TR_STATIONARY;
		
		if (!mov_dismember.integer)
			return;		
		if (le->leFragmentType == LEFT_SABER) {
			le->angles.trBase[0] = 90;
		} else if (le->leFragmentType == LEFT_GIB) {
			if (le->limbpart == DISM_WAIST) {
				le->angles.trBase[0] = le->angles.trBase[2] = 0;
			} else if (le->limbpart >= DISM_LHAND && le->limbpart <= DISM_RARM) {
				le->angles.trBase[0] = 0;			
			} else if (le->limbpart == DISM_LLEG || le->limbpart == DISM_RLEG) {
				le->angles.trBase[0] = 0;
				le->angles.trBase[2] = 0;		
			}
		}
		AnglesToAxis(le->angles.trBase, le->refEntity.axis);
	} else {

	}
}

/*
================
CG_AddFragment
================
*/
void CG_AddFragment( localEntity_t *le ) {
	vec3_t	newOrigin;
	trace_t	trace;

	if (le->forceAlpha) {
		le->refEntity.renderfx |= RF_FORCE_ENT_ALPHA;
		le->refEntity.shaderRGBA[3] = le->forceAlpha;
	}

	if ( le->pos.trType == TR_STATIONARY ) {
		// sink into the ground if near the removal time
		float t, t_e;
		
		t = (le->endTime - cg.time) - cg.timeFraction;
		if ( t < (SINK_TIME*2) ) {
			le->refEntity.renderfx |= RF_FORCE_ENT_ALPHA;
			t_e = (float)(t/(SINK_TIME*2));
			t_e = (int)((t_e)*255);

			if (t_e > 255) {
				//t_e = 255;
			} else if (t_e < 1) {
				//t_e = 1;
			}

			if (le->refEntity.shaderRGBA[3] && t_e > le->refEntity.shaderRGBA[3]) {
				t_e = le->refEntity.shaderRGBA[3];
			}

			le->refEntity.shaderRGBA[3] = t_e;

			trap_R_AddRefEntityToScene( &le->refEntity );
		} else {
			trap_R_AddRefEntityToScene( &le->refEntity );
		}

		return;
	}
	
	if (mov_dismember.integer && le->leFragmentType == LEFT_GIB)
		CG_BloodTrail( le );
	
	// calculate new position
	demoNowTrajectory( &le->pos, newOrigin );

	// trace a line from previous position to new position
	CG_Trace( &trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID );
	if ( trace.fraction == 1.0 ) {
		// still in free fall
		VectorCopy( newOrigin, le->refEntity.origin );

		if ( le->leFlags & LEF_TUMBLE ) {
			vec3_t angles;

			demoNowTrajectory( &le->angles, angles );
			AnglesToAxis( angles, le->refEntity.axis );
		}

		if ( mov_dismember.integer && le->leFragmentType == LEFT_GIB ) {
			le->refEntity.origin[2] += 8;
		} else if ( mov_dismember.integer && le->leFragmentType == LEFT_SABER ) {
			le->refEntity.origin[2] += 1;
		}
		trap_R_AddRefEntityToScene( &le->refEntity );

		// add a blood trail
		if ( le->leBounceSoundType == LEBS_BLOOD ) {
			CG_BloodTrail( le );
		}

		return;
	}

	// if it is in a nodrop zone, remove it
	// this keeps gibs from waiting at the bottom of pits of death
	// and floating levels
	if ( trap_CM_PointContents( trace.endpos, 0 ) & CONTENTS_NODROP ) {
		CG_FreeLocalEntity( le );
		return;
	}

	if (!trace.startsolid) {
		if (!mov_dismember.integer) { // TODO Weird.
			// leave a mark
			CG_FragmentBounceMark( le, &trace );
			// do a bouncy sound
			CG_FragmentBounceSound( le, &trace );
		} else {
			le->angles.trDelta[1] /= (1+random()*0.2);
			le->angles.trDelta[2] /= (1+random()*0.2);
		
			if (le->leFragmentType == LEFT_SABER) {
				le->angles.trBase[0] = ((le->angles.trBase[0]-90)/2)+90;
				le->angles.trDelta[0] /= 2;
			} else {
				if (le->limbpart == DISM_WAIST) {
					le->angles.trBase[0] /= 5;
					le->angles.trDelta[0] /= 5;
					le->angles.trBase[1] /= 2;
					le->angles.trDelta[1] /= 2;
					le->angles.trBase[2] /= 5;
					le->angles.trDelta[2] /= 5;
				} else if (le->limbpart >= DISM_LHAND && le->limbpart <= DISM_RARM) {
					le->angles.trBase[0] /= 5;
					le->angles.trDelta[0] /= 5;	
					le->angles.trBase[1] /= 2;
					le->angles.trDelta[1] /= 2;
					le->angles.trBase[2] /= 2;
					le->angles.trDelta[2] /= 2;	
				} else if (le->limbpart == DISM_LLEG || le->limbpart == DISM_RLEG) {
					le->angles.trBase[0] /= 5;
					le->angles.trDelta[0] /= 5;	
					le->angles.trBase[1] /= 2;
					le->angles.trDelta[1] /= 2;
					le->angles.trBase[2] = ((le->angles.trBase[2]-90)/5)+90;
					le->angles.trDelta[2] /= 5;
				}	else {
					le->angles.trBase[0] /= 2;
					le->angles.trDelta[0] /= 2;	
					le->angles.trBase[1] /= 2;
					le->angles.trDelta[1] /= 2;
					le->angles.trBase[2] /= 2;
					le->angles.trDelta[2] /= 2;				
				}

			}			
			AnglesToAxis(le->angles.trBase, le->refEntity.axis);
			// do a bouncy sound
			if (le->bouncetime < cg.time) {
				CG_FragmentBounceSound( le, &trace );
			}
		}

		if (le->bounceSound) { //specified bounce sound (debris)
			trap_S_StartSound(le->pos.trBase, ENTITYNUM_WORLD, CHAN_AUTO, le->bounceSound);
		}

		// reflect the velocity on the trace plane
		CG_ReflectVelocity( le, &trace );

		trap_R_AddRefEntityToScene( &le->refEntity );
	}
}

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB( localEntity_t *le ) {
	refEntity_t *re;
	float c;

	re = &le->refEntity;

	c = ((le->endTime - cg.time) - cg.timeFraction) * le->lifeRate;
	c *= 0xff;

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;
	re->shaderRGBA[3] = le->color[3] * c;

	trap_R_AddRefEntityToScene( re );
}

static void CG_AddFadeScaleModel( localEntity_t *le )
{
	refEntity_t	*ent = &le->refEntity;

	float frac = (( cg.time - le->startTime ) + cg.timeFraction)/((float)( le->endTime - le->startTime ));

	frac *= frac * frac; // yes, this is completely ridiculous...but it causes the shell to grow slowly then "explode" at the end

	ent->nonNormalizedAxes = qtrue;

	AxisCopy( axisDefault, ent->axis );

	VectorScale( ent->axis[0], le->radius * frac, ent->axis[0] );
	VectorScale( ent->axis[1], le->radius * frac, ent->axis[1] );
	VectorScale( ent->axis[2], le->radius * 0.5f * frac, ent->axis[2] );

	frac = 1.0f - frac;

	ent->shaderRGBA[0] = le->color[0] * frac;
	ent->shaderRGBA[1] = le->color[1] * frac;
	ent->shaderRGBA[2] = le->color[2] * frac;
	ent->shaderRGBA[3] = le->color[3] * frac;

	// add the entity
	trap_R_AddRefEntityToScene( ent );
}

/*
==================
CG_AddMoveScaleFade
==================
*/
static void CG_AddMoveScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	if ( le->fadeInTime > le->startTime && cg.time < le->fadeInTime ) {
		// fade / grow time
		c = 1.0f - (( le->fadeInTime - cg.time) - cg.timeFraction) / ( le->fadeInTime - le->startTime );
	}
	else {
		// fade / grow time
		c = (( le->endTime - cg.time) - cg.timeFraction) * le->lifeRate;
	}

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	if ( !( le->leFlags & LEF_PUFF_DONT_SCALE ) ) {
		re->radius = le->radius * ( 1.0 - c ) + 8;
	}

	demoNowTrajectory( &le->pos, re->origin );

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}

/*
==================
CG_AddPuff
==================
*/
static void CG_AddPuff( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	// fade / grow time
	c = ((le->endTime - cg.time) - cg.timeFraction) / (le->endTime - le->startTime);

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;

	if ( !( le->leFlags & LEF_PUFF_DONT_SCALE ) ) {
		re->radius = le->radius * ( 1.0 - c ) + 8;
	}

	demoNowTrajectory(&le->pos, re->origin);

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}

/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	// fade / grow time
	c = ((le->endTime - cg.time) - cg.timeFraction) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];
	re->radius = le->radius * ( 1.0 - c ) + 8;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}


/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade( localEntity_t *le ) {
	refEntity_t	*re;
	float		c;
	vec3_t		delta;
	float		len;

	re = &le->refEntity;

	// fade time
	c = ((le->endTime - cg.time) - cg.timeFraction) * le->lifeRate;

	re->shaderRGBA[3] = 0xff * c * le->color[3];

	re->origin[2] = le->pos.trBase[2] - ( 1.0 - c ) * le->pos.trDelta[2];

	re->radius = le->radius * ( 1.0 - c ) + 16;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( re->origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < le->radius ) {
		CG_FreeLocalEntity( le );
		return;
	}

	trap_R_AddRefEntityToScene( re );
}



/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion( localEntity_t *ex ) {
	refEntity_t	*ent;

	ent = &ex->refEntity;

	// add the entity
	trap_R_AddRefEntityToScene(ent);

	// add the dlight
	if ( ex->light ) {
		float light = (float)((cg.time - ex->startTime) + cg.timeFraction) / (ex->endTime - ex->startTime);

		if ( light < 0.5 ) {
			light = 1.0;
		} else {
			light = 1.0 - ( light - 0.5 ) * 2;
		}
		light = ex->light * light;
		trap_R_AddLightToScene(ent->origin, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2] );
	}
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion( localEntity_t *le ) {
	refEntity_t	re;
	float c;

	re = le->refEntity;

	c = ((le->endTime - cg.time) - cg.timeFraction) / (float)(le->endTime - le->startTime);
	if ( c > 1 ) {
		c = 1.0;	// can happen during connection problems
	}

	re.shaderRGBA[0] = 0xff;
	re.shaderRGBA[1] = 0xff;
	re.shaderRGBA[2] = 0xff;
	re.shaderRGBA[3] = 0xff * c * 0.33;

	re.reType = RT_SPRITE;
	re.radius = 42 * ( 1.0 - c ) + 30;

	trap_R_AddRefEntityToScene( &re );

	// add the dlight
	if ( le->light ) {
		float light = (( cg.time - le->startTime ) + cg.timeFraction) / ( le->endTime - le->startTime );

		if ( light < 0.5 ) {
			light = 1.0;
		} else {
			light = 1.0 - ( light - 0.5 ) * 2;
		}
		light = le->light * light;
		trap_R_AddLightToScene(re.origin, light, le->lightColor[0], le->lightColor[1], le->lightColor[2] );
	}
}


/*
===================
CG_AddRefEntity
===================
*/
void CG_AddRefEntity( localEntity_t *le ) {
	if (le->endTime < cg.time) {
		CG_FreeLocalEntity( le );
		return;
	}
	trap_R_AddRefEntityToScene( &le->refEntity );
}

/*
===================
CG_AddScorePlum
===================
*/
#define NUMBER_SIZE		8

void CG_AddScorePlum( localEntity_t *le ) {
	refEntity_t	*re;
	vec3_t		origin, delta, dir, vec, up = {0, 0, 1};
	float		c, len;
	int			i, score, digits[10], numdigits, negative;

	re = &le->refEntity;

	c = ((le->endTime - cg.time) - cg.timeFraction ) * le->lifeRate;

	score = le->radius;
	if (score < 0) {
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0x11;
		re->shaderRGBA[2] = 0x11;
	}
	else {
		re->shaderRGBA[0] = 0xff;
		re->shaderRGBA[1] = 0xff;
		re->shaderRGBA[2] = 0xff;
		if (score >= 50) {
			re->shaderRGBA[1] = 0;
		} else if (score >= 20) {
			re->shaderRGBA[0] = re->shaderRGBA[1] = 0;
		} else if (score >= 10) {
			re->shaderRGBA[2] = 0;
		} else if (score >= 2) {
			re->shaderRGBA[0] = re->shaderRGBA[2] = 0;
		}

	}
	if (c < 0.25)
		re->shaderRGBA[3] = 0xff * 4 * c;
	else
		re->shaderRGBA[3] = 0xff;

	re->radius = NUMBER_SIZE / 2;

	VectorCopy(le->pos.trBase, origin);
	origin[2] += 110 - c * 100;

	VectorSubtract(cg.refdef.vieworg, origin, dir);
	CrossProduct(dir, up, vec);
	VectorNormalize(vec);

	VectorMA(origin, -10 + 20 * sin(c * 2 * M_PI), vec, origin);

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract( origin, cg.refdef.vieworg, delta );
	len = VectorLength( delta );
	if ( len < 7*7 ) {
//		CG_FreeLocalEntity( le );
		return;
	}

	negative = qfalse;
	if (score < 0) {
		negative = qtrue;
		score = -score;
	}

	for (numdigits = 0; !(numdigits && !score); numdigits++) {
		digits[numdigits] = score % 10;
		score = score / 10;
	}

	if (negative) {
		digits[numdigits] = 10;
		numdigits++;
	}

	for (i = 0; i < numdigits; i++) {
		VectorMA(origin, (float) (((float) numdigits / 2) - i) * NUMBER_SIZE, vec, re->origin);
		re->customShader = cgs.media.numberShaders[digits[numdigits-1-i]];
		trap_R_AddRefEntityToScene( re );
	}
}

/*
===================
CG_AddOLine

For forcefields/other rectangular things
===================
*/
void CG_AddOLine( localEntity_t *le )
{
	refEntity_t	*re;
	float		frac, alpha;

	re = &le->refEntity;

	frac = ((cg.time - le->startTime) + cg.timeFraction) / (float) (le->endTime - le->startTime);
	if ( frac > 1 ) 
		frac = 1.0;	// can happen during connection problems
	else if (frac < 0)
		frac = 0.0;

	// Use the liferate to set the scale over time.
	re->data.line.width = le->data.line.width + (le->data.line.dwidth * frac);
	if (re->data.line.width <= 0)
	{
		CG_FreeLocalEntity( le );
		return;
	}

	// We will assume here that we want additive transparency effects.
	alpha = le->alpha + (le->dalpha * frac);
	re->shaderRGBA[0] = 0xff * alpha;
	re->shaderRGBA[1] = 0xff * alpha;
	re->shaderRGBA[2] = 0xff * alpha;
	re->shaderRGBA[3] = 0xff * alpha;	// Yes, we could apply c to this too, but fading the color is better for lines.

	re->shaderTexCoord[0] = 1;
	re->shaderTexCoord[1] = 1;

	re->rotation = 90;

	re->reType = RT_ORIENTEDLINE;

	trap_R_AddRefEntityToScene( re );
}

/*
===================
CG_AddLine

for beams and the like.
===================
*/
void CG_AddLine( localEntity_t *le )
{
	refEntity_t	*re;

	re = &le->refEntity;

	re->reType = RT_LINE;

	trap_R_AddRefEntityToScene( re );
}

//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/
void CG_AddLocalEntities( void ) {
	localEntity_t	*le, *next;

	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntities.prev;
	for ( ;le && le != &cg_activeLocalEntities ; le = next ) {
		// grab next now, so if the local entity is freed we
		// still have it
		next = le->prev;

		if ( cg.time >= le->endTime ) {
			CG_FreeLocalEntity( le );
			continue;
		}
		switch ( le->leType ) {
		default:
			CG_Error( "Bad leType: %i", le->leType );
			break;

		case LE_MARK:
			break;

		case LE_SPRITE_EXPLOSION:
			CG_AddSpriteExplosion( le );
			break;

		case LE_EXPLOSION:
			CG_AddExplosion( le );
			break;

		case LE_FADE_SCALE_MODEL:
			CG_AddFadeScaleModel( le );
			break;

		case LE_FRAGMENT:			// gibs and brass
			CG_AddFragment( le );
			break;

		case LE_PUFF:
			CG_AddPuff( le );
			break;

		case LE_MOVE_SCALE_FADE:		// water bubbles
			CG_AddMoveScaleFade( le );
			break;

		case LE_FADE_RGB:				// teleporters, railtrails
			CG_AddFadeRGB( le );
			break;

		case LE_FALL_SCALE_FADE: // gib blood trails
			CG_AddFallScaleFade( le );
			break;

		case LE_SCALE_FADE:		// rocket trails
			CG_AddScaleFade( le );
			break;

		case LE_SCOREPLUM:
			CG_AddScorePlum( le );
			break;

		case LE_OLINE:
			CG_AddOLine( le );
			break;

		case LE_SHOWREFENTITY:
			CG_AddRefEntity( le );
			break;

		case LE_LINE:					// oriented lines for FX
			CG_AddLine( le );
			break;
		}
	}
}




