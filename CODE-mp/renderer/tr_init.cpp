// tr_init.c -- functions that are not called every frame


#include "tr_local.h"
#include <cmath>

#ifndef DEDICATED
#if !defined __TR_WORLDEFFECTS_H
	#include "tr_WorldEffects.h"
#endif
#endif //!DEDICATED

#include "tr_font.h"

#ifdef G2_COLLISION_ENABLED
#if !defined (MINIHEAP_H_INC)
	#include "../qcommon/MiniHeap.h"
#endif

#include "../ghoul2/G2_local.h"
#endif


extern float EvalWaveForm(const waveForm_t* wf);
extern void ParseWaveformAlone(char** text, waveForm_t* output);

//#ifdef __USEA3D
//// Defined in snd_a3dg_refcommon.c
//void RE_A3D_RenderGeometry (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
//#endif

#ifdef G2_COLLISION_ENABLED
CMiniHeap *G2VertSpaceServer = NULL;
#endif

#ifndef DEDICATED
glconfig_t	glConfig;
glstate_t	glState;
static void GfxInfo_f( void );

#endif 

glMMEConfig_t	glMMEConfig;

cvar_t	*r_hdr;
cvar_t	*r_flareSize;
cvar_t	*r_flareFade;

cvar_t	*r_ignoreFastPath;

cvar_t	*r_verbose;
cvar_t	*r_ignore;

cvar_t	*r_displayRefresh;

cvar_t	*r_detailTextures;

cvar_t	*r_znear;				// near Z clip plane
cvar_t	*r_zinvert;				// invert z buffer
cvar_t	*r_zproj;				// z distance of projection plane
cvar_t	*r_stereoSeparation;	// separation of cameras for stereo capture

cvar_t	*r_smp;
cvar_t	*r_showSmp;
cvar_t	*r_skipBackEnd;

cvar_t	*r_ignorehwgamma;
cvar_t	*r_measureOverdraw;

cvar_t	*r_inGameVideo;
cvar_t	*r_fastsky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
cvar_t	*r_dlightBacks;

cvar_t* r_skyboxRotate;			// Degrees on height axis to rotate skybox (to align landscapes etc)

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;
cvar_t	*r_autolodscalevalue;

cvar_t	*r_newDLights;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_showcluster;
cvar_t	*r_nocurves;

cvar_t	*r_dlightStyle;
cvar_t	*r_surfaceSprites;
cvar_t	*r_surfaceWeather;

cvar_t	*r_windSpeed;
cvar_t	*r_windAngle;
cvar_t	*r_windGust;
cvar_t	*r_windDampFactor;
cvar_t	*r_windPointForce;
cvar_t	*r_windPointX;
cvar_t	*r_windPointY;

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_compressed_lightmaps;
cvar_t	*r_ext_preferred_tc_method;
cvar_t	*r_ext_gamma_control;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;
cvar_t	*r_ext_texture_filter_anisotropic;

#ifdef JEDIACADEMY_GLOW
cvar_t	*r_DynamicGlow;
cvar_t	*r_DynamicGlowPasses;
cvar_t	*r_DynamicGlowDelta;
cvar_t	*r_DynamicGlowIntensity;
cvar_t	*r_DynamicGlowSoft;
cvar_t	*r_DynamicGlowWidth;
cvar_t	*r_DynamicGlowHeight;
#endif

// Gamma handling variables
cvar_t* r_gammaSrgbLightmaps;
cvar_t* r_gammaSrgbTextures;
cvar_t* r_gammaLegacy;
cvar_t* r_gammaLegacyPrecision;
cvar_t* r_gammaSrgbLightvalues;
cvar_t* r_HUDBrightness;

cvar_t* r_hideMissingModels;


cvar_t	*r_ignoreGLErrors;
cvar_t	*r_logFile;

cvar_t	*r_stencilbits;
cvar_t	*r_depthbits;
cvar_t	*r_colorbits;
cvar_t	*r_stereo;
cvar_t	*r_primitives;
cvar_t	*r_texturebits;
cvar_t	*r_texturebitslm;

cvar_t	*r_multiSample;
cvar_t	*r_multiSampleNvidia;

cvar_t	*r_drawBuffer;
cvar_t	*r_lightmap;
cvar_t	*r_vertexLight;
cvar_t	*r_uiFullScreen;
cvar_t	*r_shadows;
cvar_t	*r_flares;
cvar_t	*r_mode;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_showtris;
cvar_t	*r_showsky;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_swapInterval;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_fullscreen;
cvar_t	*r_noborder;

cvar_t	*r_customwidth;
cvar_t	*r_customheight;
cvar_t	*r_customaspect;

cvar_t	*r_overBrightBits;
cvar_t	*r_fboOverbright;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;

cvar_t	*r_maxpolys;
int		max_polys;
cvar_t	*r_maxpolyverts;
int		max_polyverts;

cvar_t	*r_modelpoolmegs;

cvar_t* r_drawAllAreas;

cvar_t* r_convertModelBones;
cvar_t* r_loadSkinsJKA;

/*
Ghoul2 Insert Start
*/

cvar_t	*r_noServerGhoul2;
cvar_t	*r_Ghoul2AnimSmooth=0;
cvar_t	*r_Ghoul2UnSqashAfterSmooth=0;
//cvar_t	*r_Ghoul2UnSqash;
//cvar_t	*r_Ghoul2TimeBase=0; from single player
//cvar_t	*r_Ghoul2NoLerp;
//cvar_t	*r_Ghoul2NoBlend;
//cvar_t	*r_Ghoul2BlendMultiplier=0;

/*
Ghoul2 Insert End
*/

cvar_t* r_fontSharpness;
cvar_t* r_font3DBrightness;

#ifndef DEDICATED

void ( APIENTRY * qglMultiTexCoord2fARB )( GLenum texture, GLfloat s, GLfloat t );
void ( APIENTRY * qglActiveTextureARB )( GLenum texture );
void ( APIENTRY * qglClientActiveTextureARB )( GLenum texture );

void ( APIENTRY * qglLockArraysEXT) (GLint, GLint);
void ( APIENTRY * qglUnlockArraysEXT) (void);

void ( APIENTRY * qglPointParameterfEXT)( GLenum, GLfloat);
void ( APIENTRY * qglPointParameterfvEXT)( GLenum, GLfloat *);

// Declare Register Combiners function pointers.
PFNGLCOMBINERPARAMETERFVNVPROC				qglCombinerParameterfvNV = NULL;
PFNGLCOMBINERPARAMETERIVNVPROC				qglCombinerParameterivNV = NULL;
PFNGLCOMBINERPARAMETERFNVPROC				qglCombinerParameterfNV = NULL;
PFNGLCOMBINERPARAMETERINVPROC				qglCombinerParameteriNV = NULL;
PFNGLCOMBINERINPUTNVPROC				qglCombinerInputNV = NULL;
PFNGLCOMBINEROUTPUTNVPROC				qglCombinerOutputNV = NULL;
PFNGLFINALCOMBINERINPUTNVPROC				qglFinalCombinerInputNV = NULL;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC			qglGetCombinerInputParameterfvNV = NULL;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC			qglGetCombinerInputParameterivNV = NULL;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC			qglGetCombinerOutputParameterfvNV = NULL;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC			qglGetCombinerOutputParameterivNV = NULL;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC		qglGetFinalCombinerInputParameterfvNV = NULL;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC		qglGetFinalCombinerInputParameterivNV = NULL;

// Declare Vertex and Fragment Program function pointers.
PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB = NULL;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC qglProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC qglProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC qglProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC qglProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC qglProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC qglProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC qglProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC qglProgramLocalParameter4fvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC qglGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC qglGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC qglGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC qglGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB = NULL;
PFNGLISPROGRAMARBPROC qglIsProgramARB = NULL;
//teh's PBO
PFNGLGENBUFFERSARBPROC qglGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC qglBindBufferARB = NULL;
PFNGLBUFFERDATAARBPROC qglBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC qglMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC qglUnmapBufferARB = NULL;

#ifdef _WIN32
// Declare Render-Texture function pointers.
PFNWGLBINDTEXIMAGEARBPROC				qwglBindTexImageARB = NULL;
PFNWGLRELEASETEXIMAGEARBPROC			qwglReleaseTexImageARB = NULL;
PFNWGLSETPBUFFERATTRIBARBPROC			qwglSetPbufferAttribARB = NULL;
#endif

void RE_SetLightStyle(int style, int color);

void RE_GetBModelVerts( int bmodelIndex, vec3_t *verts, vec3_t normal );

#endif // !DEDICATED


static void AssertCvarRange( cvar_t *cv, float minVal, float maxVal, qboolean shouldBeIntegral )
{
	if ( shouldBeIntegral )
	{
		if ( ( int ) cv->value != cv->integer )
		{
			ri.Printf( PRINT_WARNING, "WARNING: cvar '%s' must be integral (%f)\n", cv->name, cv->value );
			ri.Cvar_Set( cv->name, va( "%d", cv->integer ) );
		}
	}

	if ( cv->value < minVal )
	{
		ri.Printf( PRINT_WARNING, "WARNING: cvar '%s' out of range (%f < %f)\n", cv->name, cv->value, minVal );
		ri.Cvar_Set( cv->name, va( "%f", minVal ) );
	}
	else if ( cv->value > maxVal )
	{
		ri.Printf( PRINT_WARNING, "WARNING: cvar '%s' out of range (%f > %f)\n", cv->name, cv->value, maxVal );
		ri.Cvar_Set( cv->name, va( "%f", maxVal ) );
	}
}

#ifndef DEDICATED

/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if ( glConfig.vidWidth == 0 )
	{
		GLimp_Init();
		GL_SetDefaultState();
		// print info the first time only
		GfxInfo_f();

		glMMEConfig.glWidth = glConfig.vidWidth;
		glMMEConfig.glHeight = glConfig.vidHeight;

#ifdef CAPTURE_FLOAT
		R_FrameBuffer_Init();
#endif
	}
	else
	{
		// set default state
		GL_SetDefaultState();
	}

	// init command buffers and SMP
	R_InitCommandBuffers();

	// set default state
	GL_SetDefaultState();
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors( void ) {
    int		err;
    char	s[64];

    err = qglGetError();
    if ( err == GL_NO_ERROR ) {
        return;
    }
    if ( r_ignoreGLErrors->integer ) {
        return;
    }
    switch( err ) {
        case GL_INVALID_ENUM:
            strcpy( s, "GL_INVALID_ENUM" );
            break;
        case GL_INVALID_VALUE:
            strcpy( s, "GL_INVALID_VALUE" );
            break;
        case GL_INVALID_OPERATION:
            strcpy( s, "GL_INVALID_OPERATION" );
            break;
        case GL_STACK_OVERFLOW:
            strcpy( s, "GL_STACK_OVERFLOW" );
            break;
        case GL_STACK_UNDERFLOW:
            strcpy( s, "GL_STACK_UNDERFLOW" );
            break;
        case GL_OUT_OF_MEMORY:
            strcpy( s, "GL_OUT_OF_MEMORY" );
            break;
        default:
            Com_sprintf( s, sizeof(s), "%i", err);
            break;
    }

    ri.Error( ERR_FATAL, "GL_CheckErrors: %s", s );
}

#endif //!DEDICATED

/*
** R_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int         width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

vidmode_t r_vidModes[] = {
    { "Mode  0: 320x240",		320,	240,	1 },
    { "Mode  1: 400x300",		400,	300,	1 },
    { "Mode  2: 512x384",		512,	384,	1 },
    { "Mode  3: 640x480",		640,	480,	1 },
    { "Mode  4: 800x600",		800,	600,	1 },
    { "Mode  5: 960x720",		960,	720,	1 },
    { "Mode  6: 1024x768",		1024,	768,	1 },
    { "Mode  7: 1152x864",		1152,	864,	1 },
    { "Mode  8: 1280x1024",		1280,	1024,	1 },
    { "Mode  9: 1600x1200",		1600,	1200,	1 },
    { "Mode 10: 2048x1536",		2048,	1536,	1 },
    { "Mode 11: 856x480 (wide)",856,	480,	1 }
};
static int	s_numVidModes = ( sizeof( r_vidModes ) / sizeof( r_vidModes[0] ) );

qboolean R_GetModeInfo( int *width, int *height, float *windowAspect, int mode ) {
	vidmode_t	*vm;

    if ( mode < -1 ) {
        return qfalse;
	}
	if ( mode >= s_numVidModes ) {
		return qfalse;
	}

	if ( mode == -1 ) {
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		*windowAspect = r_customaspect->value;
		return qtrue;
	}

	vm = &r_vidModes[mode];

    *width  = vm->width;
    *height = vm->height;
    *windowAspect = (float)vm->width / ( vm->height * vm->pixelAspect );

    return qtrue;
}

/*
** R_ModeList_f
*/
static void R_ModeList_f( void ) {
	int i;
	ri.Printf( PRINT_ALL, "\n" );
	for ( i = 0; i < s_numVidModes; i++ ) {
		ri.Printf( PRINT_ALL, "%s\n", r_vidModes[i].description );
	}
	ri.Printf( PRINT_ALL, "\n" );
}


/* 
============================================================================== 
 
						SCREEN SHOTS 
 
============================================================================== 
*/ 
#ifndef DEDICATED
/*
==================
RB_TakeScreenshotCmd
==================
*/
const void *RB_ScreenShotCmd( const void *data ) {
	const screenShotCommand_t *cmd = (const screenShotCommand_t *)data;
	byte *inBuf, *outBuf;
	int w, h, outSize;

	w = glConfig.vidWidth;
	h = glConfig.vidHeight;
	outSize = w * h * 4;
	inBuf = (byte *)ri.Hunk_AllocateTempMemory( outSize * 2 );
	outBuf = inBuf + outSize;

	qglReadPixels( 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, inBuf ); 
	if ( ( mme_screenShotGamma->integer || (tr.overbrightBits > 0) ) && (glConfig.deviceSupportsGamma ) ) {
		if (!r_fboOverbright->integer) { // All a bit shitty. Should fix it up a bit someday.
			R_GammaCorrect(inBuf, outSize);
		}
	}
	switch ( cmd->format ) {
	case mmeShotFormatJPG:
		outSize = SaveJPG( mme_jpegQuality->integer, w, h, mmeShotTypeRGB, inBuf, outBuf, outSize );
		break;
	case mmeShotFormatTGA:
		outSize = SaveTGA( mme_tgaCompression->integer, w, h, mmeShotTypeRGB, inBuf, outBuf, outSize );
		break;
	case mmeShotFormatPNG:
		outSize = SavePNG( mme_pngCompression->integer, w, h, mmeShotTypeRGB, inBuf, outBuf, outSize );
		break;
	default:
		outSize = 0;
	}
	if (outSize)
		ri.FS_WriteFile( cmd->name, outBuf, outSize );
	ri.Hunk_FreeTempMemory( inBuf );
	return (const void *)(cmd + 1);	
}


/*
====================
R_LevelShot

levelshots are specialized 256*256 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
#define LEVELSHOTSIZE 256
static void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source;
	byte		*src, *dst;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	sprintf( checkname, "levelshots/%s.tga", tr.world->baseName );

	source = (unsigned char *)ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight * 3 );

	buffer = (unsigned char *)ri.Hunk_AllocateTempMemory( LEVELSHOTSIZE * LEVELSHOTSIZE*3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = LEVELSHOTSIZE & 255;
	buffer[13] = LEVELSHOTSIZE >> 8;
	buffer[14] = LEVELSHOTSIZE & 255;
	buffer[15] = LEVELSHOTSIZE >> 8;
	buffer[16] = 24;	// pixel size

	qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_RGB, GL_UNSIGNED_BYTE, source ); 

	// resample from source
	xScale = glConfig.vidWidth / (4.0*LEVELSHOTSIZE);
	yScale = glConfig.vidHeight / (3.0*LEVELSHOTSIZE);
	for ( y = 0 ; y < LEVELSHOTSIZE ; y++ ) {
		for ( x = 0 ; x < LEVELSHOTSIZE ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + 3 * ( glConfig.vidWidth * (int)( (y*3+yy)*yScale ) + (int)( (x*4+xx)*xScale ) );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * LEVELSHOTSIZE + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if ( (mme_screenShotGamma->integer || ( tr.overbrightBits > 0 )) && glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, LEVELSHOTSIZE * LEVELSHOTSIZE*3 + 18 );

	ri.Hunk_FreeTempMemory( buffer );
	ri.Hunk_FreeTempMemory( source );

	ri.Printf( PRINT_ALL, "Wrote %s\n", checkname );
}

void R_ScreenShot( const char *shotName, mmeShotFormat_t shotFormat ) {
	screenShotCommand_t *cmd;
	
	if ( !tr.registered ) {
		return;
	}
	cmd = (screenShotCommand_t *)R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SCREENSHOT;
	Q_strncpyz( cmd->name, shotName, sizeof( cmd->name ));
	cmd->format = shotFormat;
}


/* 
================== 
R_ScreenShotTGA_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
================== 
*/  

static qboolean R_ScreenShotName( const char *start, const char *ext, char *fileName) {
	int i;
	for (i=0;i<10000;i++) {
		Com_sprintf( fileName, MAX_OSPATH, "screenshots/%s.%04d.%s", 
			start, i, ext );
		if (!ri.FS_FileExists( fileName))
			return qtrue;
	}
	Com_Printf("Screenshot limit reached\n");
	return qtrue;
}

static void R_ScreenShot_f (const char *ext, mmeShotFormat_t shotFormat) {
	char	fileName[MAX_OSPATH];
	const char	*cmd = ri.Cmd_Argv(1);
	qboolean silent = qfalse;

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) && shotFormat != mmeShotFormatPNG ) {
		R_LevelShot();
		return;
	}
	if (!strcmp( cmd, "silent" ) )
		silent = qtrue;
	if (!cmd[0] || silent)
		cmd = "shot";	
		
	if (R_ScreenShotName( cmd, ext, fileName)) {
		if (!silent)
			ri.Printf( PRINT_ALL, "Saving shot %s\n", fileName );
		R_ScreenShot( fileName, shotFormat );
	}
} 

static void R_ScreenShotTGA_f (void) {
	R_ScreenShot_f("tga", mmeShotFormatTGA );
} 

static void R_ScreenShotJPEG_f (void) {
	R_ScreenShot_f("jpg", mmeShotFormatJPG );
} 

static void R_ScreenShotPNG_f (void) {
	R_ScreenShot_f("png", mmeShotFormatPNG );
} 

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState( void ) {
	qglClearDepth(1.0f);

	qglCullFace(GL_FRONT);

	qglColor4f (1,1,1,1);

	// initialize downstream texture unit if we're running
	// in a multitexture environment
	if ( qglActiveTextureARB ) {
		GL_SelectTexture( 1 );
		GL_TextureMode( r_textureMode->string );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		GL_SelectTexture( 0 );
	}

	qglEnable(GL_TEXTURE_2D);
	GL_TextureMode( r_textureMode->string );
	GL_TexEnv( GL_MODULATE );

	qglShadeModel( GL_SMOOTH );

	qglDepthFunc(GL_LEQUAL);

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	qglEnableClientState (GL_VERTEX_ARRAY);

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask( GL_TRUE );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );
}

/*
================
R_PrintLongString

Workaround for ri.Printf's 4096 characters buffer limit.
================
*/
void R_PrintLongString(const char *string) {
	char buffer[4096];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		ri.Printf( PRINT_ALL, "%s", buffer );
		p += 4095;
		size -= 4095;
	}
}

/*
================
GfxInfo_f
================
*/
void GfxInfo_f( void ) 
{
	cvar_t *sys_cpustring = ri.Cvar_Get( "sys_cpustring", "", 0 );
	const char *enablestrings[] =
	{
		"disabled",
		"enabled"
	};
	const char *fsstrings[] =
	{
		"windowed",
		"fullscreen"
	};
	const char *noborderstrings[] =
	{
		"",
		"noborder "
	};

	const char *tc_table[] = 
	{
		"None",
		"GL_S3_s3tc",
		"GL_EXT_texture_compression_s3tc",
	};

	
	ri.Printf( PRINT_ALL, "GL_EXTENSIONS: " );
	R_PrintLongString( glConfig.extensions_string );
	ri.Printf( PRINT_ALL, "\n" );
	ri.Printf( PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	ri.Printf( PRINT_ALL, "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", glConfig.maxActiveTextures );
	ri.Printf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	ri.Printf(PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string);
	ri.Printf(PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string);
	ri.Printf(PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string);
	//ri.Printf( PRINT_ALL, "MODE: %d, %d x %d %s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, fsstrings[r_fullscreen->integer == 1] );
	ri.Printf( PRINT_ALL, "MODE: %d, %d x %d %s%s hz:", r_mode->integer, glConfig.vidWidth, glConfig.vidHeight, r_fullscreen->integer == 0 ? noborderstrings[r_noborder->integer == 1] : noborderstrings[0] ,fsstrings[r_fullscreen->integer == 1] );
	if ( glConfig.displayFrequency )
	{
		ri.Printf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		ri.Printf( PRINT_ALL, "N/A\n" );
	}
	if ( glConfig.deviceSupportsGamma )
	{
		ri.Printf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	}
	else
	{
		ri.Printf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}
	ri.Printf( PRINT_ALL, "CPU: %s\n", sys_cpustring->string );

	// rendering primitives
	{
		int		primitives;

		// default is to use triangles if compiled vertex arrays are present
		ri.Printf( PRINT_ALL, "rendering primitives: " );
		primitives = r_primitives->integer;
		if ( primitives == 0 ) {
			if ( qglLockArraysEXT ) {
				primitives = 2;
			} else {
				primitives = 1;
			}
		}
		if ( primitives == -1 ) {
			ri.Printf( PRINT_ALL, "none\n" );
		} else if ( primitives == 2 ) {
			ri.Printf( PRINT_ALL, "single glDrawElements\n" );
		} else if ( primitives == 1 ) {
			ri.Printf( PRINT_ALL, "multiple glArrayElement\n" );
		} else if ( primitives == 3 ) {
			ri.Printf( PRINT_ALL, "multiple glColor4ubv + glTexCoord2fv + glVertex3fv\n" );
		}
	}

	ri.Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode->string );
	ri.Printf( PRINT_ALL, "picmip: %d\n", r_picmip->integer );
	ri.Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits->integer );
	ri.Printf( PRINT_ALL, "lightmap texture bits: %d\n", r_texturebitslm->integer );
	ri.Printf( PRINT_ALL, "multitexture: %s\n", enablestrings[qglActiveTextureARB != 0] );
	ri.Printf( PRINT_ALL, "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] );
	ri.Printf( PRINT_ALL, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	ri.Printf( PRINT_ALL, "compressed textures: %s\n", enablestrings[glConfig.textureCompression != TC_NONE] );
	ri.Printf( PRINT_ALL, "compressed lightmaps: %s\n", enablestrings[(r_ext_compressed_lightmaps->integer != 0 && glConfig.textureCompression != TC_NONE)] );
	ri.Printf( PRINT_ALL, "texture compression method: %s\n", tc_table[glConfig.textureCompression] );
	ri.Printf( PRINT_ALL, "anisotropic filtering: %s\n", enablestrings[(r_ext_texture_filter_anisotropic->integer != 0) && glConfig.textureFilterAnisotropicAvailable] );

#ifdef JEDIACADEMY_GLOW
	Com_Printf ("Dynamic Glow: %s\n", enablestrings[r_DynamicGlow->integer != 0] );
	if (g_bTextureRectangleHack) Com_Printf ("Dynamic Glow ATI BAD DRIVER HACK %s\n", enablestrings[g_bTextureRectangleHack] );
#endif

	if ( glConfig.smpActive ) {
		ri.Printf( PRINT_ALL, "Using dual processor acceleration\n" );
	}
	if ( r_finish->integer ) {
		ri.Printf( PRINT_ALL, "Forcing glFinish\n" );
	}
	if ( r_displayRefresh ->integer ) {
		ri.Printf( PRINT_ALL, "Display refresh set to %d\n", r_displayRefresh->integer );
	}
	if (tr.world)
	{
		ri.Printf( PRINT_ALL, "Light Grid size set to (%.2f %.2f %.2f)\n", tr.world->lightGridSize[0], tr.world->lightGridSize[1], tr.world->lightGridSize[2] );
	}
}

#ifdef JEDIACADEMY_GLOW
void R_AtiHackToggle_f(void)
{
	g_bTextureRectangleHack = !g_bTextureRectangleHack;
}
#endif

#endif // !DEDICATED
/*
===============
R_Register
===============
*/
void R_Register( void ) 
{
	//
	// latched and archived variables
	//
	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compress_textures", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_compressed_lightmaps = ri.Cvar_Get( "r_ext_compress_lightmaps", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_preferred_tc_method = ri.Cvar_Get( "r_ext_preferred_tc_method", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_gamma_control = ri.Cvar_Get( "r_ext_gamma_control", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_fboFishEye = ri.Cvar_Get("r_fboFishEye", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "1", CVAR_ARCHIVE );

#ifdef JEDIACADEMY_GLOW
	r_DynamicGlow = ri.Cvar_Get( "r_DynamicGlow", "0", CVAR_ARCHIVE );
	r_DynamicGlowPasses = ri.Cvar_Get( "r_DynamicGlowPasses", "5", CVAR_ARCHIVE );
	r_DynamicGlowDelta = ri.Cvar_Get( "r_DynamicGlowDelta", "0.8f", CVAR_ARCHIVE );
	r_DynamicGlowIntensity = ri.Cvar_Get( "r_DynamicGlowIntensity", "1.13f", CVAR_ARCHIVE );
	r_DynamicGlowSoft = ri.Cvar_Get( "r_DynamicGlowSoft", "1", CVAR_ARCHIVE );
	r_DynamicGlowWidth = ri.Cvar_Get( "r_DynamicGlowWidth", "320", CVAR_ARCHIVE|CVAR_LATCH );
	r_DynamicGlowHeight = ri.Cvar_Get( "r_DynamicGlowHeight", "240", CVAR_ARCHIVE|CVAR_LATCH );
#endif

	r_picmip = ri.Cvar_Get ("r_picmip", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH );
	AssertCvarRange( r_picmip, 0, 16, qtrue );
	r_detailTextures = ri.Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_texturebitslm = ri.Cvar_Get( "r_texturebitslm", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_colorbits = ri.Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stereo = ri.Cvar_Get( "r_stereo", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_stencilbits = ri.Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE | CVAR_LATCH );
	r_depthbits = ri.Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_overBrightBits = ri.Cvar_Get ("r_overBrightBits", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_fboOverbright = ri.Cvar_Get ("r_fboOverbright", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_mode = ri.Cvar_Get( "r_mode", "3", CVAR_ARCHIVE | CVAR_LATCH );
	r_fullscreen = ri.Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_noborder = ri.Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE|CVAR_LATCH );
	r_customwidth = ri.Cvar_Get( "r_customwidth", "1600", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = ri.Cvar_Get( "r_customheight", "1024", CVAR_ARCHIVE | CVAR_LATCH );
	r_customaspect = ri.Cvar_Get( "r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH );
	r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_uiFullScreen = ri.Cvar_Get( "r_uifullscreen", "0", 0);
	r_subdivisions = ri.Cvar_Get ("r_subdivisions", "4", CVAR_ARCHIVE | CVAR_LATCH);

	r_hdr = ri.Cvar_Get("r_hdr", "1", CVAR_ARCHIVE | CVAR_LATCH);

	r_multiSample = ri.Cvar_Get( "r_multiSample", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_multiSampleNvidia = ri.Cvar_Get( "r_multiSampleNvidia", "0", CVAR_ARCHIVE | CVAR_LATCH );

#ifdef MACOS_X
        // Default to using SMP on Mac OS X if we have multiple processors
//	r_smp = ri.Cvar_Get( "r_smp", Sys_ProcessorCount() > 1 ? "1" : "0", CVAR_ARCHIVE | CVAR_LATCH);
#else        
//	r_smp = ri.Cvar_Get( "r_smp", "0", CVAR_ARCHIVE | CVAR_LATCH);
#endif
	r_smp = ri.Cvar_Get( "r_smp", "0", CVAR_ROM);
	r_ignoreFastPath = ri.Cvar_Get( "r_ignoreFastPath", "1", CVAR_ARCHIVE | CVAR_LATCH );

	//
	// temporary latched variables that can only change over a restart
	//
	r_displayRefresh = ri.Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH );
	AssertCvarRange( r_displayRefresh, 0, 200, qtrue );
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", CVAR_CHEAT );
	r_intensity = ri.Cvar_Get ("r_intensity", "1", CVAR_LATCH );
	r_singleShader = ri.Cvar_Get ("r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE );
	r_autolodscalevalue = ri.Cvar_Get( "r_autolodscalevalue", "0", CVAR_ROM );

	// Gamma handling/srgb stuff
	r_gammaSrgbLightmaps = ri.Cvar_Get("r_gammaSrgbLightmaps", "0", CVAR_ARCHIVE);
	r_gammaSrgbTextures = ri.Cvar_Get("r_gammaSrgbTextures", "1", CVAR_ARCHIVE);
	r_gammaLegacy = ri.Cvar_Get("r_gammaLegacy", "0", CVAR_ARCHIVE);
	r_gammaLegacyPrecision = ri.Cvar_Get("r_gammaLegacyPrecision", "2", CVAR_ARCHIVE);
	r_gammaSrgbLightvalues = ri.Cvar_Get("r_gammaSrgbLightvalues", "1", CVAR_ARCHIVE);
	r_HUDBrightness = ri.Cvar_Get("r_HUDBrightness", "0.5", CVAR_ARCHIVE);

	r_hideMissingModels = ri.Cvar_Get("r_hideMissingModels", "1", CVAR_ARCHIVE);

	r_flares = ri.Cvar_Get ("r_flares", "0", CVAR_ARCHIVE );
	r_znear = ri.Cvar_Get( "r_znear", "1", CVAR_CHEAT );
	r_zinvert = ri.Cvar_Get( "r_zinvert", "0", CVAR_ARCHIVE | CVAR_LATCH );
	AssertCvarRange( r_znear, 0.001f, 200, qfalse );
	r_zproj = ri.Cvar_Get( "r_zproj", "107", CVAR_ARCHIVE );
	r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "0", 0 );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
	r_inGameVideo = ri.Cvar_Get( "r_inGameVideo", "1", CVAR_ARCHIVE );
	r_drawSun = ri.Cvar_Get( "r_drawSun", "0", CVAR_ARCHIVE );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_ARCHIVE );
	r_skyboxRotate = ri.Cvar_Get( "r_skyboxRotate", "0", CVAR_ARCHIVE );
	r_finish = ri.Cvar_Get ("r_finish", "0", CVAR_ARCHIVE);
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	r_swapInterval = ri.Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE );
#ifdef __MACOS__
	r_gamma = ri.Cvar_Get( "r_gamma", "1.2", CVAR_ARCHIVE );
#else
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE );
#endif
	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE );

	r_primitives = ri.Cvar_Get( "r_primitives", "0", CVAR_ARCHIVE );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.6", CVAR_CHEAT );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_CHEAT );

	r_debugLight = ri.Cvar_Get( "r_debuglight", "0", CVAR_TEMP );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );

	r_dlightStyle = ri.Cvar_Get( "r_dlightStyle", "0", CVAR_ARCHIVE );
	r_surfaceSprites = ri.Cvar_Get ("r_surfaceSprites", "1", CVAR_TEMP);
	r_surfaceWeather = ri.Cvar_Get ("r_surfaceWeather", "0", CVAR_TEMP);

	r_windSpeed = ri.Cvar_Get ("r_windSpeed", "0", 0);
	r_windAngle = ri.Cvar_Get ("r_windAngle", "0", 0);
	r_windGust = ri.Cvar_Get ("r_windGust", "0", 0);
	r_windDampFactor = ri.Cvar_Get ("r_windDampFactor", "0.1", 0);
	r_windPointForce = ri.Cvar_Get ("r_windPointForce", "0", 0);
	r_windPointX = ri.Cvar_Get ("r_windPointX", "0", 0);
	r_windPointY = ri.Cvar_Get ("r_windPointY", "0", 0);

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", CVAR_CHEAT );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", CVAR_CHEAT );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );

	r_flareSize = ri.Cvar_Get ("r_flareSize", "40", CVAR_CHEAT);
	r_flareFade = ri.Cvar_Get ("r_flareFade", "7", CVAR_CHEAT);

	r_showSmp = ri.Cvar_Get ("r_showSmp", "0", CVAR_CHEAT);
	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);

	r_newDLights = ri.Cvar_Get ("r_newDLights", "0", 0);

	r_measureOverdraw = ri.Cvar_Get( "r_measureOverdraw", "0", CVAR_CHEAT );
	r_lodscale = ri.Cvar_Get( "r_lodscale", "5", 0 );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", CVAR_CHEAT);
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", CVAR_CHEAT );
	r_ignore = ri.Cvar_Get( "r_ignore", "1", CVAR_CHEAT );
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	r_showcluster = ri.Cvar_Get ("r_showcluster", "0", CVAR_CHEAT);
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	r_verbose = ri.Cvar_Get( "r_verbose", "0", CVAR_CHEAT );
	r_logFile = ri.Cvar_Get( "r_logFile", "0", CVAR_CHEAT );
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	r_showtris = ri.Cvar_Get ("r_showtris", "0", CVAR_CHEAT);
	r_showsky = ri.Cvar_Get ("r_showsky", "0", CVAR_CHEAT);
	r_shownormals = ri.Cvar_Get ("r_shownormals", "0", CVAR_CHEAT);
	r_clear = ri.Cvar_Get ("r_clear", "0", CVAR_CHEAT);
	r_offsetFactor = ri.Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT );
	r_drawBuffer = ri.Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	r_noportals = ri.Cvar_Get ("r_noportals", "0", CVAR_CHEAT);
	r_shadows = ri.Cvar_Get( "cg_shadows", "1", 0 );

	r_maxpolys = ri.Cvar_Get( "r_maxpolys", va("%d", MAX_POLYS), 0);
	r_maxpolyverts = ri.Cvar_Get( "r_maxpolyverts", va("%d", MAX_POLYVERTS), 0);

	r_convertModelBones = ri.Cvar_Get("r_convertModelBones", "1", CVAR_ARCHIVE);
	r_loadSkinsJKA = ri.Cvar_Get("r_loadSkinsJKA", "1", CVAR_ARCHIVE);
/*
Ghoul2 Insert Start
*/
	r_noServerGhoul2 = ri.Cvar_Get( "r_noserverghoul2", "0", CVAR_CHEAT);

	r_Ghoul2AnimSmooth = ri.Cvar_Get( "r_ghoul2animsmooth", ".3", 0 );
	r_Ghoul2UnSqashAfterSmooth = ri.Cvar_Get( "r_ghoul2unsqashaftersmooth", "1", 0 );
/*
Ghoul2 Insert End
*/
extern qboolean Sys_LowPhysicalMemory();
	r_modelpoolmegs = Cvar_Get("r_modelpoolmegs", "10", CVAR_ARCHIVE);
	if (Sys_LowPhysicalMemory() ) {
		Cvar_Set("r_modelpoolmegs", "0");
	}

	r_drawAllAreas = ri.Cvar_Get("r_drawAllAreas", "0", CVAR_TEMP | CVAR_CHEAT);

	// make sure all the commands added here are also
	// removed in R_Shutdown
#ifndef DEDICATED
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShotTGA_f );
	ri.Cmd_AddCommand( "screenshotTGA", R_ScreenShotTGA_f );
	ri.Cmd_AddCommand( "screenshotJPEG", R_ScreenShotJPEG_f );
	ri.Cmd_AddCommand( "screenshotPNG", R_ScreenShotPNG_f );
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
	ri.Cmd_AddCommand( "minimize", GLimp_Minimize );
#ifdef JEDIACADEMY_GLOW
	ri.Cmd_AddCommand( "r_atihack", R_AtiHackToggle_f );
#endif
	ri.Cmd_AddCommand("r_we", R_WorldEffect_f);
	ri.Cmd_AddCommand( "imagecacheinfo", RE_RegisterImages_Info_f);
#endif
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "modelist", R_ModeList_f );
	ri.Cmd_AddCommand( "modelcacheinfo", RE_RegisterModels_Info_f);

	r_fontSharpness = ri.Cvar_Get("r_fontSharpness", "3", CVAR_ARCHIVE);
	r_font3DBrightness = ri.Cvar_Get("r_font3DBrightness", "2", CVAR_ARCHIVE);
}

#ifdef G2_COLLISION_ENABLED
#define G2_VERT_SPACE_SERVER_SIZE 256
#endif

void APIENTRY
MessageCallback(GLenum source,
	GLenum type,
	GLuint id,
	GLenum severity,
	GLsizei length,
	const GLchar* message,
	const void* userParam)
{
	ri.Printf(PRINT_WARNING, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
		(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
		type, severity, message);
}





#ifdef JEDIACADEMY_GLOW
//GLuint pboIds[2];
vector<GLuint> pboIds(2);
vector<int> pboRollingShutterProgresses(1);
vector<float> pboRollingShutterDrifts(1);
int rollingShutterBufferCount = 1;
int progressOvershoot = 0;
float drift = 0;
#endif
/*
===============
R_Init
===============
*/
void R_Init( void ) {	
	int i;
	byte *ptr;

	ri.Printf( PRINT_ALL, "----- R_Init -----\n" );

	// clear all our internal state
	Com_Memset( &tr, 0, sizeof( tr ) );
	Com_Memset( &backEnd, 0, sizeof( backEnd ) );
#ifndef DEDICATED
	Com_Memset( &tess, 0, sizeof( tess ) );
#endif



	// During init, enable debug output
	//qglEnable(GL_DEBUG_OUTPUT); //Nope won't work. Needs opengl 4.3.
	//qglDebugMessageCallback(MessageCallback, 0);


//	Swap_Init();

#ifndef DEDICATED
#ifndef FINAL_BUILD
	if ( (int)tess.xyz & 15 ) {
		Com_Printf( "WARNING: tess.xyz not 16 byte aligned (%x)\n",(int)tess.xyz & 15 );
	}
#endif
	//Com_Memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );
	for (i = 0; i < SHADER_MAX_VERTEXES; i++) {
		tess.constantColor255[i][0] = tess.constantColor255[i][1] = tess.constantColor255[i][2] = tess.constantColor255[i][3] = 255;
		tess.constantColor255Scaled[i][0] = tess.constantColor255Scaled[i][1] = tess.constantColor255Scaled[i][2] = tess.constantColor255Scaled[i][3] = 1.0f;
	}
#endif
	//
	// init function tables
	//
	for ( i = 0; i < FUNCTABLE_SIZE; i++ )
	{
		tr.sinTable[i]		= sin( DEG2RAD( i * 360.0f / ( ( float ) ( FUNCTABLE_SIZE - 1 ) ) ) );
		tr.squareTable[i]	= ( i < FUNCTABLE_SIZE/2 ) ? 1.0f : -1.0f;
		tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

		if ( i < FUNCTABLE_SIZE / 2 )
		{
			if ( i < FUNCTABLE_SIZE / 4 )
			{
				tr.triangleTable[i] = ( float ) i / ( FUNCTABLE_SIZE / 4 );
			}
			else
			{
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i-FUNCTABLE_SIZE / 4];
			}
		}
		else
		{
			tr.triangleTable[i] = -tr.triangleTable[i-FUNCTABLE_SIZE/2];
		}
	}
#ifndef DEDICATED
	R_InitFogTable();

	R_NoiseInit();
#endif
	R_Register();

	R_MME_Init();
	R_MME_InitStereo();

	max_polys = r_maxpolys->integer;
	if (max_polys < MAX_POLYS)
		max_polys = MAX_POLYS;

	max_polyverts = r_maxpolyverts->integer;
	if (max_polyverts < MAX_POLYVERTS)
		max_polyverts = MAX_POLYVERTS;

	ptr = (unsigned char *)ri.Hunk_Alloc( sizeof( *backEndData[0] ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);
	backEndData[0] = (backEndData_t *) ptr;
	backEndData[0]->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData[0] ));
	backEndData[0]->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData[0] ) + sizeof(srfPoly_t) * max_polys);
	if ( r_smp->integer ) {
		ptr = (unsigned char *)ri.Hunk_Alloc( sizeof( *backEndData[1] ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);
		backEndData[1] = (backEndData_t *) ptr;
		backEndData[1]->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData[1] ));
		backEndData[1]->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData[1] ) + sizeof(srfPoly_t) * max_polys);
	} else {
		backEndData[1] = NULL;
	}
#ifndef DEDICATED
	R_ToggleSmpFrame();

	for(i = 0; i < MAX_LIGHT_STYLES; i++)
	{
		RE_SetLightStyle(i, -1);
	}
	InitOpenGL();

	R_InitImages();
	R_InitShaders();
	R_InitSkins();
	R_InitFonts();
	R_InitFreeType();
#endif
	R_ModelInit();
#ifndef DEDICATED

#ifdef G2_COLLISION_ENABLED
	if (!G2VertSpaceServer)
	{
		G2VertSpaceServer = new CMiniHeap(G2_VERT_SPACE_SERVER_SIZE * 1024);
	}
#endif

	int	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Printf (PRINT_ALL, "glGetError() = 0x%x\n", err);
#endif
#ifdef JEDIACADEMY_GLOW
	{

		// create 2 pixel buffer objects, you need to delete them when program exits.
		// glBufferDataARB with NULL pointer reserves only memory space.
#ifdef CAPTURE_FLOAT
		int dataSize = glConfig.vidWidth * glConfig.vidHeight * 3*4;
#else
		int dataSize = glConfig.vidWidth * glConfig.vidHeight * 3;
#endif


		if (2 > pboIds.size()) { // 2 PBOs, one for capturing, 2 for maybe double buffering, we'll see..
			pboIds.resize(2);
		}

		qglGenBuffersARB(pboIds.size(), pboIds.data());

		for (int i = 0; i < pboIds.size(); i++) {
			qglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[i]);
			qglBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, dataSize, 0, GL_DYNAMIC_READ_ARB);
		}

		qglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
#endif
	ri.Printf( PRINT_ALL, "----- finished R_Init -----\n" );
}

/*
===============
RE_Shutdown
===============
*/
void RE_Shutdown( qboolean destroyWindow ) {	

	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", destroyWindow );

	ri.Cmd_RemoveCommand ("modellist");
	ri.Cmd_RemoveCommand ("screenshot");
	ri.Cmd_RemoveCommand ("screenshotTGA");
	ri.Cmd_RemoveCommand ("screenshotJPEG");
	ri.Cmd_RemoveCommand ("screenshotPNG");
	ri.Cmd_RemoveCommand ("imagelist");
	ri.Cmd_RemoveCommand ("shaderlist");
	ri.Cmd_RemoveCommand ("skinlist");
	ri.Cmd_RemoveCommand ("gfxinfo");
	ri.Cmd_RemoveCommand ("minimize");
	ri.Cmd_RemoveCommand ("modelist");
	ri.Cmd_RemoveCommand ("shaderstate");
	ri.Cmd_RemoveCommand ("r_we");
	ri.Cmd_RemoveCommand ("modelcacheinfo");
	ri.Cmd_RemoveCommand ("imagecacheinfo");

#ifdef JEDIACADEMY_GLOW
	if ( r_DynamicGlow && r_DynamicGlow->integer )
	{
		// Release the Glow Vertex Shader.
		if ( tr.glowVShader )
		{
			qglDeleteProgramsARB( 1, &tr.glowVShader );
		}

		// Release Pixel Shader.
		if ( tr.glowPShader )
		{
			if ( qglCombinerParameteriNV  )
			{
				// Release the Glow Regcom call list.
				qglDeleteLists( tr.glowPShader, 1 );
			}
			else if ( qglGenProgramsARB )
			{
				// Release the Glow Fragment Shader.
				qglDeleteProgramsARB( 1, &tr.glowPShader );
			}
		}

		// Release the scene glow texture.
		qglDeleteTextures( 1, &tr.screenGlow );

		// Release the scene texture.
		qglDeleteTextures( 1, &tr.sceneImage );

		// Release the blur texture.
		qglDeleteTextures( 1, &tr.blurImage );
	}
#endif

#ifndef DEDICATED
	R_ShutdownFonts();
	if ( tr.registered ) {
		R_SyncRenderThread();
		R_ShutdownCommandBuffers();
		if (destroyWindow)
		{
			R_DeleteTextures();		// only do this for vid_restart now, not during things like map load
		}
	}

	R_MME_Shutdown();
	R_MME_ShutdownStereo();
	
	R_DoneFreeType();

	// shut down platform specific OpenGL stuff
	if ( destroyWindow ) {
#ifdef CAPTURE_FLOAT
		R_FrameBuffer_Shutdown();
#endif
		GLimp_Shutdown();

		Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		Com_Memset( &glState, 0, sizeof( glState ) );
	}
#endif //!DEDICATED

	tr.registered = qfalse;

#ifdef G2_COLLISION_ENABLED
	if (G2VertSpaceServer)
	{
		delete G2VertSpaceServer;
		G2VertSpaceServer = 0;
	}
#endif
}

#ifndef DEDICATED

/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration( void ) {
	R_SyncRenderThread();
	if (!Sys_LowPhysicalMemory()) {
		RB_ShowImages();
	}
}

void RE_GetLightStyle(int style, color4f_t color)
{
	if (style >= MAX_LIGHT_STYLES)
	{
	    ri.Error( ERR_FATAL, "RE_GetLightStyle: %d is out of range", (int)style );
		return;
	}

	Com_Memcpy(color, styleColors[style], sizeof(color4f_t));
	//*(int *)color = *(int *)styleColors[style];
}

void RE_SetLightStyle(int style, int color)
{
	if (style >= MAX_LIGHT_STYLES)
	{
	    ri.Error( ERR_FATAL, "RE_SetLightStyle: %d is out of range", (int)style );
		return;
	}

	Vector4Copy(*(color4ub_t*)&color, styleColors[style]);
	/*if (*(int*)styleColors[style] != color)
	{
		*(int *)styleColors[style] = color;
	}*/
}

#endif //!DEDICATED

static void R_DemoRandomSeed(int time, float timeFraction) {
	srand(time + timeFraction);
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
refexport_t *GetRefAPI ( int apiVersion, refimport_t *rimp ) {
	static refexport_t	re;

	ri = *rimp;

	Com_Memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", 
			REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;
#ifndef DEDICATED
	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.RegisterShaderNoMipHUD = RE_RegisterShaderNoMipHUD;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.DrawRotatePic = RE_RotatePic;
	re.DrawRotatePic2 = RE_RotatePic2;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddMiniRefEntityToScene = RE_AddMiniRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.Font_StrLenPixels = RE_Font_StrLenPixels;
	re.Font_StrLenChars = RE_Font_StrLenChars;
	re.Font_HeightPixels = RE_Font_HeightPixels;
	re.Font_DrawString = RE_Font_DrawString;
	re.Font_DrawString_3D = RE_Font_DrawString_3D;
	re.Language_IsAsian = Language_IsAsian;
	re.Language_UsesSpaces = Language_UsesSpaces;
	re.AnyLanguage_ReadCharFromString = AnyLanguage_ReadCharFromString;

	re.RemapShader = R_RemapShader;
	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	re.GetLightStyle = RE_GetLightStyle;
	re.SetLightStyle = RE_SetLightStyle;

	re.GetBModelVerts = RE_GetBModelVerts;
#endif //!DEDICATED

	//mme
	re.Capture = R_MME_Capture;
	re.CaptureStereo = R_MME_CaptureStereo;
	re.BlurInfo = R_MME_BlurInfo;
	
	re.Time = R_MME_Time;
	re.TimeFraction = R_MME_TimeFraction;
	
	re.MMERegisterFont = R_MME_RegisterFont;
	re.MMEFakeAdvanceFrames = R_MME_FakeAdvanceFrames;
	re.MMEGetRollingShutterInfo = R_MME_GetRollingShutterInfo;
	re.FontRatioFix = RE_FontRatioFix;

	re.DemoRandomSeed = R_DemoRandomSeed;

	re.ParseWaveformAlone = ParseWaveformAlone;
	re.EvalWaveForm = EvalWaveForm;

	return &re;
}

