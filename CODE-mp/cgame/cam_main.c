
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
}

void Cam_DrawClientNames(void) //FIXME: draw entitynums
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		centity_t* cent = &cg_entities[i];

		if (cent && ((i != CG_CrosshairPlayer() /*&& i != cam.specEnt*/) /*|| cam_freeview.integer*/))
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