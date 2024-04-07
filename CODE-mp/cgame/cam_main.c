
#include "cg_demos.h"
#include "cg_local.h"
camera_t cam;

void Cam_Draw2d(void)
{
	if (cam_shownames.integer && !cam_shownames3D.integer)
		Cam_DrawClientNames();
}
void Cam_Draw3d(void)
{
	if (cam_shownames.integer && cam_shownames3D.integer)
		Cam_DrawClientNames();

	if (cam_hud3D.integer )
		Cam_Add3DHUd();
}

void Cam_DrawClientNames(void) //FIXME: draw entitynums
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		centity_t* cent = &cg_entities[i];

		if (cent && cent->currentValid && ((i != CG_CrosshairPlayer() /*&& i != cam.specEnt*/) /*|| cam_freeview.integer*/))
		{
			trace_t trace;

			int skipNumber = cam_shownamesIncludePlayer.integer ? -1 : cg.snap->ps.clientNum;
			if (cam_specEnt.integer != -1) {
				skipNumber = cam_shownamesIncludePlayer.integer ? -1 : cam_specEnt.integer;
			}
			if (demo.chase.target != -1) {
				skipNumber = cam_shownamesIncludePlayer.integer ? -1 : demo.chase.target;
			}
			if (!cam_shownames3D.integer) { // Why waste time on traces when we're drawing in 3d anyway
				CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, cent->lerpOrigin, skipNumber, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_CORPSE);
			}

			if ((!cam_shownames3D.integer && trace.entityNum == i) || (cam_shownames3D.integer && skipNumber != i))
			{
				float x, y;
				vec3_t org;
				float size;

				VectorCopy(cent->lerpOrigin, org);
				
				// See if we can get a more precise location based on the player's head
				if (cam_shownamesPositionBasedOnG2Head.integer) {
					clientInfo_t* ci = cgs.clientinfo + i;
					if (ci->bolt_head) {
						mdxaBone_t boneMatrix;
						if (trap_G2API_GetBoltMatrix(cent->ghoul2, 0, ci->bolt_head, &boneMatrix, cent->turAngles, cent->lerpOrigin, cg.time, cgs.gameModels, cent->modelScale)) {
							vec3_t betterOrigin;
							trap_G2API_GiveMeVectorFromMatrix(&boneMatrix, ORIGIN, betterOrigin);
							VectorCopy(betterOrigin, org);
						}
					}
				}

				

				if (cam_shownames3D.integer) {


					char* text = cgs.clientinfo[i].name;

					size = 0.5f*cam_shownames3DScale.value; // guess
					org[2] += 30 + CG_Text_Height(text, size, FONT_LARGE);


					int style = cam_shownamesStyle.integer;
					style = style > 6 ? 6 : style; // There are no styles above 6
					style = style < 0 ? 0 : style; // There are no styles below 0

					x = CG_Text_Width(text, size, FONT_LARGE) / 2;

					

					
					//
					vec3_t axis[3];
					vec3_t angles;
					if (cam_shownames3DCamOrient.integer) { // Angle looking actually towards camera
						vec3_t viewDir;
						VectorSubtract(org,cg.refdef.vieworg,viewDir);
						vectoangles(viewDir, angles);
						angles[2] = cg.refdefViewAngles[2];
					}
					else { // Angle looking towards plane of camera
						VectorCopy(cg.refdefViewAngles, angles);
					}
					if (cam_shownames3DLockZRot.integer)
						angles[2] = 0;
					if (cam_shownames3DLockYRot.integer)
						angles[0] = 0;
					AnglesToAxis(angles, axis);
					VectorMA(org, x, axis[1], org);
					CG_Text_Paint_3D(org, axis, size, colorWhite, text, 0, 0, style/*NORMAL*/, FONT_LARGE);


					// This works: but hard to lock angles.
					//VectorMA(org, x, cg.refdef.viewaxis[1], org);
					//CG_Text_Paint_3D(org, cg.refdef.viewaxis, size, colorLtGrey, text, 0, 0, style/*NORMAL*/, FONT_LARGE);

				}
				else {
					// Classic 2D overlay

					size = Distance(cg.refdef.vieworg, org);
					if (!size) return;

					size = 200.0f / size;

					org[2] += 45;

					if (size > 0.8f) size = 0.8f;
					if (size < 0.3f) size = 0.3f;

					if (CG_WorldCoordToScreenCoordFloat(org, &x, &y))
					{
						char* text = cgs.clientinfo[i].name;

						x = (x - SCREEN_WIDTH / 2) * cgs.widthRatioCoef + SCREEN_WIDTH / 2;

						int style = cam_shownamesStyle.integer;
						style = style > 6 ? 6 : style; // There are no styles above 6
						style = style < 0 ? 0 : style; // There are no styles below 0

						x -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
						//y -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
						CG_Text_Paint(x, y, size, colorWhite, text, 0, 0, style/*NORMAL*/, FONT_MEDIUM);
					}
				}
				
			}
		}
	}
}

static qboolean GetBoltPositionReal( centity_t* cent, qhandle_t bolt, vec3_t result) {
	if (!cent->ghoul2 || /*!bolt || */bolt == -1) {
		return qfalse;
	}
	mdxaBone_t boneMatrix;
	if (trap_G2API_GetBoltMatrix(cent->ghoul2, 0, bolt, &boneMatrix, cent->turAngles, cent->lerpOrigin, cg.time, cgs.gameModels, cent->modelScale)) {
		vec3_t betterOrigin;
		trap_G2API_GiveMeVectorFromMatrix(&boneMatrix, ORIGIN, betterOrigin);
		VectorCopy(betterOrigin, result);
		return qtrue;
	}
	return qfalse;
}

static qboolean GetBoltPosition(centity_t* cent, const char* boltName,  vec3_t result) {
	if (!cent->ghoul2) {
		return qfalse;
	}
	qhandle_t bolt = trap_G2API_AddBolt(cent->ghoul2, 0, boltName);
	return GetBoltPositionReal( cent, bolt, result);
}

// copied from/based on demoDrawSetupVerts
void hud3DDrawSetupVerts(polyVert_t* verts, const vec4_t color) {
	int i;
	for (i = 0; i < 4; i++) {
		verts[i].modulate[0] = color[0] * 255;
		verts[i].modulate[1] = color[1] * 255;
		verts[i].modulate[2] = color[2] * 255;
		verts[i].modulate[3] = color[3] * 255;
		verts[i].st[0] = (i & 2) ? 0 : 1;
		verts[i].st[1] = (i & 1) ? 0 : 1;
	}
}


void GetPerpendicularViewVector(const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up);

// copied from/based on demoDrawRawLine
void hud3DDrawRawLine(const vec3_t start, const vec3_t end, float width, polyVert_t* verts, vec3_t fixedUp) {
	vec3_t up;
	vec3_t middle;

	//VectorScale(start, 0.5, middle);
	//VectorMA(middle, 0.5, end, middle);
	//if (VectorDistance(middle, cg.refdef.vieworg) < 100)
	//	return;
	if (fixedUp) {
		VectorCopy(fixedUp, up);
	}
	else {
		GetPerpendicularViewVector(cg.refdef.vieworg, start, end, up);
	}
	VectorMA(start, width, up, verts[0].xyz);
	VectorMA(start, -width, up, verts[1].xyz);
	VectorMA(end, -width, up, verts[2].xyz);
	VectorMA(end, width, up, verts[3].xyz);
	trap_R_AddPolyToScene(cgs.media.mmeWhiteShader, 4, verts);
}


void Cam_Add3DHUd() {
	vec3_t cervical, llumbar;
	vec3_t forward,right, up;
	vec3_t start, end;
	vec3_t startHealth, endHealth;
	vec3_t startShield, endShield;
	vec4_t forceBarColor = { 0.0f, 0.3f, 0.5f, 0.5f};
	vec4_t healthBarColor = { 0.5f, 0.0f, 0.0f, 0.5f};
	vec4_t shieldBarColor = { 0.0f, 0.5f, 0.0f, 0.5f};
	float fullHeight;
	polyVert_t verts[4];

	int playerNum = cam_shownamesIncludePlayer.integer ? -1 : cg.snap->ps.clientNum;
	if (cam_specEnt.integer != -1) {
		playerNum = cam_shownamesIncludePlayer.integer ? -1 : cam_specEnt.integer;
	}
	if (demo.chase.target != -1) {
		playerNum = cam_shownamesIncludePlayer.integer ? -1 : demo.chase.target;
	}
	if (playerNum != cg.snap->ps.clientNum) {
		return;
	}

	centity_t* cent = &cg_entities[playerNum];
	if (!cent->currentValid) {
		return;
	}
	clientInfo_t* ci = cgs.clientinfo + playerNum;
	qboolean success = qtrue;
	success = success && GetBoltPositionReal(cent, ci->shadowBolts.cervical, cervical);
	success = success && GetBoltPositionReal(cent, ci->shadowBolts.llumbar, llumbar);
	if (!success) {
		return;
	}
	AngleVectors(cent->lerpAngles, NULL, right, NULL);
	VectorSubtract(cervical, llumbar, up);
	fullHeight = VectorNormalize(up);
	CrossProduct(right, up, forward);
	VectorNormalize(forward);
	VectorMA(llumbar, cam_hud3DOffset.value, forward, start);

	VectorCopy(start,startHealth);
	VectorCopy(start,startShield);

	VectorMA(start, 2.0f, right, start);
	VectorMA(start, fullHeight*(float)cg.predictedPlayerState.fd.forcePower/ 100.0f, up, end);

	VectorMA(startShield, -1.5f, right, startShield);
	VectorMA(startShield, fullHeight*(float)cg.predictedPlayerState.stats[STAT_ARMOR] / 200.0f, up, endShield);

	VectorMA(startHealth, -2.25f, right, startHealth);
	VectorMA(startHealth, fullHeight*(float)cg.predictedPlayerState.stats[STAT_HEALTH] / 100.0f, up, endHealth);


	//VectorMA(start, fullHeight*cg.predictedPlayerState.stats[STAT_HEALTH], up, end);
	hud3DDrawSetupVerts(verts, forceBarColor);
	hud3DDrawRawLine(start,end,1.5f,verts, NULL); // 1.25 from middle
	hud3DDrawSetupVerts(verts, shieldBarColor);
	hud3DDrawRawLine(startShield, endShield,0.5f,verts, NULL); // 1.25 from middle
	hud3DDrawSetupVerts(verts, healthBarColor);
	hud3DDrawRawLine(startHealth, endHealth,0.5f,verts, NULL); // 1.25 from middle
}

void Cam_AddPlayerShadowLines() {
	int i;
	vec3_t p1, p2;
	vec3_t rtibia, ltibia, rtalus, ltalus, cervical, llumbar, rhumerus, lhumerus, rradius, lradius, rhand, lhand;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		centity_t* cent = &cg_entities[i];
		clientInfo_t* ci = cgs.clientinfo + i;

		if (cent->currentValid) {
			if (0) { // simple
				VectorCopy(cent->lerpOrigin, p1);
				VectorCopy(cent->lerpOrigin, p2);
				p1[2] += DEFAULT_MAXS_2;
				p1[2] += DEFAULT_MINS_2;

				trap_R_AddShadowLineToScene(p1, p2, 20.0, 0, 0,1);
			}
			else { // this is cool idea in theory but ... atm a bit slow and doesnt look quite right yet; edit: nvm
				
				VectorCopy(cent->lerpOrigin, p1);
				VectorCopy(cent->lerpOrigin, p2);
				p1[2] += DEFAULT_MAXS_2;
				//p2[2] += DEFAULT_MINS_2;
				trap_R_AddShadowLineToScene(p1, p2, 60.0, 400.0, 0, 2); // ambient occlusion thingie

				/*qboolean success = qtrue;
				success = success && GetBoltPosition(cent,"rtibia",rtibia);
				success = success && GetBoltPosition(cent,"ltibia", ltibia);
				success = success && GetBoltPosition(cent,"rtalus", rtalus);
				success = success && GetBoltPosition(cent,"ltalus", ltalus);
				success = success && GetBoltPosition(cent,"cervical", cervical);
				success = success && GetBoltPosition(cent,"lower_lumbar", llumbar);
				success = success && GetBoltPosition(cent,"rhumerus", rhumerus);
				success = success && GetBoltPosition(cent,"lhumerus", lhumerus);
				success = success && GetBoltPosition(cent,"rradius", rradius);
				success = success && GetBoltPosition(cent,"lradius", lradius);
				success = success && GetBoltPosition(cent,"rhand", rhand);
				success = success && GetBoltPosition(cent,"lhand", lhand);*/
				qboolean success = qtrue;
				success = success && GetBoltPositionReal(cent,ci->shadowBolts.rtibia,rtibia);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.ltibia, ltibia);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.rtalus, rtalus);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.ltalus, ltalus);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.cervical, cervical);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.llumbar, llumbar);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.rhumerus, rhumerus);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.lhumerus, lhumerus);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.rradius, rradius);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.lradius, lradius);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.rhand, rhand);
				success = success && GetBoltPositionReal(cent, ci->shadowBolts.lhand, lhand);

				if (success) {

					trap_R_AddShadowLineToScene(cervical, llumbar, 15.0, 0, 0, 0);

					trap_R_AddShadowLineToScene(cervical, rhumerus, 7.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(rradius, rhumerus, 5.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(rradius, rhand, 3.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(cervical, lhumerus, 7.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(lradius, lhumerus, 5.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(lradius, lhand, 3.0, 0, 0, 0);

					trap_R_AddShadowLineToScene(rtibia, llumbar, 7.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(rtalus, rtibia,3.0, 60.0, 30.0, 1);
					trap_R_AddShadowLineToScene(ltibia, llumbar, 7.0, 0, 0, 0);
					trap_R_AddShadowLineToScene(ltalus, ltibia, 3.0, 60.0, 30.0, 1);
				}
			}
		}
	}
}