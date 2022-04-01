/*
// this line must stay at top so the whole PCH thing works...
#include "cg_headers.h"

//#include "cg_local.h"
#include "cg_media.h"
#include "cg_text.h"
*/

// Copyright (C) 1999-2000 Id Software, Inc.
//
// cg_drawtools.c -- helper functions called by cg_draw, cg_scoreboard, cg_info, etc
#include "cg_local.h"
#include "../game/q_shared.h"


/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color ) {
	trap_R_SetColor( color );
	
	CG_DrawTopBottom(x, y, width, height, size);
	CG_DrawSides(x, y, width, height, size);
	
	trap_R_SetColor( NULL );
}



/*
=================
CG_GetColorForHealth
=================
*/
void CG_GetColorForHealth( int health, int armor, vec4_t hcolor ) {
	int		count;
	int		max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	if ( health <= 0 ) {
		VectorClear( hcolor );	// black
		hcolor[3] = 1;
		return;
	}
	count = armor;
	max = health * ARMOR_PROTECTION / ( 1.0 - ARMOR_PROTECTION );
	if ( max < count ) {
		count = max;
	}
	health += count;

	// set the color based on health
	hcolor[0] = 1.0;
	hcolor[3] = 1.0;
	if ( health >= 100 ) {
		hcolor[2] = 1.0;
	} else if ( health < 66 ) {
		hcolor[2] = 0;
	} else {
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) {
		hcolor[1] = 1.0;
	} else if ( health < 30 ) {
		hcolor[1] = 0;
	} else {
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}

/*
================
CG_DrawSides

Coords are virtual 640x480
================
*/
void CG_DrawSides(float x, float y, float w, float h, float size) {
	size *= cgs.screenXScale;
	trap_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
}

void CG_DrawTopBottom(float x, float y, float w, float h, float size) {
	size *= cgs.screenYScale;
	trap_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
	trap_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
}

/*
-------------------------
CGC_FillRect2
real coords
-------------------------
*/
void CG_FillRect2( float x, float y, float width, float height, const float *color ) {
	trap_R_SetColor( color );
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cgs.media.whiteShader);
	trap_R_SetColor( NULL );
}

/*
================
CG_FillRect

Coordinates are 640*480 virtual values
=================
*/
void CG_FillRect( float x, float y, float width, float height, const float *color ) {
	trap_R_SetColor( color );

	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cgs.media.whiteShader);

	trap_R_SetColor( NULL );
}


/*
================
CG_DrawPic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
=================
*/
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	trap_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

/*
================
CG_DrawRotatePic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
rotates around the upper right corner of the passed in point
=================
*/
void CG_DrawRotatePic( float x, float y, float width, float height,float angle, qhandle_t hShader ) {
	trap_R_DrawRotatePic( x, y, width, height, 0, 0, 1, 1, angle, hShader );
}

/*
================
CG_DrawRotatePic2

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
Actually rotates around the center point of the passed in coordinates
=================
*/
void CG_DrawRotatePic2( float x, float y, float width, float height,float angle, qhandle_t hShader ) {
	trap_R_DrawRotatePic2( x, y, width, height, 0, 0, 1, 1, angle, hShader );
}

/*
===============
CG_DrawChar

Coordinates and size in 640*480 virtual screen size
===============
*/
void CG_DrawChar( int x, int y, int width, int height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float	ax, ay, aw, ah;
	float size2;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;

	row = ch>>4;
	col = ch&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	trap_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size2, 
		cgs.media.charsetShader );

}


/* True Type functions */

int CG_Text_Width2(const char *text, float scale, int limit) {
  int count,len;
	float out;
	const mmeGlyphInfo_t *glyph;
	float useScale;
	const char *s = text;
	const mmeFontInfo_t *font = &cgs.textFont;
	useScale = scale * font->glyphScale;
	out = 0;
	if (text) {
		len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if (Q_IsColorStringHex(s + 1)) {
				int skipCount = 0;
				Q_parseColorHex(s + 1, 0, &skipCount);
				s += 1 + skipCount;
				continue;
			}
			else if ( ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) ) || Q_IsColorString( s ) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[(int)*s];
				out += glyph->xSkip;
				s++;
				count++;
			}
		}
	}
	return out * useScale;
}

int CG_Text_Height2(const char *text, float scale, int limit) {
	int len, count;
	float max;
	mmeGlyphInfo_t *glyph;
	float useScale;
	const char *s = text;
	mmeFontInfo_t *font = &cgs.textFont;
	useScale = scale * font->glyphScale;
	max = 0;
	if (text) {
		len = strlen(text);
		if (limit > 0 && len > limit) {
			len = limit;
		}
		count = 0;
		while (s && *s && count < len) {
			if (Q_IsColorStringHex(s + 1)) {
				int skipCount = 0;
				Q_parseColorHex(s + 1, 0, &skipCount);
				s += 1 + skipCount;
				continue;
			}
			else if ( ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) ) || Q_IsColorString( s ) ) {
				s += 2;
				continue;
			} else {
				glyph = &font->glyphs[(int)*s];

				if (max < glyph->height) {
					max = glyph->height;
				}
				s++;
				count++;
			}
		}
	}
	return max * useScale;
}

static void CG_Text_PaintChar(float x, float y, float width, float height, float scale, float s, float t, float s2, float t2, qhandle_t hShader) {
  float w, h;
  w = width * scale;
  h = height * scale;
  trap_R_DrawStretchPic( x, y, w, h, s, t, s2, t2, hShader );
}

void CG_Text_Paint2(float x, float y, float scale, vec4_t color, const char *text, qboolean shadowed ) {
	int len, count;
	vec4_t newColor;
	mmeGlyphInfo_t *glyph;
	float useScale;
	mmeFontInfo_t *font = &cgs.textFont;
	useScale = scale * font->glyphScale;
	if (text) {
		const char *s = text;
		trap_R_SetColor( color );
		memcpy(&newColor[0], &color[0], sizeof(vec4_t));
		len = strlen(text);
		count = 0;
		while (s && *s && count < len) {
			int colorLen;
			glyph = &font->glyphs[(int)*s]; // TTimo: FIXME: getting nasty warnings without the cast, hopefully this doesn't break the VM build
			//int yadj = Assets.textFont.glyphs[text[i]].bottom + Assets.textFont.glyphs[text[i]].top;
			//float yadj = scale * (Assets.textFont.glyphs[text[i]].imageHeight - Assets.textFont.glyphs[text[i]].height);
			if (Q_IsColorStringHex(s + 1)) {
				int skipCount = 0;
				Q_parseColorHex(s + 1, newColor, &skipCount);
				s += 1 + skipCount;
				//newColor[3] = color[3]; // eeeh.
				trap_R_SetColor(newColor);
				continue;
			}
			else if ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) ) {
				memcpy( newColor, g_color_table_nt[ColorIndexNT(*(s+1))], sizeof( newColor ) );
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			} else if ( Q_IsColorString( s ) || Q_IsColorString_1_02(s) || Q_IsColorString_Extended(s)) {
				memcpy( newColor, g_color_table[ColorIndex(*(s+1))], sizeof( newColor ) );
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			} else {
				float yadj = useScale * glyph->top;
				if ( shadowed ) {
					int ofs = 2;
					colorBlack[3] = newColor[3];
					trap_R_SetColor( colorBlack );
					CG_Text_PaintChar(x + ofs, y - yadj + ofs, 
														glyph->imageWidth,
														glyph->imageHeight,
														useScale, 
														glyph->s,
														glyph->t,
														glyph->s2,
														glyph->t2,
														glyph->glyph);
					colorBlack[3] = 1.0;
					trap_R_SetColor( newColor );
				}
				CG_Text_PaintChar(x, y - yadj, 
					glyph->imageWidth, glyph->imageHeight,
					useScale, glyph->s,	glyph->t, glyph->s2, glyph->t2, glyph->glyph);
				x += (glyph->xSkip * useScale);
				s++;
				count++;
			}
		}
		trap_R_SetColor( NULL );
	}
}


/*
==================
CG_DrawStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
#include "../../ui/menudef.h"	// for "ITEM_TEXTSTYLE_SHADOWED"
void CG_DrawStringExt( int x, int y, const char *string, const float *setColor, 
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight, int maxChars )
{
	if (trap_Language_IsAsian())
	{
		// hack-a-doodle-do (post-release quick fix code)...
		//
		vec4_t color;
		memcpy(color,setColor, sizeof(color));	// de-const it
		CG_Text_Paint(x, y, 1.0f,	// float scale, 
						color,		// vec4_t color, 
						string,		// const char *text, 
						0.0f,		// float adjust, 
						0,			// int limit, 
						shadow ? ITEM_TEXTSTYLE_SHADOWED : 0,	// int style, 
						FONT_MEDIUM		// iMenuFont
						) ;
	}
	else
	{
		vec4_t		color;
		const char	*s;
		int			xx;

		// draw the drop shadow
		if (shadow) {
			color[0] = color[1] = color[2] = 0;
			color[3] = setColor[3];
			trap_R_SetColor( color );
			s = string;
			xx = x;
			while ( *s ) {
				if (Q_IsColorStringHex(s + 1)) {
					int skipCount = 0;
					Q_parseColorHex(s + 1, 0, &skipCount);
					s += 1 + skipCount;
					continue;
				}
				else if ( ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) ) || Q_IsColorString( s ) ) {
					s += 2;
					continue;
				}
				CG_DrawChar( xx + 2*cgs.widthRatioCoef, y + 2, charWidth, charHeight, *s );
				xx += charWidth;
				s++;
			}
		}

		// draw the colored text
		s = string;
		xx = x;
		trap_R_SetColor( setColor );
		while ( *s ) {
			if (Q_IsColorStringHex(s + 1)) {
				int skipCount = 0;
				Q_parseColorHex(s + 1, color, &skipCount);
				s += 1 + skipCount;
				if (!forceColor) {
					//color[3] = setColor[3]; // eeeh.
					trap_R_SetColor(color);
				}
				continue;
			}
			else if ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) ) {
				if ( !forceColor ) {
					memcpy( color, g_color_table_nt[ColorIndexNT(*(s+1))], sizeof( color ) );
					color[3] = setColor[3];
					trap_R_SetColor( color );
				}
				s += 2;
				continue;
			} else if ( Q_IsColorString( s ) || Q_IsColorString_1_02(s) || Q_IsColorString_Extended(s)) {
				if ( !forceColor ) {
					memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
					color[3] = setColor[3];
					trap_R_SetColor( color );
				}
				s += 2;
				continue;
			}
			CG_DrawChar( xx, y, charWidth, charHeight, *s );
			xx += charWidth;
			s++;
		}
		trap_R_SetColor( NULL );
	}
}

void CG_DrawBigString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt( x, y, s, color, qfalse, qtrue, BIGCHAR_WIDTH*cgs.widthRatioCoef, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
}

void CG_DrawSmallString( int x, int y, const char *s, float alpha ) {
	float	color[4];

	color[0] = color[1] = color[2] = 1.0;
	color[3] = alpha;
	CG_DrawStringExt( x, y, s, color, qfalse, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT, 0 );
}

/*
=================
CG_DrawStrlen

Returns character count, skiping color escape codes
=================
*/
int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if (Q_IsColorStringHex(s + 1)) {
			int skipCount = 0;
			Q_parseColorHex(s + 1, 0, &skipCount);
			s += 1 + skipCount;
		}
		else if ( ( demo15detected && cg.ntModDetected && Q_IsColorStringNT( s ) )
			|| Q_IsColorString( s ) || Q_IsColorString_1_02(s) || Q_IsColorString_Extended(s)) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
=============
CG_TileClearBox

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
static void CG_TileClearBox( int x, int y, int w, int h, qhandle_t hShader ) {
	float	s1, t1, s2, t2;

	s1 = x/64.0;
	t1 = y/64.0;
	s2 = (x+w)/64.0;
	t2 = (y+h)/64.0;
	trap_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}



/*
==============
CG_TileClear

Clear around a sized down screen
==============
*/
void CG_TileClear( void ) {
	int		top, bottom, left, right;
	int		w, h;

	w = cgs.glconfig.vidWidth;
	h = cgs.glconfig.vidHeight;

	if ( cg.refdef.x == 0 && cg.refdef.y == 0 && 
		cg.refdef.width == w && cg.refdef.height == h ) {
		return;		// full screen rendering
	}

	top = cg.refdef.y;
	bottom = top + cg.refdef.height-1;
	left = cg.refdef.x;
	right = left + cg.refdef.width-1;

	// clear above view screen
	CG_TileClearBox( 0, 0, w, top, cgs.media.backTileShader );

	// clear below view screen
	CG_TileClearBox( 0, bottom, w, h - bottom, cgs.media.backTileShader );

	// clear left of view screen
	CG_TileClearBox( 0, top, left, bottom - top + 1, cgs.media.backTileShader );

	// clear right of view screen
	CG_TileClearBox( right, top, w - right, bottom - top + 1, cgs.media.backTileShader );
}



/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( int startMsec, int totalMsec ) {
	static vec4_t color;
	float t;

	if ( startMsec == 0 ) {
		return NULL;
	}

	t = (cg.time - startMsec) + cg.timeFraction;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// this color shouldn't be visible yet
	if (t < 0){
		return NULL;
	}

	// fade out
	if ( totalMsec - t < FADE_TIME ) {
		color[3] = ((float)totalMsec - t) * 1.0f / FADE_TIME;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	return color;
}


/*
=================
CG_ColorForHealth
=================
*/
void CG_ColorForGivenHealth( vec4_t hcolor, int health ) 
{
	// set the color based on health
	hcolor[0] = 1.0;
	if ( health >= 100 ) 
	{
		hcolor[2] = 1.0;
	} 
	else if ( health < 66 ) 
	{
		hcolor[2] = 0;
	} 
	else 
	{
		hcolor[2] = ( health - 66 ) / 33.0;
	}

	if ( health > 60 ) 
	{
		hcolor[1] = 1.0;
	} 
	else if ( health < 30 ) 
	{
		hcolor[1] = 0;
	} 
	else 
	{
		hcolor[1] = ( health - 30 ) / 30.0;
	}
}

/*
=================
CG_ColorForHealth
=================
*/
void CG_ColorForHealth( vec4_t hcolor ) 
{
	int		health;
	int		count;
	int		max;

	// calculate the total points of damage that can
	// be sustained at the current health / armor level
	health = cg.snap->ps.stats[STAT_HEALTH];

	if ( health <= 0 ) 
	{
		VectorClear( hcolor );	// black
		hcolor[3] = 1;
		return;
	}

	count = cg.snap->ps.stats[STAT_ARMOR];
	max = health * ARMOR_PROTECTION / ( 1.0 - ARMOR_PROTECTION );
	if ( max < count ) 
	{
		count = max;
	}
	health += count;

	hcolor[3] = 1.0;
	CG_ColorForGivenHealth( hcolor, health );
}

/*
==============
CG_DrawNumField

Take x,y positions as if 640 x 480 and scales them to the proper resolution

==============
*/
void CG_DrawNumField (float x, float y, int width, float value,float charWidth,float charHeight,int style,qboolean zeroFill) 
{
	char	num[16], *ptr;
	int		l;
	int		frame;
	float	xWidth;
	int		i = 0;

	if (width < 1) {
		return;
	}

	// draw number string
	if (width > 5) {
		width = 5;
	}

	switch ( width ) {
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf (num, sizeof(num), "%i", (int)value);
	l = strlen(num);
	if (l > width)
		l = width;

	// FIXME: Might need to do something different for the chunky font??
	switch(style)
	{
	case NUM_FONT_SMALL:
		xWidth = charWidth;
		break;
	case NUM_FONT_CHUNKY:
		xWidth = (charWidth/1.2f) + 2.0f;
		break;
	default:
	case NUM_FONT_BIG:
		xWidth = (charWidth/2.0f) + 7.0f;//(charWidth/6);
		break;
	}

	if ( zeroFill )
	{
		for (i = 0; i < (width - l); i++ )
		{
			switch(style)
			{
			case NUM_FONT_SMALL:
				CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.smallnumberShaders[0] );
				break;
			case NUM_FONT_CHUNKY:
				CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.chunkyNumberShaders[0] );
				break;
			default:
			case NUM_FONT_BIG:
				CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.numberShaders[0] );
				break;
			}
			x += 2 + (xWidth)*cgs.widthRatioCoef;
		}
	}
	else
	{
		x += (2 + (xWidth)*(width - l))*cgs.widthRatioCoef;
	}

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		switch(style)
		{
		case NUM_FONT_SMALL:
			CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.smallnumberShaders[frame] );
			x++;	// For a one line gap
			break;
		case NUM_FONT_CHUNKY:
			CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.chunkyNumberShaders[frame] );
			break;
		default:
		case NUM_FONT_BIG:
			CG_DrawPic( x,y, charWidth*cgs.widthRatioCoef, charHeight, cgs.media.numberShaders[frame] );
			break;
		}

		x += (xWidth)*cgs.widthRatioCoef;
		ptr++;
		l--;
	}

}

#include "../ui/ui_shared.h"	// for some text style junk
void UI_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color ) 
{
	// having all these different style defines (1 for UI, one for CG, and now one for the re->font stuff) 
	//	is dumb, but for now...
	//
	int iStyle = 0;
	char s[1024];
	int iMenuFont = (style & UI_SMALLFONT) ? FONT_SMALL : FONT_MEDIUM;

	// the best fix in the world
	Q_strncpyz(s, str, sizeof(s) - 2);
	if (demo15detected && cg.ntModDetected)
		Q_StripColorNewNT(s);
	else
		Q_StripColorNew(s);

	switch (style & (UI_LEFT|UI_CENTER|UI_RIGHT))
	{
		default:
		case UI_LEFT:
		{
			// nada...
		}
		break;

		case UI_CENTER:
		{
//			x -= CG_Text_Width(str, 1.0, iMenuFont) / 2;
			x -= CG_Text_Width(s, 1.0, iMenuFont) / 2;
		}
		break;

		case UI_RIGHT:
		{
//			x -= CG_Text_Width(str, 1.0, iMenuFont) / 2;
			x -= CG_Text_Width(s, 1.0, iMenuFont) / 2;
		}
		break;
	}

	if (style & UI_DROPSHADOW)
	{
		iStyle = ITEM_TEXTSTYLE_SHADOWED;
	}
	else
	if ( style & (UI_BLINK|UI_PULSE) )
	{
		iStyle = ITEM_TEXTSTYLE_BLINK;
	}

	CG_Text_Paint(x, y, 1.0, color, str, 0, 0, iStyle, iMenuFont);
}

void UI_DrawScaledProportionalString( int x, int y, const char* str, int style, vec4_t color, float scale) 
{
	// having all these different style defines (1 for UI, one for CG, and now one for the re->font stuff) 
	//	is dumb, but for now...
	//
	int iStyle = 0;

	switch (style & (UI_LEFT|UI_CENTER|UI_RIGHT))
	{
		default:
		case UI_LEFT:
		{
			// nada...
		}
		break;

		case UI_CENTER:
		{
			x -= CG_Text_Width(str, scale, FONT_MEDIUM) / 2;
		}
		break;

		case UI_RIGHT:
		{
			x -= CG_Text_Width(str, scale, FONT_MEDIUM) / 2;
		}
		break;
	}

	if (style & UI_DROPSHADOW)
	{
		iStyle = ITEM_TEXTSTYLE_SHADOWED;
	}
	else
	if ( style & (UI_BLINK|UI_PULSE) )
	{
		iStyle = ITEM_TEXTSTYLE_BLINK;
	}

	CG_Text_Paint(x, y, scale, color, str, 0, 0, iStyle, FONT_MEDIUM);
}




