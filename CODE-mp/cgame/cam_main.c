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

			CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, cent->lerpOrigin, cg.snap->ps.clientNum, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_BODY | CONTENTS_CORPSE);

			if (trace.entityNum == i)
			{
				float x, y;
				vec3_t org;
				float size;

				VectorCopy(cent->lerpOrigin, org);
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

					x -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
					y -= CG_Text_Width(text, size, FONT_MEDIUM) / 2;
					CG_Text_Paint(x, y, size, colorLtGrey, text, 0, 0, 0/*NORMAL*/, FONT_MEDIUM);
				}
			}
		}
	}
}