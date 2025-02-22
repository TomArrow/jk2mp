// Filename:-	tr_font.h
//
// font support 

#ifndef TR_FONT_H
#define TR_FONT_H



#define GLYPH_MAX_KOREAN_SHADERS 3
#define GLYPH_MAX_TAIWANESE_SHADERS 4
#define GLYPH_MAX_JAPANESE_SHADERS 3
#define GLYPH_MAX_ASIAN_SHADERS 4	// this MUST equal the larger of the above defines

#define MAX_FONT_VARIANTS 8

class CFontInfo
{
private:
	// From the fontdat file
	glyphInfo_t		mGlyphs[GLYPH_COUNT];

	int				mPointSize;
	int				mHeight;
	int				mAscender;
	int				mDescender;
	
	int				mAsianHack;
	// end of fontdat data


	int				mShader;   				// handle to the shader with the glyph
	int				mShader3D;   			// handle to the shader with the glyph, 3D variant of the shader (needs to behave a bit differently, have depth checks etc)

	char			m_sFontName[MAX_QPATH];	// eg "fonts/lcd"	// needed for korean font-hint if we need >1 hangul set
	
	int				m_hAsianShaders[GLYPH_MAX_ASIAN_SHADERS];	// shaders for Korean glyphs where applicable		
	int				m_hAsianShaders3D[GLYPH_MAX_ASIAN_SHADERS];	// shaders for Korean glyphs where applicable, for 3D	
	glyphInfo_t		m_AsianGlyph;			// special glyph containing asian->western scaling info for all glyphs
	int				m_iAsianGlyphsAcross;	// needed to dynamically calculate S,T coords
	int				m_iAsianPagesLoaded;
	bool			m_bAsianLastPageHalfHeight;
	int				m_iAsianLanguageLoaded;	// doesn't matter what this is, so long as it's comparable as being changed

	CFontInfo* m_variants[MAX_FONT_VARIANTS];
	int				m_numVariants;
	int				m_handle;

public:
	bool			mbRoundCalcs;	// trying to make this !@#$%^ thing work with scaling

	CFontInfo(const char *fontName);
	CFontInfo(int fill) { memset(this, fill, sizeof(*this)); }
	~CFontInfo(void) {}

	int GetHandle();

	void AddVariant(CFontInfo* variant);
	int GetNumVariants();
	CFontInfo* GetVariant(int index);

	const int GetPointSize(void) const { return(mPointSize); }
	const int GetHeight(void) const { return(mHeight); }
	const int GetAscender(void) const { return(mAscender); }
	const int GetDescender(void) const { return(mDescender); }

	const glyphInfo_t *GetLetter(const unsigned int uiLetter, int *piShader = NULL,qboolean get3D = qfalse);
	const int GetAsianCode(ulong uiLetter) const;

	const int GetLetterWidth(const unsigned int uiLetter) const;
	const int GetLetterHorizAdvance(const unsigned int uiLetter) const;
	const int GetShader(void) const { return(mShader); }
	const int GetShader3D(void) const { return(mShader3D); }

	void FlagNoAsianGlyphs(void) { m_hAsianShaders[0] = 0; m_iAsianLanguageLoaded = -1; }	// used during constructor
	bool AsianGlyphsAvailable(void) const { return !!(m_hAsianShaders[0]); }

	void UpdateAsianIfNeeded( bool bForceReEval = false);
};

void R_ShutdownFonts(void);
void R_InitFonts(void);
int RE_RegisterFont(const char *psName);
int RE_Font_StrLenPixels(const char *psText, const int iFontHandle, const float fScale = 1.0f);
int RE_Font_StrLenChars(const char *psText);
int RE_Font_HeightPixels(const int iFontHandle, const float fScale = 1.0f);
void RE_Font_DrawString(float ox, float oy, const char *psText, const float *rgba, const int iFontHandle, int iCharLimit, const float fScale = 1.0f);
void RE_Font_DrawString_3D(vec_t* origin, vec_t* axis, const char* psText, const float* rgba, int iFontHandle, int iCharLimit, float fScale);
qboolean Language_IsAsian(void);
qboolean Language_UsesSpaces(void);
unsigned int AnyLanguage_ReadCharFromString( const char *psText, int *piAdvanceCount, qboolean *pbIsTrailingPunctuation/* = NULL*/ );


void RE_FontRatioFix(float ratio);


#endif	// #ifndef TR_FONT_H

// end

