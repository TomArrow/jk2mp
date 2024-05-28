// Nerevar's way to produce dismemberment
#include "cg_demos.h" 


/*=========================================================================================
===========================================================================================
																DISMEMBERMENT
===========================================================================================
=========================================================================================*/

// TODO Try to also allow dismemberment for dead bodies that are no longer at the original entity number < MAX_CLIENTS

void demoSaberDismember(centity_t *cent, vec3_t dir) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t saberorigin, saberangles;
	clientInfo_t *ci;
	
	if (!cent->ghoul2)
		return;
	
	////////////INIT
	le = CG_AllocLocalEntity();
	re = &le->refEntity;
	
	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = cg.time + 20000;
	le->lifeRate = 1.0 / (le->endTime - le->startTime);
	
	VectorCopy(cg_entities[cent->currentState.saberEntityNum].currentState.pos.trBase,saberorigin);
	VectorCopy(cg_entities[cent->currentState.saberEntityNum].currentState.apos.trBase,saberangles);
	
	VectorCopy( saberorigin, re->origin );
	AnglesToAxis( saberangles, re->axis );
	
	le->pos.trType = TR_GRAVITY;
	le->angles.trType = TR_GRAVITY;
	VectorCopy( saberorigin, le->pos.trBase );
	VectorCopy( saberangles, le->angles.trBase ); 
	le->pos.trTime = cg.time;
	le->angles.trTime = cg.time;

	le->bounceFactor = 0.6f;
	
	VectorCopy(dir, le->pos.trDelta );
	le->angles.trDelta[0] = Q_irand(-20,20);
	le->angles.trDelta[1] = Q_irand(-20,20);
	le->angles.trDelta[2] = Q_irand(-20,20);
	le->angles.trDelta[Q_irand(0,3)] = 0;
	
	le->leFragmentType = LEFT_SABER;

	/////////SABER GHOUL2
	ci = &cgs.clientinfo[cent->currentState.clientNum];
	if (ci->saberModel && ci->saberModel[0])
		trap_G2API_InitGhoul2Model(&re->ghoul2, va("models/weapons2/%s/saber_w.glm", ci->saberModel), 0, 0, 0, 0, 0);
	else
		trap_G2API_InitGhoul2Model(&re->ghoul2, "models/weapons2/saber/saber_w.glm", 0, 0, 0, 0, 0);
	
	/////REMOVE SABER FROM PLAYERMODEL
	if (trap_G2API_HasGhoul2ModelOnIndex(&(cent->ghoul2), 1))
		trap_G2API_RemoveGhoul2Model(&(cent->ghoul2), 1);
}

void CG_G2PlayerAnglesSimple(centity_t* ent, vec3_t legs[3], vec3_t legsAngles) {
	vec3_t		velocity;
	float		speed;
	int			dir;

	VectorClear(legsAngles);

	dir = ent->currentState.angles2[YAW];
	if (dir < 0 || dir > 7) {
		return;
	}

	// lean towards the direction of travel
	VectorCopy(ent->currentState.pos.trDelta, velocity);
	speed = VectorNormalize(velocity);

	if (speed) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05;

		AnglesToAxis(legsAngles, axis);

		side = speed * DotProduct(velocity, axis[0]);
		legsAngles[PITCH] += side;
	}


	legsAngles[YAW] = ent->lerpAngles[YAW];

	legsAngles[ROLL] = 0;
}

/*
void CG_G2PlayerAngles(centity_t* ent, vec3_t legs[3], vec3_t legsAngles) {
	vec3_t		torsoAngles, headAngles;
	// float		dest;
	static	int	movementOffsets[8] = { 0, 22, 45, -22, 0, 22, -45, -22 };
	vec3_t		velocity;
	float		speed;
	int			dir;
	vec3_t		velPos, velAng;
	int			adddir = 0;
	float		dif;
	float		degrees_negative = 0;
	float		degrees_positive = 0;
	vec3_t		ulAngles, llAngles, viewAngles, angles, thoracicAngles = { 0,0,0 };

	VectorCopy(ent->lerpAngles, headAngles);
	headAngles[YAW] = AngleMod(headAngles[YAW]);
	VectorClear(legsAngles);
	VectorClear(torsoAngles);

	// --------- yaw -------------

	// adjust legs for movement dir
	dir = ent->currentState.angles2[YAW];
	if (dir < 0 || dir > 7) {
		return;
	}

	torsoAngles[YAW] = headAngles[YAW] + 0.25 * movementOffsets[dir];

	// --------- pitch -------------
	*/
	/*
	// only show a fraction of the pitch angle in the torso
	if ( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75;
	} else {
		dest = headAngles[PITCH] * 0.75;
	}
	*/
/*
	torsoAngles[PITCH] = ent->lerpAngles[PITCH];

	// --------- roll -------------


	// lean towards the direction of travel
	VectorCopy(ent->currentState.pos.trDelta, velocity);
	speed = VectorNormalize(velocity);

	if (speed) {
		vec3_t	axis[3];
		float	side;

		speed *= 0.05;

		AnglesToAxis(legsAngles, axis);
		side = speed * DotProduct(velocity, axis[1]);
		legsAngles[ROLL] -= side;

		side = speed * DotProduct(velocity, axis[0]);
		legsAngles[PITCH] += side;
	}

	//rww - crazy velocity-based leg angle calculation
	legsAngles[YAW] = headAngles[YAW];
	velPos[0] = ent->lerpOrigin[0] + velocity[0];
	velPos[1] = ent->lerpOrigin[1] + velocity[1];
	velPos[2] = ent->lerpOrigin[2] + velocity[2];

	if (ent->currentState.groundEntityNum == ENTITYNUM_NONE)
	{ //off the ground, no direction-based leg angles
		VectorCopy(ent->lerpOrigin, velPos);
	}

	VectorSubtract(ent->lerpOrigin, velPos, velAng);

	if (!VectorCompare(velAng, vec3_origin))
	{
		vectoangles(velAng, velAng);

		if (velAng[YAW] <= legsAngles[YAW])
		{
			degrees_negative = (legsAngles[YAW] - velAng[YAW]);
			degrees_positive = (360 - legsAngles[YAW]) + velAng[YAW];
		}
		else
		{
			degrees_negative = legsAngles[YAW] + (360 - velAng[YAW]);
			degrees_positive = (velAng[YAW] - legsAngles[YAW]);
		}

		if (degrees_negative < degrees_positive)
		{
			dif = degrees_negative;
			adddir = 0;
		}
		else
		{
			dif = degrees_positive;
			adddir = 1;
		}

		if (dif > 90)
		{
			dif = (180 - dif);
		}

		if (dif > 60)
		{
			dif = 60;
		}

		//Slight hack for when playing is running backward
		if (dir == 3 || dir == 5)
		{
			dif = -dif;
		}

		if (adddir)
		{
			legsAngles[YAW] -= dif;
		}
		else
		{
			legsAngles[YAW] += dif;
		}
	}

	legsAngles[YAW] = ent->lerpAngles[YAW];

	legsAngles[ROLL] = 0;
	torsoAngles[ROLL] = 0;

	// pull the angles back out of the hierarchial chain
	AnglesSubtract(headAngles, torsoAngles, headAngles);
	AnglesSubtract(torsoAngles, legsAngles, torsoAngles);
	AnglesToAxis(legsAngles, legs);
	// we assume that model 0 is the player model.

	VectorCopy(ent->lerpAngles, viewAngles);

	if (viewAngles[PITCH] > 290)
	{ //keep the same general range as lerpAngles on the client so we can use the same spine correction
		viewAngles[PITCH] -= 360;
	}

	viewAngles[YAW] = viewAngles[ROLL] = 0;
	viewAngles[PITCH] *= 0.5;

	if (!demo15detected)
	{
		VectorCopy(legsAngles, angles);
	}
	else
	{
		VectorCopy(ent->lerpAngles, angles);
		angles[PITCH] = 0;
	}

	G_G2ClientSpineAngles(ent, viewAngles, angles, thoracicAngles, ulAngles, llAngles);

	if (!demo15detected)
	{
		ulAngles[YAW] += torsoAngles[YAW] * 0.3;
		llAngles[YAW] += torsoAngles[YAW] * 0.3;
		thoracicAngles[YAW] += torsoAngles[YAW] * 0.4;

		ulAngles[PITCH] = torsoAngles[PITCH] * 0.3;
		llAngles[PITCH] = torsoAngles[PITCH] * 0.3;
		thoracicAngles[PITCH] = torsoAngles[PITCH] * 0.4;

		ulAngles[ROLL] += torsoAngles[ROLL] * 0.3;
		llAngles[ROLL] += torsoAngles[ROLL] * 0.3;
		thoracicAngles[ROLL] += torsoAngles[ROLL] * 0.4;
	}

	//trap_G2API_SetBoneAngles(ent->ghoul2, 0, "upper_lumbar", ulAngles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, level.time);
	//trap_G2API_SetBoneAngles(ent->ghoul2, 0, "lower_lumbar", llAngles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, level.time);
	//trap_G2API_SetBoneAngles(ent->ghoul2, 0, "thoracic", thoracicAngles, BONE_ANGLES_POSTMULT, POSITIVE_X, NEGATIVE_Y, NEGATIVE_Z, NULL, 0, level.time);
}*/
void CG_GetDismemberBolt(centity_t* self, vec3_t boltPoint, dismpart_t limbType)
{
	int useBolt = 0;// self->bolt;
	vec3_t properOrigin, properAngles;// , addVel;
	vec3_t legAxis[3];
	mdxaBone_t	boltMatrix;
	float fVSpeed = 0;

	switch (limbType)
	{
	case DISM_HEAD:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "cranium");
		break;
	case DISM_WAIST:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "thoracic");
		break;
	case DISM_LARM:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "lradius");
		break;
	case DISM_RARM:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "rradius");
		break;
	case DISM_RHAND:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "rhand");
		break;
	case DISM_LHAND: // Added by me
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "lhand");
		break;
	case DISM_LLEG:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "ltibia");
		break;
	case DISM_RLEG:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "rtibia");
		break;
	default:
		useBolt = trap_G2API_AddBolt(self->ghoul2, 0, "cranium");
		break;
	}

	//VectorCopy(self->client->ps.origin, properOrigin);
	//VectorCopy(self->client->ps.viewangles, properAngles);
	VectorCopy(self->lerpOrigin, properOrigin);
	VectorCopy(self->lerpAngles, properAngles);

	//try to predict the origin based on velocity so it's more like what the client is seeing (no need for this in cgame)
	/*VectorCopy(self->currentState.pos.trDelta, addVel);
	VectorNormalize(addVel);
	
	if (self->currentState.pos.trDelta[0] < 0)
	{
		fVSpeed += (-self->currentState.pos.trDelta[0]);
	}
	else
	{
		fVSpeed += self->currentState.pos.trDelta[0];
	}
	if (self->currentState.pos.trDelta[1] < 0)
	{
		fVSpeed += (-self->currentState.pos.trDelta[1]);
	}
	else
	{
		fVSpeed += self->currentState.pos.trDelta[1];
	}
	if (self->currentState.pos.trDelta[2] < 0)
	{
		fVSpeed += (-self->currentState.pos.trDelta[2]);
	}
	else
	{
		fVSpeed += self->currentState.pos.trDelta[2];
	}

	fVSpeed *= 0.08;

	properOrigin[0] += addVel[0] * fVSpeed;
	properOrigin[1] += addVel[1] * fVSpeed;
	properOrigin[2] += addVel[2] * fVSpeed*/

	properAngles[0] = 0;
	properAngles[1] = self->lerpAngles[YAW];
	properAngles[2] = 0;

	AnglesToAxis(properAngles, legAxis);
	CG_G2PlayerAnglesSimple(self, legAxis, properAngles);

	trap_G2API_GetBoltMatrix(self->ghoul2, 0, useBolt, &boltMatrix, properAngles, properOrigin, cg.time, NULL, vec3_origin);

	boltPoint[0] = boltMatrix.matrix[0][3];
	boltPoint[1] = boltMatrix.matrix[1][3];
	boltPoint[2] = boltMatrix.matrix[2][3];

	if (!demo15detected)
	{
		//trap_G2API_GetBoltMatrix(self->ghoul2, 1, 0, &boltMatrix, properAngles, properOrigin, cg.time, NULL, vec3_origin);

		if (/*self->client  && */limbType == G2_MODELPART_RHAND)
		{ //Make some saber hit sparks over the severed wrist area
			/*vec3_t boltAngles;
			centity_t* te;

			boltAngles[0] = -boltMatrix.matrix[0][1];
			boltAngles[1] = -boltMatrix.matrix[1][1];
			boltAngles[2] = -boltMatrix.matrix[2][1];*/

			//te = G_TempEntity(boltPoint, EV_SABER_HIT);

			//VectorCopy(boltPoint, te->s.origin);
			//VectorCopy(boltAngles, te->s.angles);

			//if (!te->s.angles[0] && !te->s.angles[1] && !te->s.angles[2])
			//{ //don't let it play with no direction
			//	te->s.angles[1] = 1;
			//}

			//te->s.eventParm = 16; //lots of sparks
		}
	}
}


//Main dismemberment function
static void demoDismember( centity_t *cent , vec3_t dir, int part, vec3_t limborg, vec3_t limbang ) {
	localEntity_t	*le;
	refEntity_t		*re;
	const char *limbBone;
	char *limbName;
	char *limbCapName;
	char *stubCapName;
	int  limb_anim;
	int clientnum = cent->currentState.number;
	vec3_t	boltPoint;
		
	if (!cent->ghoul2 || cg_entities[clientnum].dism.cut[part] == qtrue)
		return;
	
	if (cg_entities[clientnum].dism.cut[DISM_WAIST] == qtrue)
		if (part >= DISM_HEAD && part <= DISM_RARM)  //connected to waist
			return;
	
	if (cg_entities[clientnum].dism.cut[DISM_LARM] == qtrue) 
		if (part == DISM_LHAND) //connected to left arm
			return;
	
	if (cg_entities[clientnum].dism.cut[DISM_RARM] == qtrue)
		if (part == DISM_RHAND) //connected to right arm
			return;

	////////////INIT
	le = CG_AllocLocalEntity();
	re = &le->refEntity;
	
	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = cg.time + 20000; //limb lifetime (FIXME: cvar?)
	le->lifeRate = 1.0f / (le->endTime - le->startTime);
	
	VectorCopy( limborg, re->origin );
	AnglesToAxis( limbang, re->axis );
	
	le->pos.trType = TR_GRAVITY;
	le->angles.trType = TR_GRAVITY;
	VectorCopy( limborg, le->pos.trBase );
	VectorCopy( limbang, le->angles.trBase); 
	le->pos.trTime = cg.time;
	le->angles.trTime = cg.time;

	le->bounceFactor = 0.1f + random()*0.2;
	
	VectorCopy(dir, le->pos.trDelta );
	le->leFragmentType = LEFT_GIB;
	
	/////////DUPLICATE GHOUL2
	if (re->ghoul2 && trap_G2_HaveWeGhoul2Models(re->ghoul2))
		trap_G2API_CleanGhoul2Models(&re->ghoul2);
	if (cent->ghoul2 && trap_G2_HaveWeGhoul2Models(cent->ghoul2))
		trap_G2API_DuplicateGhoul2Instance(cent->ghoul2, &re->ghoul2);
	
	/////////ANIMATION FIXME: stop routine animations
	
	switch( part ) {
		case DISM_HEAD:
			limbBone = "cervical";
			limbName = "head";
			limbCapName = "head_cap_torso_off";
			stubCapName = "torso_cap_head_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_HEAD1_15:BOTH_DISMEMBER_HEAD1;
			break;
		case DISM_WAIST:
			limbBone = "pelvis";
			limbName = "torso";
			limbCapName = "torso_cap_hips_off";
			stubCapName = "hips_cap_torso_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_HEAD1_15:BOTH_DISMEMBER_TORSO1;
			break;
		case DISM_LARM:
			limbBone = "lhumerus";
			limbName = "l_arm";
			limbCapName = "l_arm_cap_torso_off";
			stubCapName = "torso_cap_l_arm_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_LARM_15:BOTH_DISMEMBER_LARM;
			break;
		case DISM_RARM:
			limbBone = "rhumerus";
			limbName = "r_arm";
			limbCapName = "r_arm_cap_torso_off";
			stubCapName = "torso_cap_r_arm_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_RARM_15:BOTH_DISMEMBER_RARM;
			break;
		case DISM_LHAND:
			limbBone = "lradiusX";
			limbName = "l_hand";
			limbCapName = "l_hand_cap_l_arm_off";
			stubCapName = "l_arm_cap_l_hand_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_LARM_15:BOTH_DISMEMBER_LARM;
			break;
		case DISM_RHAND:
			limbBone = "rradiusX";
			limbName = "r_hand";
			limbCapName = "r_hand_cap_r_arm_off";
			stubCapName = "r_arm_cap_r_hand_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_RARM_15:BOTH_DISMEMBER_RARM;
			break;
		case DISM_LLEG:
			limbBone = "lfemurYZ";
			limbName = "l_leg";
			limbCapName = "l_leg_cap_hips_off";
			stubCapName = "hips_cap_l_leg_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_LLEG_15:BOTH_DISMEMBER_LLEG;
			break;
		case DISM_RLEG:
			limbBone = "rfemurYZ";
			limbName = "r_leg";
			limbCapName = "r_leg_cap_hips_off";
			stubCapName = "hips_cap_r_leg_off";
			limb_anim = demo15detected?BOTH_DISMEMBER_RLEG_15:BOTH_DISMEMBER_RLEG;
			break;
		default:
			return;	
	}

	if (mov_dismemberClassical.integer) {
		vec3_t centerToBoltPoint;
		CG_GetDismemberBolt(cent, boltPoint, part);
		VectorSubtract(boltPoint, cent->lerpOrigin, centerToBoltPoint);
		VectorNormalize(centerToBoltPoint);
		VectorMA(cent->currentState.pos.trDelta, 100, centerToBoltPoint, le->pos.trDelta);
	}

	//FIXME: FREEZE THE ANIMATION

	//////////DISMEMBER
	trap_G2API_SetRootSurface(re->ghoul2, 0, limbName);
	trap_G2API_SetNewOrigin(re->ghoul2, trap_G2API_AddBolt(re->ghoul2, 0, limbBone));
	trap_G2API_SetSurfaceOnOff(re->ghoul2, limbCapName, 0);
	
	trap_G2API_SetSurfaceOnOff(cent->ghoul2, limbName, 0x00000100);
	trap_G2API_SetSurfaceOnOff(cent->ghoul2, stubCapName, 0);
	
	le->limbpart = part;
	cg_entities[clientnum].dism.cut[part] = qtrue;	
	if (mov_dismemberDisallowNative.integer) {
		cg_entities[clientnum].torsoBolt = 1;
	}
	cg_entities[clientnum].anyDismember = qtrue;
	
	////EFFECTS
	if (cg_dismemberSaberHitSounds.integer) {
		trap_S_StartSound(limborg, cent->currentState.number, CHAN_BODY, trap_S_RegisterSound(va("sound/weapons/saber/saberhit%i.mp3", Q_irand(1, 4))));
	}
	VectorNormalize(dir);
	trap_FX_PlayEffectID( trap_FX_RegisterEffect("saber/blood_sparks.efx"), limborg, dir );
}

// TODO Wtf is this algorithm. Seems random af. It should check for contact with body parts but it does some other random thing instead.
void demoCheckDismember(vec3_t saberhitorg) {
	centity_t *attacker;
	centity_t *target;
	vec3_t dir;
	float velocity;
	int i;
	
	vec3_t boltOrg[8];
	int newBolt;
	mdxaBone_t			matrix;
	char *limbTagName;
	float limbdis[8];
	qboolean cut[8];
	int dismnum, limbnum;
	
	float bestlen = 999999;
	int best = -1;

	for (i = 0; i < MAX_CLIENTS; i++) {
		centity_t *test = &cg_entities[i];		
		if (test && test->currentState.eFlags & EF_DEAD
			&& cg_entities[i].dism.deathtime
			&& test->currentState.eType == ET_PLAYER ) {
			if (cg_entities[i].dism.deathtime == cg.time ) {
				float dist = Distance(test->lerpOrigin,saberhitorg);			
				if (dist < bestlen) {
					bestlen = dist;
					best = i;
				}
			}
		}
	}

	/* found dismembered client? */
	if (best < 0)
		return;

	target = &cg_entities[best];
	if (!target)
		return;
		
	if (cg_entities[best].dism.lastkiller >= 0 && cg_entities[best].dism.lastkiller < MAX_CLIENTS) {
		attacker = &cg_entities[cg_entities[best].dism.lastkiller];
		if (!attacker)
			return;
	} else {
		return;
	}
	
	
		
	for (i = 0 ; i < 8; i++) {				
		if (i == DISM_HEAD) {
			limbTagName = "*head_cap_torso";
		} else if (i == DISM_LHAND) {
			limbTagName = "*l_hand_cap_l_arm";
		} else if (i == DISM_RHAND) {
			limbTagName = "*r_hand_cap_r_arm";
		} else if (i == DISM_LARM) {
			limbTagName = "*l_arm_cap_torso";
		} else if (i == DISM_RARM) {
			limbTagName = "*r_arm_cap_torso";
		} else if (i == DISM_LLEG) {
			limbTagName = "*l_leg_cap_hips";
		} else if (i == DISM_RLEG) {
			limbTagName = "*r_leg_cap_hips";
		} else /*if (i == DISM_WAIST)*/ {
			limbTagName = "*torso_cap_hips";
		}
		
		newBolt = trap_G2API_AddBolt( target->ghoul2, 0, limbTagName );

		if ( newBolt != -1 ) {
			trap_G2API_GetBoltMatrix(target->ghoul2, 0, newBolt, &matrix, target->lerpAngles, target->lerpOrigin, cg.time, cgs.gameModels, target->modelScale);

			trap_G2API_GiveMeVectorFromMatrix(&matrix, ORIGIN, boltOrg[i]);
			//trap_G2API_GiveMeVectorFromMatrix(&matrix, NEGATIVE_Y, boltAng[i]);
			
			//boltAng[i][0] = random();
			//boltAng[i][0] = random();
			//boltAng[i][0] = random();
			
			//TESTING
			//trap_FX_PlayEffectID(trap_FX_RegisterEffect("marker.efx"), boltOrg, target->lerpAngles);
			//trap_FX_PlayEffectID(trap_FX_RegisterEffect("marker.efx"), saberhitorg, target->lerpAngles);
			
			//trap_SendConsoleCommand(va("echo %i: %i %i %i, %i %i %i;\n",(int)Distance(saberhitorg,boltOrg), (int)boltOrg[0], (int)boltOrg[1], (int)boltOrg[2], (int)saberhitorg[0], (int)saberhitorg[1], (int)saberhitorg[2] ));
			
			limbdis[i] = Distance(saberhitorg,boltOrg[i]);
			cut[i] = qfalse;
		}
	}
	
	dismnum = 0;
	
	//CALC LIMB NUMBER TO DISMEMBER
	if ( BG_SaberInAttack(attacker->currentState.saberMove & ~ANIM_TOGGLEBIT) ) {
		if ( BG_SaberInSpecial(attacker->currentState.saberMove & ~ANIM_TOGGLEBIT) ) {
			limbnum = 3;
		} else {			
			limbnum = 2;
		}
	} else {
		limbnum = 1;
	}
	
	if (cgs.gametype == GT_CTF || cgs.gametype == GT_CTY) {
		i = 23;
	} else {
		i = 16;
	}
	
	limbnum += (int)(((VectorLength(attacker->currentState.pos.trDelta)/100)*(VectorLength(attacker->currentState.pos.trDelta)/100))/i);
	
	if (limbnum > 7) limbnum = 7;
	///////////////////////////////

	if (limbnum == 7) {
		//CGCam_Shake( 1500, 1500 );
		trap_S_StartSound(attacker->lerpOrigin, attacker->currentState.number, CHAN_AUTO, trap_S_RegisterSound("sound/gauss_shot.wav"));
	}
	
	//CG_CenterPrint(va("%i",limbnum), SCREEN_HEIGHT * .05, 0);
	
	if (limbnum == 7) {
		for (i = 0; i < 8; i++) {
			cut[i] = qtrue;
		}
	} else {
		while (dismnum < limbnum) {
			float bestlen = 999999999;
			int best = 0;
			for (i = 0; i < 8; i++) {
					if (limbdis[i] < bestlen && !cut[i]) {
						best = i;
						bestlen = limbdis[i];
					}
			}			
			cut[best] = qtrue;
			dismnum ++;
		}
	}
	
	velocity = VectorLength(target->currentState.pos.trDelta);
		
	for (i = 0; i < 8; i++) {
		if (cut[i]) {
			if (limbnum == 7) {
				dir[0] = (-1 + random()*2);
				dir[1] = (-1 + random()*2);
				dir[2] = (-1 + random()*2);
				VectorScale(dir,velocity,dir);
			} else {
				dir[0] = target->currentState.pos.trDelta[0] * (0.5 + random());
				dir[1] = target->currentState.pos.trDelta[1] * (0.5 + random());
				dir[2] = target->currentState.pos.trDelta[2] * (0.5 + random());
			}
			demoDismember(target,dir,i,boltOrg[i],dir);
		}
	}
}

#ifdef RELDEBUG
//#pragma optimize("", off)
#endif

void demoCheckCorpseDism( centity_t *attacker ) {
	centity_t *saber = &cg_entities[attacker->currentState.saberEntityNum];
	vec3_t saberstart,saberend,saberang;
	int i;
	
	if (!saber || !attacker || !attacker->saberLength 
		|| cgs.clientinfo[attacker->currentState.number].team == TEAM_SPECTATOR)
		return;
		
	VectorCopy(saber->currentState.pos.trBase,saberstart);
	VectorCopy(saber->currentState.apos.trBase,saberang);
	VectorMA(saberstart,attacker->saberLength,saberang,saberend);
	
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (i != attacker->currentState.number) {
			centity_t *target = &cg_entities[i];
		
			float attackerMaxSaberLength = cgs.clientinfo[attacker->currentState.number].saberLength;
			if ( target && target->currentState.eFlags & EF_DEAD && Distance(target->lerpOrigin,saberend) < max(80,(80* attackerMaxSaberLength/SABER_LENGTH_MAX)) /*&& Distance(target->lerpOrigin,saberend)*/) {
				vec3_t boltOrg, boltAng;
				int newBolt;
				mdxaBone_t			matrix;
				int part;
				char *limbTagName;
				
				for (part = 0 ; part < 8; part++) {				
					if (part == DISM_HEAD) {
						limbTagName = "*head_cap_torso";
					} else if (part == DISM_LHAND) {
						limbTagName = "*l_hand_cap_l_arm";
					} else if (part == DISM_RHAND) {
						limbTagName = "*r_hand_cap_r_arm";
					} else if (part == DISM_LARM) {
						limbTagName = "*l_arm_cap_torso";
					} else if (part == DISM_RARM) {
						limbTagName = "*r_arm_cap_torso";
					} else if (part == DISM_LLEG) {
						limbTagName = "*l_leg_cap_hips";
					} else if (part == DISM_RLEG) {
						limbTagName = "*r_leg_cap_hips";
					} else /*if (part == DISM_WAIST)*/ {
						limbTagName = "*torso_cap_hips";
					}
					
					newBolt = trap_G2API_AddBolt( target->ghoul2, 0, limbTagName );

					if ( newBolt != -1 ) {
						int length;
						int rad = 5;
						
						if (part == DISM_WAIST)
							rad = 4;
							
						trap_G2API_GetBoltMatrix(target->ghoul2, 0, newBolt, &matrix, target->lerpAngles, target->lerpOrigin, cg.time, cgs.gameModels, target->modelScale);
			
						trap_G2API_GiveMeVectorFromMatrix(&matrix, ORIGIN, boltOrg);
						trap_G2API_GiveMeVectorFromMatrix(&matrix, NEGATIVE_Y, boltAng);
						
						//int segments = min(5 * attackerMaxSaberLength / SABER_LENGTH_MAX,5);
						int segments = max(mov_dismemberCheckSegments.integer * attackerMaxSaberLength / SABER_LENGTH_MAX, mov_dismemberCheckSegments.integer);
						float distancePerSegment = 1.0f/(float)segments;
						for (length = 0; length < segments; length++) {
							vec3_t checkorg;							
							VectorMA(saberend,-attacker->saberLength*length* distancePerSegment, saberang,checkorg);
							if (Distance(boltOrg,checkorg) < rad) {
								demoDismember(target,saberang,part,boltOrg,boltAng);
							}
						}
					}
				}											
			}
		}		
	}
}

#ifdef RELDEBUG
//#pragma optimize("", on)
#endif

void demoPlayerDismember(centity_t *cent) {
	int clientnum = cent->currentState.number;
	
	if (mov_dismember.integer == 2)
		demoCheckCorpseDism(cent);
		
	if (cent->currentState.eFlags & EF_DEAD) {
		//Make some smoke
		int newBolt;
		mdxaBone_t	matrix;
		int part;
		char *stubTagName;
			
		for (part = 0; part < 8; part++) {				
			if (cg_entities[clientnum].dism.cut[part] == qfalse) 
				continue;

			if (part >= DISM_HEAD && part <= DISM_RARM && cg_entities[clientnum].dism.cut[DISM_WAIST] == qtrue)
				continue;

			if (part == DISM_RHAND && cg_entities[clientnum].dism.cut[DISM_RARM] == qtrue)
				continue;
			if (part == DISM_LHAND && cg_entities[clientnum].dism.cut[DISM_LARM] == qtrue)
				continue;

			switch(part) {
				case DISM_HEAD:
					stubTagName = "*torso_cap_head";
					break;
				case DISM_LHAND:
					stubTagName = "*l_arm_cap_l_hand";
					break;
				case DISM_RHAND:
					stubTagName = "*r_arm_cap_r_hand";
					break;
				case DISM_LARM:
					stubTagName = "*torso_cap_l_arm";
					break;
				case DISM_RARM:
					stubTagName = "*torso_cap_r_arm";
					break;
				case DISM_LLEG:
					stubTagName = "*hips_cap_l_leg";
					break;
				case DISM_RLEG:
					stubTagName = "*hips_cap_r_leg";
					break;
				case DISM_WAIST:
					stubTagName = "*hips_cap_torso";
					break;
				default:
					continue;
			}
				
			newBolt = trap_G2API_AddBolt( cent->ghoul2, 0, stubTagName );
			if ( newBolt != -1 ) {
				vec3_t boltOrg, boltAng;
		
				trap_G2API_GetBoltMatrix(cent->ghoul2, 0, newBolt, &matrix, cent->lerpAngles, cent->lerpOrigin, cg.time, cgs.gameModels, cent->modelScale);
		
				trap_G2API_GiveMeVectorFromMatrix(&matrix, ORIGIN, boltOrg);
				trap_G2API_GiveMeVectorFromMatrix(&matrix, NEGATIVE_Y, boltAng);
		
				trap_FX_PlayEffectID(trap_FX_RegisterEffect("smoke_bolton"), boltOrg, boltAng);
			}
		}		
	}
}
