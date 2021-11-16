#include "cg_local.h"

void Cam_Draw2d(void)
{
	if (cam_shownames.integer)
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
			CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, cent->lerpOrigin, skipNumber, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_CORPSE);

			if (trace.entityNum == i)
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

				size = Distance(cg.refdef.vieworg, org);
				if (!size) return;

				size = 200.0f / size;

				org[2] += 45;

				if (size > 0.8f) size = 0.8f;
				if (size < 0.3f) size = 0.3f;


				if (CG_WorldCoordToScreenCoordFloat(org, &x, &y))
				{
					char* text = cgs.clientinfo[i].name;

					x = (x - SCREEN_WIDTH/2)*cgs.widthRatioCoef + SCREEN_WIDTH / 2;

					int style = cam_shownamesStyle.integer;
					style = style > 6 ? 6 : style; // There are no styles above 6
					style = style < 0 ? 0 : style; // There are no styles below 0

					x -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
					//y -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
					CG_Text_Paint(x, y, size, colorLtGrey, text, 0, 0, style/*NORMAL*/, FONT_MEDIUM);
				}
			}
		}
	}
}