/*
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_LogComment
** GLimp_Shutdown
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/
#include <assert.h>
#include "../renderer/tr_local.h"
#include "../renderer/qgl.h"
#include "../qcommon/qcommon.h"
#include "resource.h"
#include "glw_win.h"
#include "win_local.h"
#include "../qcommon/strip.h"
extern void WG_CheckHardwareGamma( void );
extern void WG_RestoreGamma( void );

static qboolean GLW_CreateWindow( int width, int height, int colorbits, qboolean cdsFullscreen );




typedef enum {
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

#define TRY_PFD_SUCCESS		0
#define TRY_PFD_FAIL_SOFT	1
#define TRY_PFD_FAIL_HARD	2

#define	WINDOW_CLASS_NAME	"Jedi Knight 2: Jedi Outcast MP"

#ifndef MULTISAMPLE_FILTER_HINT_NV		
#define	MULTISAMPLE_FILTER_HINT_NV                0x8534
#endif

static void		GLW_InitExtensions( void );
static rserr_t	GLW_SetMode( int mode, 
							 int colorbits, 
							 qboolean cdsFullscreen );

static qboolean s_classRegistered = qfalse;

//
// function declaration
//
void	 QGL_EnableLogging( qboolean enable );
qboolean QGL_Init( const char *dllname );
void     QGL_Shutdown( void );

//
// variable declarations
//
glwstate_t glw_state;

cvar_t	*r_allowSoftwareGL;		// don't abort out if the pixelformat claims software

#ifdef JEDIACADEMY_GLOW
// Whether the current hardware supports dynamic glows/flares.
extern bool g_bDynamicGlowSupported;
#endif

static void GLW_ARB_InitExtensions( void ) {
	const char *wglExtensions_;

	if (!glw_state.hDC || !glw_state.hGLRC)
		return; 

	if ( qwglGetExtensionsStringARB )
		return;

	qwglGetExtensionsStringARB = (const char *( WINAPI *) (HDC))qwglGetProcAddress( "wglGetExtensionsStringARB" );
	
	if (qwglGetExtensionsStringARB) {
		wglExtensions_ = qwglGetExtensionsStringARB( glw_state.hDC );
	} else {
		wglExtensions_ = "";
	}

	if (strstr( wglExtensions_, "WGL_ARB_pixel_format")) {
		ri.Printf( PRINT_ALL, "...Found WGL_ARB_pixel_format extension\n");
		qwglGetPixelFormatAttribivARB	= (BOOL (WINAPI *) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues))qwglGetProcAddress( "wglGetPixelFormatAttribivARB" );
		qwglGetPixelFormatAttribfvARB	= (BOOL (WINAPI *) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues))qwglGetProcAddress( "wglGetPixelFormatAttribfvARB" );
		qwglChoosePixelFormatARB		= (BOOL (WINAPI *) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats))qwglGetProcAddress( "wglChoosePixelFormatARB" );
	}
	if (strstr( wglExtensions_, "WGL_ARB_pbuffer")) {
		ri.Printf( PRINT_ALL, "...Found WGL_ARB_pbuffer extension\n");
		qwglCreatePbufferARB			= (HPBUFFERARB (WINAPI *) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int *piAttribList))qwglGetProcAddress( "wglCreatePbufferARB" );
		qwglGetPbufferDCARB				= (HDC (WINAPI *) (HPBUFFERARB hPbuffer))qwglGetProcAddress( "wglGetPbufferDCARB" );
		qwglReleasePbufferDCARB			= (int (WINAPI *) (HPBUFFERARB hPbuffer, HDC hDC))qwglGetProcAddress( "wglReleasePbufferDCARB" );
		qwglDestroyPbufferARB			= (BOOL (WINAPI *) (HPBUFFERARB hPbuffer))qwglGetProcAddress( "wglDestroyPbufferARB" );
		qwglQueryPbufferARB				= (BOOL (WINAPI *) (HPBUFFERARB hPbuffer, int iAttribute, int *piValue))qwglGetProcAddress( "wglQueryPbufferARB" );
	}
}

static int GLW_ChoosePixelFormatARB( int colorBits, int depthBits, int stencilBits, int samples, int pbuffer, int stereo ) {
	int *iAttr;
	int iAttribs[64];
	int pixelformat;
	unsigned int matching;

	if (!qwglChoosePixelFormatARB)
		return 0;
	if (pbuffer && !qwglCreatePbufferARB)
		return 0;

	iAttr = iAttribs;
	if (pbuffer) {
			*iAttr++ = WGL_PIXEL_TYPE_ARB;
			*iAttr++ = WGL_TYPE_RGBA_ARB;
			*iAttr++ = WGL_DRAW_TO_PBUFFER_ARB;
			*iAttr++ = TRUE;
#if 0
			*iAttr++ = WGL_BIND_TO_TEXTURE_RGB_ARB
			*iAttr++ = TRUE;
#endif
			*iAttr++ = WGL_DOUBLE_BUFFER_ARB;
			*iAttr++ = 0;
	} else {
		*iAttr++ = WGL_DRAW_TO_WINDOW_ARB;
		*iAttr++ = GL_TRUE;
		*iAttr++ = WGL_DOUBLE_BUFFER_ARB;
		/* Don't bother with double buffering or multisampling with an active pbuffer */
		if (glw_state.pbuf.hDC) {
			samples = 0;
			*iAttr++ = 0;
		} else {
			*iAttr++ = 1;
		}
	}
	*iAttr++ = WGL_ACCELERATION_ARB;
	*iAttr++ = WGL_FULL_ACCELERATION_ARB;
	*iAttr++ = WGL_SUPPORT_OPENGL_ARB;
	*iAttr++ = GL_TRUE;

	*iAttr++ = WGL_COLOR_BITS_ARB;
	*iAttr++ = colorBits;
	*iAttr++ = WGL_DEPTH_BITS_ARB;
	*iAttr++ = depthBits;
	*iAttr++ = WGL_STENCIL_BITS_ARB;
	*iAttr++ = stencilBits;
	*iAttr++ = WGL_AUX_BUFFERS_ARB;
	*iAttr++ = 0;

	if ( stereo ) {
		*iAttr++ = WGL_STEREO_ARB;
		*iAttr++ = GL_TRUE;
	}
	if ( samples ) {
		*iAttr++ = WGL_SAMPLE_BUFFERS_ARB;
		*iAttr++ = 1;
		*iAttr++ = WGL_SAMPLES_ARB;
		*iAttr++ = samples;
	}
	*iAttr = 0;
	qwglChoosePixelFormatARB( glw_state.hDC, iAttribs, 0, 1, &pixelformat, &matching);
	if (!matching)
		return 0;
	else 
		return pixelformat;
}

/*
** GLW_StartDriverAndSetMode
*/
static qboolean GLW_StartDriverAndSetMode( int mode, 
										   int colorbits,
										   qboolean cdsFullscreen )
{
	if (!qwglGetExtensionsStringARB && ( r_multiSample->integer || ( mme_renderWidth->integer && mme_renderHeight->integer )) ) {
		if (GLW_CreateWindow( 320, 200, 0, qfalse )) {
			GLW_ARB_InitExtensions( ) ;
		}
	}
	
	/* If offscreen rendering has been enabled, skip the normal opengl window setup */
	if (mme_renderWidth->integer > 0 && mme_renderHeight->integer > 0) {
		int pbufferList[] = {	
			WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGB_ARB,
			WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB,
			WGL_MIPMAP_TEXTURE_ARB,	0,
			0 };


		GLint	pixelFormat;
		int		width, height, samples;
		int attrib = WGL_SAMPLES_ARB;

chooseAgain:
		pixelFormat = GLW_ChoosePixelFormatARB( 32, 24, 8, r_multiSample->integer, 1, 0);

		if (!pixelFormat) {
			if ( r_multiSample->integer ) {
				ri.Printf( PRINT_ALL, "Can't create pbuffer pixelformat, trying without multisampling\n" );
				ri.Cvar_Set( "r_multiSample", "0" );
				goto chooseAgain;
			} else {
				ri.Printf( PRINT_ALL, "Can't create pbuffer pixelformat, skipping pbuffer\n" );
				goto skip_pbuffer;
			}
		}

		width = mme_renderWidth->integer;
		height = mme_renderHeight->integer;

		glw_state.pbuf.buffer = qwglCreatePbufferARB( glw_state.hDC, pixelFormat, width, height, NULL /* pbufferList */ );
		if (!glw_state.pbuf.buffer) {
			int error = GetLastError();
			goto skip_pbuffer;
		}

		qwglGetPixelFormatAttribivARB( glw_state.hDC, pixelFormat, 0, 1, &attrib, &samples );

		glw_state.pbuf.hDC = qwglGetPbufferDCARB( glw_state.pbuf.buffer );
		if(!glw_state.pbuf.hDC)
			goto skip_pbuffer;

		glw_state.pbuf.hGLRC = qwglCreateContext( glw_state.pbuf.hDC );
		if(!glw_state.pbuf.hGLRC )
			goto skip_pbuffer;
			
		qwglShareLists( glw_state.hGLRC, glw_state.pbuf.hGLRC );
		//Get the actual pBuffer dimensions
		qwglQueryPbufferARB( glw_state.pbuf.buffer, WGL_PBUFFER_WIDTH_ARB, &width);
		qwglQueryPbufferARB( glw_state.pbuf.buffer, WGL_PBUFFER_HEIGHT_ARB, &height);

		ri.Printf(PRINT_ALL, "Pbuffer Created: (%d x %d) samples %d format %d\n", width, height, samples, pixelFormat );
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		qwglMakeCurrent( glw_state.pbuf.hDC, glw_state.pbuf.hGLRC );
//		glConfig.windowAspect = 1;
		/* Always force on the console with pbuffer drawing */
//		Sys_ShowConsole( 1, qfalse );
		return qtrue;
	}
skip_pbuffer:

	rserr_t err;

	err = GLW_SetMode( mode, colorbits, cdsFullscreen );

	switch ( err )
	{
	case RSERR_INVALID_FULLSCREEN:
		ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
		return qfalse;
	case RSERR_INVALID_MODE:
		ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
		return qfalse;
	default:
		break;
	}
	return qtrue;
}

/*
** ChoosePFD
**
** Helper function that replaces ChoosePixelFormat.
*/
#define MAX_PFDS 256

static int GLW_ChoosePFD( HDC hDC, PIXELFORMATDESCRIPTOR *pPFD )
{
	PIXELFORMATDESCRIPTOR pfds[MAX_PFDS+1];
	int maxPFD = 0;
	int i;
	int bestMatch = 0;

	ri.Printf( PRINT_ALL, "...GLW_ChoosePFD( %d, %d, %d )\n", ( int ) pPFD->cColorBits, ( int ) pPFD->cDepthBits, ( int ) pPFD->cStencilBits );

	// count number of PFDs
	maxPFD = DescribePixelFormat( hDC, 1, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[0] );

	if ( maxPFD > MAX_PFDS )
	{
		ri.Printf( PRINT_WARNING, "...numPFDs > MAX_PFDS (%d > %d)\n", maxPFD, MAX_PFDS );
		maxPFD = MAX_PFDS;
	}

	ri.Printf( PRINT_ALL, "...%d PFDs found\n", maxPFD - 1 );

	// grab information
	for ( i = 1; i <= maxPFD; i++ )
	{
		DescribePixelFormat( hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[i] );
	}

	// look for a best match
	for ( i = 1; i <= maxPFD; i++ )
	{
		//
		// make sure this has hardware acceleration
		//
		if ( ( pfds[i].dwFlags & PFD_GENERIC_FORMAT ) != 0 ) 
		{
			if ( !r_allowSoftwareGL->integer )
			{
				if ( r_verbose->integer )
				{
					ri.Printf( PRINT_ALL, "...PFD %d rejected, software acceleration\n", i );
				}
				continue;
			}
		}

		// verify pixel type
		if ( pfds[i].iPixelType != PFD_TYPE_RGBA )
		{
			if ( r_verbose->integer )
			{
				ri.Printf( PRINT_ALL, "...PFD %d rejected, not RGBA\n", i );
			}
			continue;
		}

		// verify proper flags
		if ( ( ( pfds[i].dwFlags & pPFD->dwFlags ) & pPFD->dwFlags ) != pPFD->dwFlags ) 
		{
			if ( r_verbose->integer )
			{
				ri.Printf( PRINT_ALL, "...PFD %d rejected, improper flags (%x instead of %x)\n", i, pfds[i].dwFlags, pPFD->dwFlags );
			}
			continue;
		}

		// verify enough bits
		if ( pfds[i].cDepthBits < 15 )
		{
			continue;
		}
		if ( ( pfds[i].cStencilBits < 4 ) && ( pPFD->cStencilBits > 0 ) )
		{
			continue;
		}

		//
		// selection criteria (in order of priority):
		// 
		//  PFD_STEREO
		//  colorBits
		//  depthBits
		//  stencilBits
		//
		if ( bestMatch )
		{
			// check stereo
			if ( ( pfds[i].dwFlags & PFD_STEREO ) && ( !( pfds[bestMatch].dwFlags & PFD_STEREO ) ) && ( pPFD->dwFlags & PFD_STEREO ) )
			{
				bestMatch = i;
				continue;
			}
			
			if ( !( pfds[i].dwFlags & PFD_STEREO ) && ( pfds[bestMatch].dwFlags & PFD_STEREO ) && ( pPFD->dwFlags & PFD_STEREO ) )
			{
				bestMatch = i;
				continue;
			}

			// check color
			if ( pfds[bestMatch].cColorBits != pPFD->cColorBits )
			{
				// prefer perfect match
				if ( pfds[i].cColorBits == pPFD->cColorBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cColorBits > pfds[bestMatch].cColorBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check depth
			if ( pfds[bestMatch].cDepthBits != pPFD->cDepthBits )
			{
				// prefer perfect match
				if ( pfds[i].cDepthBits == pPFD->cDepthBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cDepthBits > pfds[bestMatch].cDepthBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check stencil
			if ( pfds[bestMatch].cStencilBits != pPFD->cStencilBits )
			{
				// prefer perfect match
				if ( pfds[i].cStencilBits == pPFD->cStencilBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( ( pfds[i].cStencilBits > pfds[bestMatch].cStencilBits ) && 
					 ( pPFD->cStencilBits > 0 ) )
				{
					bestMatch = i;
					continue;
				}
			}
		}
		else
		{
			bestMatch = i;
		}
	}
	
	if ( !bestMatch )
		return 0;

	if ( ( pfds[bestMatch].dwFlags & PFD_GENERIC_FORMAT ) != 0 )
	{
		if ( !r_allowSoftwareGL->integer )
		{
			ri.Printf( PRINT_ALL, "...no hardware acceleration found\n" );
			return 0;
		}
		else
		{
			ri.Printf( PRINT_ALL, "...using software emulation\n" );
		}
	}
	else if ( pfds[bestMatch].dwFlags & PFD_GENERIC_ACCELERATED )
	{
		ri.Printf( PRINT_ALL, "...MCD acceleration found\n" );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...hardware acceleration found\n" );
	}

	*pPFD = pfds[bestMatch];

	return bestMatch;
}

/*
** void GLW_CreatePFD
**
** Helper function zeros out then fills in a PFD
*/
static void GLW_CreatePFD( PIXELFORMATDESCRIPTOR *pPFD, int colorbits, int depthbits, int stencilbits, qboolean stereo )
{
    PIXELFORMATDESCRIPTOR src = 
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW |			// support window
		PFD_SUPPORT_OPENGL |			// support OpenGL
		PFD_DOUBLEBUFFER,				// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		24,								// 24-bit z-buffer	
		8,								// 8-bit stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
    };

	src.cColorBits = colorbits;
	src.cDepthBits = depthbits;
	src.cStencilBits = stencilbits;

	if ( stereo )
	{
		ri.Printf( PRINT_ALL, "...attempting to use stereo\n" );
		src.dwFlags |= PFD_STEREO;
		glConfig.stereoEnabled = qtrue;
	}
	else
	{
		glConfig.stereoEnabled = qfalse;
	}

	*pPFD = src;
}

/*
** GLW_MakeContext
*/
static int GLW_MakeContext( PIXELFORMATDESCRIPTOR *pPFD )
{
	int pixelformat;
	PIXELFORMATDESCRIPTOR tempPFD;


	pixelformat = GLW_ChoosePixelFormatARB( 
		pPFD->cColorBits, pPFD->cDepthBits, pPFD->cStencilBits, r_multiSample->integer, 0, (pPFD->dwFlags & PFD_STEREO));

	if ( pixelformat ) {
		int samples;
		int attrib = WGL_SAMPLES_ARB;

		qwglGetPixelFormatAttribivARB( glw_state.hDC, pixelformat, 0, 1, &attrib, &samples );
		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( tempPFD ), &tempPFD );

		if (!SetPixelFormat( glw_state.hDC, pixelformat, &tempPFD )) {
			ri.Printf( PRINT_ALL, "...SetPixelFormat %d with %d multisampling failed, falling back\n", pixelformat, samples );
			ri.Printf( PRINT_ALL, "Error code %d\n", GetLastError());
			glw_state.pixelFormatSet = qfalse;
		} else {
			ri.Printf( PRINT_ALL, "...PixelFormat %d with %d multisampling selected\n", pixelformat, samples );
			*pPFD = tempPFD;
			glw_state.pixelFormatSet = qtrue;
			if ( samples && r_multiSampleNvidia->integer && !strstr( glConfig.extensions_string, "GL_NV_multisample_filter_hint") ) {
				qglHint( MULTISAMPLE_FILTER_HINT_NV, GL_NICEST );
				ri.Printf( PRINT_ALL, "...Nvidia enhanced multisampling enabled\n", pixelformat, samples );
			} else {
				qglHint( MULTISAMPLE_FILTER_HINT_NV, GL_DONT_CARE );
			}
		}
	}


	//
	// don't putz around with pixelformat if it's already set (e.g. this is a soft
	// reset of the graphics system)
	//
	if ( !glw_state.pixelFormatSet )
	{
		//
		// choose, set, and describe our desired pixel format.  If we're
		// using a minidriver then we need to bypass the GDI functions,
		// otherwise use the GDI functions.
		//
		if ( ( pixelformat = GLW_ChoosePFD( glw_state.hDC, pPFD ) ) == 0 )
		{
			ri.Printf( PRINT_ALL, "...GLW_ChoosePFD failed\n");
			return TRY_PFD_FAIL_SOFT;
		}
		ri.Printf( PRINT_ALL, "...PIXELFORMAT %d selected\n", pixelformat );

		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( *pPFD ), pPFD );

		if ( SetPixelFormat( glw_state.hDC, pixelformat, pPFD ) == FALSE )
		{
			ri.Printf (PRINT_ALL, "...SetPixelFormat failed\n", glw_state.hDC );
			return TRY_PFD_FAIL_SOFT;
		}

		glw_state.pixelFormatSet = qtrue;
	}

	//
	// startup the OpenGL subsystem by creating a context and making it current
	//
	if ( !glw_state.hGLRC )
	{
		ri.Printf( PRINT_ALL, "...creating GL context: " );
		if ( ( glw_state.hGLRC = qwglCreateContext( glw_state.hDC ) ) == 0 )
		{
			ri.Printf (PRINT_ALL, "failed\n");
			if (r_multiSample->integer) {
				Com_Printf( "...Trying again without multisampling\n" );
				ri.Cvar_Set( "r_multiSample", "0" );
				return TRY_PFD_FAIL_SOFT;
			}
			return TRY_PFD_FAIL_HARD;
		}
		ri.Printf( PRINT_ALL, "succeeded\n" );

		ri.Printf( PRINT_ALL, "...making context current: " );
		if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
		{
			qwglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = NULL;
			ri.Printf (PRINT_ALL, "failed\n");
			return TRY_PFD_FAIL_HARD;
		}
		ri.Printf( PRINT_ALL, "succeeded\n" );
	}

	return TRY_PFD_SUCCESS;
}


/*
** GLW_InitDriver
**
** - get a DC if one doesn't exist
** - create an HGLRC if one doesn't exist
*/
static qboolean GLW_InitDriver( int colorbits )
{
	int		tpfd;
	int		depthbits, stencilbits;
    static PIXELFORMATDESCRIPTOR pfd;		// save between frames since 'tr' gets cleared

	ri.Printf( PRINT_ALL, "Initializing OpenGL driver\n" );

	//
	// get a DC for our window if we don't already have one allocated
	//
	if ( glw_state.hDC == NULL )
	{
		ri.Printf( PRINT_ALL, "...getting DC: " );

		if ( ( glw_state.hDC = GetDC( g_wv.hWnd ) ) == NULL )
		{
			ri.Printf( PRINT_ALL, "failed\n" );
			return qfalse;
		}
		ri.Printf( PRINT_ALL, "succeeded\n" );
	}

	if ( colorbits == 0 )
	{
		colorbits = glw_state.desktopBitsPixel;
	}

	//
	// implicitly assume Z-buffer depth == desktop color depth
	//
	if ( r_depthbits->integer == 0 ) {
		if ( colorbits > 16 ) {
			depthbits = 24;
		} else {
			depthbits = 16;
		}
	} else {
		depthbits = r_depthbits->integer;
	}

	//
	// do not allow stencil if Z-buffer depth likely won't contain it
	//
	stencilbits = r_stencilbits->integer;
	if ( depthbits < 24 )
	{
		stencilbits = 0;
	}

	//
	// make two attempts to set the PIXELFORMAT
	//

	//
	// first attempt: r_colorbits, depthbits, and r_stencilbits
	//
	if ( !glw_state.pixelFormatSet )
	{
		GLW_CreatePFD( &pfd, colorbits, depthbits, stencilbits, (qboolean)r_stereo->integer );
		if ( ( tpfd = GLW_MakeContext( &pfd ) ) != TRY_PFD_SUCCESS )
		{
			if ( tpfd == TRY_PFD_FAIL_HARD )
			{
				ri.Printf( PRINT_WARNING, "...failed hard\n" );
				return qfalse;
			}

			//
			// punt if we've already tried the desktop bit depth and no stencil bits
			//
			if ( ( r_colorbits->integer == glw_state.desktopBitsPixel ) &&
				 ( stencilbits == 0 ) )
			{
				ReleaseDC( g_wv.hWnd, glw_state.hDC );
				glw_state.hDC = NULL;

				ri.Printf( PRINT_ALL, "...failed to find an appropriate PIXELFORMAT\n" );

				return qfalse;
			}

			//
			// second attempt: desktop's color bits and no stencil
			//
			if ( colorbits > glw_state.desktopBitsPixel )
			{
				colorbits = glw_state.desktopBitsPixel;
			}
			GLW_CreatePFD( &pfd, colorbits, depthbits, 0, (qboolean)r_stereo->integer );
			if ( GLW_MakeContext( &pfd ) != TRY_PFD_SUCCESS )
			{
				if ( glw_state.hDC )
				{
					ReleaseDC( g_wv.hWnd, glw_state.hDC );
					glw_state.hDC = NULL;
				}

				ri.Printf( PRINT_ALL, "...failed to find an appropriate PIXELFORMAT\n" );

				return qfalse;
			}
		}

		/*
		** report if stereo is desired but unavailable
		*/
		if ( !( pfd.dwFlags & PFD_STEREO ) && ( r_stereo->integer != 0 ) ) 
		{
			ri.Printf( PRINT_ALL, "...failed to select stereo pixel format\n" );
			glConfig.stereoEnabled = qfalse;
		}
	}

	/*
	** store PFD specifics 
	*/
	glConfig.colorBits = ( int ) pfd.cColorBits;
	glConfig.depthBits = ( int ) pfd.cDepthBits;
	glConfig.stencilBits = ( int ) pfd.cStencilBits;

	return qtrue;
}

/*
** GLW_CreateWindow
**
** Responsible for creating the Win32 window and initializing the OpenGL driver.
*/
#define	WINDOW_STYLE	(WS_OVERLAPPED|WS_BORDER|WS_CAPTION|WS_VISIBLE)
static qboolean GLW_CreateWindow( int width, int height, int colorbits, qboolean cdsFullscreen )
{
	RECT			r;
	cvar_t			*vid_xpos, *vid_ypos;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;

	//
	// register the window class if necessary
	//
	if ( !s_classRegistered )
	{
		WNDCLASS wc;

		memset( &wc, 0, sizeof( wc ) );

		wc.style         = 0;
		wc.lpfnWndProc   = (WNDPROC) glw_state.wndproc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = g_wv.hInstance;
		wc.hIcon         = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = (HBRUSH__ *)COLOR_GRAYTEXT;
		wc.lpszMenuName  = 0;
		wc.lpszClassName = WINDOW_CLASS_NAME;

		if ( !RegisterClass( &wc ) )
		{
			ri.Error( ERR_FATAL, "GLW_CreateWindow: could not register window class" );
		}
		s_classRegistered = qtrue;
		ri.Printf( PRINT_ALL, "...registered window class\n" );
	}

	//
	// create the HWND if one does not already exist
	//
	if ( !g_wv.hWnd )
	{
		//
		// compute width and height
		//
		r.left = 0;
		r.top = 0;
		r.right  = width;
		r.bottom = height;

		if ( cdsFullscreen )
		{
			exstyle = 0;//WS_EX_TOPMOST;
			stylebits = WS_SYSMENU|WS_POPUP|WS_VISIBLE;	//sysmenu gives you the icon
		}
		else
		{
			exstyle = 0;
			if ( r_noborder->integer == 0 )
			{
				stylebits = WS_SYSMENU|WINDOW_STYLE|WS_MINIMIZEBOX;
			}
			else
			{
				stylebits = WS_POPUP|WS_VISIBLE;
			}
			AdjustWindowRect (&r, stylebits, FALSE);
		}

		w = r.right - r.left;
		h = r.bottom - r.top;

		if ( cdsFullscreen )
		{
			x = 0;
			y = 0;
		}
		else
		{
			vid_xpos = ri.Cvar_Get ("vid_xpos", "", 0);
			vid_ypos = ri.Cvar_Get ("vid_ypos", "", 0);
			x = vid_xpos->integer;
			y = vid_ypos->integer;

			// adjust window coordinates if necessary 
			// so that the window is completely on screen
			if ( x < 0 )
				x = 0;
			if ( y < 0 )
				y = 0;

			if ( w < glw_state.desktopWidth &&
				 h < glw_state.desktopHeight )
			{
				if ( x + w > glw_state.desktopWidth )
					x = ( glw_state.desktopWidth - w );
				if ( y + h > glw_state.desktopHeight )
					y = ( glw_state.desktopHeight - h );
			}
		}

		g_wv.hWnd = CreateWindowEx (
			 exstyle, 
			 WINDOW_CLASS_NAME,
			 WINDOW_CLASS_NAME,
			 stylebits,
			 x, y, w, h,
			 NULL,
			 NULL,
			 g_wv.hInstance,
			 NULL);

		if ( !g_wv.hWnd )
		{
			ri.Error (ERR_FATAL, "GLW_CreateWindow() - Couldn't create window");
		}
	
		ShowWindow( g_wv.hWnd, SW_SHOW );
		UpdateWindow( g_wv.hWnd );
		ri.Printf( PRINT_ALL, "...created window@%d,%d (%dx%d)\n", x, y, w, h );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...window already present, CreateWindowEx skipped\n" );
	}

	if ( !GLW_InitDriver( colorbits ) )
	{
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;

		return qfalse;
	}

	SetForegroundWindow( g_wv.hWnd );
	SetFocus( g_wv.hWnd );

	return qtrue;
}

static void PrintCDSError( int value )
{
	switch ( value )
	{
	case DISP_CHANGE_RESTART:
		ri.Printf( PRINT_ALL, "restart required\n" );
		break;
	case DISP_CHANGE_BADPARAM:
		ri.Printf( PRINT_ALL, "bad param\n" );
		break;
	case DISP_CHANGE_BADFLAGS:
		ri.Printf( PRINT_ALL, "bad flags\n" );
		break;
	case DISP_CHANGE_FAILED:
		ri.Printf( PRINT_ALL, "DISP_CHANGE_FAILED\n" );
		break;
	case DISP_CHANGE_BADMODE:
		ri.Printf( PRINT_ALL, "bad mode\n" );
		break;
	case DISP_CHANGE_NOTUPDATED:
		ri.Printf( PRINT_ALL, "not updated\n" );
		break;
	default:
		ri.Printf( PRINT_ALL, "unknown error %d\n", value );
		break;
	}
}

/*
** GLW_SetMode
*/
static rserr_t GLW_SetMode( int mode, 
							int colorbits, 
							qboolean cdsFullscreen )
{
	HDC hDC;
	const char *win_fs[] = { "W", "FS" };
	int		cdsRet;
	DEVMODE dm;
		
	//
	// print out informational messages
	//
	ri.Printf( PRINT_ALL, "...setting mode %d:", mode );
	if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d %s\n", glConfig.vidWidth, glConfig.vidHeight, win_fs[cdsFullscreen] );

	//
	// check our desktop attributes
	//
	hDC = GetDC( GetDesktopWindow() );
	glw_state.desktopBitsPixel = GetDeviceCaps( hDC, BITSPIXEL );
	glw_state.desktopWidth = GetDeviceCaps( hDC, HORZRES );
	glw_state.desktopHeight = GetDeviceCaps( hDC, VERTRES );
	ReleaseDC( GetDesktopWindow(), hDC );

	//
	// verify desktop bit depth
	//
	if ( glw_state.desktopBitsPixel < 15 || glw_state.desktopBitsPixel == 24 )
	{
		if ( colorbits == 0 || ( !cdsFullscreen && colorbits >= 15 ) )
		{
			// since I can't be bothered trying to mess around with asian codepages and MBCS stuff for a windows
			//	error box that'll only appear if something's seriously fucked then I'm going to fallback to
			//	english text when these would otherwise be used...
			//
			char sErrorHead[1024];	// ott

			extern qboolean Language_IsAsian(void);
			Q_strncpyz(sErrorHead, Language_IsAsian() ? "Low Desktop Color Depth" : SP_GetStringTextString("CON_TEXT_LOW_DESKTOP_COLOUR_DEPTH"), sizeof(sErrorHead) );

			const char *psErrorBody = Language_IsAsian() ?
												"It is highly unlikely that a correct windowed\n"
												"display can be initialized with the current\n"
												"desktop display depth.  Select 'OK' to try\n"
												"anyway.  Select 'Cancel' to try a fullscreen\n"
												"mode instead."
												:
												SP_GetStringTextString("CON_TEXT_TRY_ANYWAY");

			if ( MessageBox( NULL, 							
						psErrorBody,
						sErrorHead,
						MB_OKCANCEL | MB_ICONEXCLAMATION ) != IDOK )
			{
				return RSERR_INVALID_MODE;
			}
		}
	}

	// do a CDS if needed
	if ( cdsFullscreen )
	{
		memset( &dm, 0, sizeof( dm ) );
		
		dm.dmSize = sizeof( dm );
		
		dm.dmPelsWidth  = glConfig.vidWidth;
		dm.dmPelsHeight = glConfig.vidHeight;
		dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

		if ( r_displayRefresh->integer != 0 )
		{
			dm.dmDisplayFrequency = r_displayRefresh->integer;
			dm.dmFields |= DM_DISPLAYFREQUENCY;
		}
		
		// try to change color depth if possible
		if ( colorbits != 0 )
		{
			if ( glw_state.allowdisplaydepthchange )
			{
				dm.dmBitsPerPel = colorbits;
				dm.dmFields |= DM_BITSPERPEL;
				ri.Printf( PRINT_ALL, "...using colorsbits of %d\n", colorbits );
			}
			else
			{
				ri.Printf( PRINT_ALL, "WARNING:...changing depth not supported on Win95 < pre-OSR 2.x\n" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...using desktop display depth of %d\n", glw_state.desktopBitsPixel );
		}

		//
		// if we're already in fullscreen then just create the window
		//
		if ( glw_state.cdsFullscreen )
		{
			ri.Printf( PRINT_ALL, "...already fullscreen, avoiding redundant CDS\n" );

			if ( !GLW_CreateWindow ( glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue ) )
			{
				ri.Printf( PRINT_ALL, "...restoring display settings\n" );
				ChangeDisplaySettings( 0, 0 );
				return RSERR_INVALID_MODE;
			}
		}
		//
		// need to call CDS
		//
		else
		{
			ri.Printf( PRINT_ALL, "...calling CDS: " );
			
			// try setting the exact mode requested, because some drivers don't report
			// the low res modes in EnumDisplaySettings, but still work
			if ( ( cdsRet = ChangeDisplaySettings( &dm, CDS_FULLSCREEN ) ) == DISP_CHANGE_SUCCESSFUL )
			{
				ri.Printf( PRINT_ALL, "ok\n" );

				if ( !GLW_CreateWindow ( glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue) )
				{
					ri.Printf( PRINT_ALL, "...restoring display settings\n" );
					ChangeDisplaySettings( 0, 0 );
					return RSERR_INVALID_MODE;
				}
				
				glw_state.cdsFullscreen = qtrue;
			}
			else
			{
				//
				// the exact mode failed, so scan EnumDisplaySettings for the next largest mode
				//
				DEVMODE		devmode;
				int			modeNum;

				ri.Printf( PRINT_ALL, "failed, " );
				
				PrintCDSError( cdsRet );
			
				ri.Printf( PRINT_ALL, "...trying next higher resolution:" );
				
				// we could do a better matching job here...
				for ( modeNum = 0 ; ; modeNum++ ) {
					if ( !EnumDisplaySettings( NULL, modeNum, &devmode ) ) {
						modeNum = -1;
						break;
					}
					if ( devmode.dmPelsWidth >= glConfig.vidWidth
						&& devmode.dmPelsHeight >= glConfig.vidHeight
						&& devmode.dmBitsPerPel >= 15 ) {
						break;
					}
				}

				if ( modeNum != -1 && ( cdsRet = ChangeDisplaySettings( &devmode, CDS_FULLSCREEN ) ) == DISP_CHANGE_SUCCESSFUL )
				{
					ri.Printf( PRINT_ALL, " ok\n" );
					if ( !GLW_CreateWindow( glConfig.vidWidth, glConfig.vidHeight, colorbits, qtrue) )
					{
						ri.Printf( PRINT_ALL, "...restoring display settings\n" );
						ChangeDisplaySettings( 0, 0 );
						return RSERR_INVALID_MODE;
					}
					
					glw_state.cdsFullscreen = qtrue;
				}
				else
				{
					ri.Printf( PRINT_ALL, " failed, " );
					
					PrintCDSError( cdsRet );
					
					ri.Printf( PRINT_ALL, "...restoring display settings\n" );
					ChangeDisplaySettings( 0, 0 );
					
/*				jfm:  i took out the following code to allow fallback to mode 3, with this code it goes half windowed and just doesn't work.
					glw_state.cdsFullscreen = qfalse;
					glConfig.isFullscreen = qfalse;
					if ( !GLW_CreateWindow( glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse) )
					{
						return RSERR_INVALID_MODE;
					}
*/
					return RSERR_INVALID_FULLSCREEN;
				}
			}
		}
	}
	else
	{
		if ( glw_state.cdsFullscreen )
		{
			ChangeDisplaySettings( 0, 0 );
		}

		glw_state.cdsFullscreen = qfalse;
		if ( !GLW_CreateWindow( glConfig.vidWidth, glConfig.vidHeight, colorbits, qfalse ) )
		{
			return RSERR_INVALID_MODE;
		}
	}

	//
	// success, now check display frequency, although this won't be valid on Voodoo(2)
	//
	memset( &dm, 0, sizeof( dm ) );
	dm.dmSize = sizeof( dm );
	if ( EnumDisplaySettings( NULL, ENUM_CURRENT_SETTINGS, &dm ) )
	{
		glConfig.displayFrequency = dm.dmDisplayFrequency;
	}

	// NOTE: this is overridden later on standalone 3Dfx drivers
	glConfig.isFullscreen = cdsFullscreen;

	return RSERR_OK;
}

/*
** GLW_CheckForExtension

  Cannot use strstr directly to differentiate between (for eg) reg_combiners and reg_combiners2
*/

bool GL_CheckForExtension(const char *ext)
{
	char	*temp;
	char	term;

	temp = strstr(glConfig.extensions_string, ext);
	if(!temp)
	{
		return(false);
	}
	// String exists but it may not be terminated
	term = temp[strlen(ext)];
	if((term == ' ') || !term)
	{
		return(true);
	}
	return(false);
}

static const char *wglExtensions = NULL;

/* WGL version of the above, ASSUMES wglExtensions is non-null */
bool WGL_CheckForExtension(const char *ext)
{
	const char *ptr = Q_stristr( wglExtensions, ext );
	if (ptr == NULL)
		return false;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify it's complete string.
}

//--------------------------------------------
static void GLW_InitTextureCompression( void )
{
	qboolean newer_tc, old_tc;

	// Check for available tc methods.
	newer_tc = ( strstr( glConfig.extensions_string, "ARB_texture_compression" )
		&& strstr( glConfig.extensions_string, "EXT_texture_compression_s3tc" )) ? qtrue : qfalse;
	old_tc = ( strstr( glConfig.extensions_string, "GL_S3_s3tc" )) ? qtrue : qfalse;

	if ( old_tc )
	{
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc available\n" );
	}

	if ( newer_tc )
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc available\n" );
	}

	if ( !r_ext_compressed_textures->value )
	{
		// Compressed textures are off
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...ignoring texture compression\n" );
	}
	else if ( !old_tc && !newer_tc )
	{
		// Requesting texture compression, but no method found
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...no supported texture compression method found\n" );
		ri.Printf( PRINT_ALL, ".....ignoring texture compression\n" );
	}
	else
	{
		// some form of supported texture compression is avaiable, so see if the user has a preference
		if ( r_ext_preferred_tc_method->integer == TC_NONE )
		{
			// No preference, so pick the best
			if ( newer_tc )
			{
				ri.Printf( PRINT_ALL, "...no tc preference specified\n" );
				ri.Printf( PRINT_ALL, ".....using GL_EXT_texture_compression_s3tc\n" );
				glConfig.textureCompression = TC_S3TC_DXT;
			}
			else
			{
				ri.Printf( PRINT_ALL, "...no tc preference specified\n" );
				ri.Printf( PRINT_ALL, ".....using GL_S3_s3tc\n" );
				glConfig.textureCompression = TC_S3TC;
			}
		}
		else
		{
			// User has specified a preference, now see if this request can be honored
			if ( old_tc && newer_tc )
			{
				// both are avaiable, so we can use the desired tc method
				if ( r_ext_preferred_tc_method->integer == TC_S3TC )
				{
					ri.Printf( PRINT_ALL, "...using preferred tc method, GL_S3_s3tc\n" );
					glConfig.textureCompression = TC_S3TC;
				}
				else
				{
					ri.Printf( PRINT_ALL, "...using preferred tc method, GL_EXT_texture_compression_s3tc\n" );
					glConfig.textureCompression = TC_S3TC_DXT;
				}
			}
			else
			{
				// Both methods are not available, so this gets trickier
				if ( r_ext_preferred_tc_method->integer == TC_S3TC )
				{
					// Preferring to user older compression
					if ( old_tc )
					{
						ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
						glConfig.textureCompression = TC_S3TC;
					}
					else
					{
						// Drat, preference can't be honored 
						ri.Printf( PRINT_ALL, "...preferred tc method, GL_S3_s3tc not available\n" );
						ri.Printf( PRINT_ALL, ".....falling back to GL_EXT_texture_compression_s3tc\n" );
						glConfig.textureCompression = TC_S3TC_DXT;
					}
				}
				else
				{
					// Preferring to user newer compression
					if ( newer_tc )
					{
						ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
						glConfig.textureCompression = TC_S3TC_DXT;
					}
					else
					{
						// Drat, preference can't be honored 
						ri.Printf( PRINT_ALL, "...preferred tc method, GL_EXT_texture_compression_s3tc not available\n" );
						ri.Printf( PRINT_ALL, ".....falling back to GL_S3_s3tc\n" );
						glConfig.textureCompression = TC_S3TC;
					}
				}
			}
		}
	}
}

/*
** GLW_InitExtensions
*/
static void GLW_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
#ifdef JEDIACADEMY_GLOW
		g_bDynamicGlowSupported = false;
		ri.Cvar_Set( "r_DynamicGlow","0" );
#endif
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// Debugging
	//extern void (APIENTRYP qglDebugMessageCallback) (DEBUGPROC callback, const void* userParam);
	qglDebugMessageCallback = (void (APIENTRY*) (DEBUGPROC,const void*)) qwglGetProcAddress("glDebugMessageCallback");
	//glEnable = (void (APIENTRY*) (DEBUGPROC,const void*)) qwglGetProcAddress("glDebugMessageCallback");


	// Select our tc scheme
	GLW_InitTextureCompression();

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_EXT_texture_filter_anisotropic
	glConfig.textureFilterAnisotropicAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_filter_anisotropic" ) )
	{
		glConfig.textureFilterAnisotropicAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic available\n" );

		if ( r_ext_texture_filter_anisotropic->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "1" );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "0" );
	}



	// GL_EXT_clamp_to_edge
	glConfig.clampToEdgeAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "GL_EXT_texture_edge_clamp" ) )
	{
		glConfig.clampToEdgeAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...Using GL_EXT_texture_edge_clamp\n" );
	}


	// WGL_EXT_swap_control
	qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" );
	if ( qwglSwapIntervalEXT )
	{
		ri.Printf( PRINT_ALL, "...using WGL_EXT_swap_control\n" );
		r_swapInterval->modified = qtrue;	// force a set next frame
	}
	else
	{
		ri.Printf( PRINT_ALL, "...WGL_EXT_swap_control not found\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( strstr( glConfig.extensions_string, "GL_ARB_multitexture" )  )
	{
		if ( r_ext_multitexture->integer )
		{
			qglMultiTexCoord2fARB = ( void ( APIENTRY * ) ( GLenum, GLfloat, GLfloat ) ) qwglGetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( void ( APIENTRY * ) ( GLenum ) ) qwglGetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( void ( APIENTRY * ) ( GLenum ) ) qwglGetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB )
			{
				qglGetIntegerv( GL_MAX_ACTIVE_TEXTURES_ARB, &glConfig.maxActiveTextures );

				if ( glConfig.maxActiveTextures > 1 )
				{
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	if ( strstr( glConfig.extensions_string, "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void ( APIENTRY * )( int, int ) ) qwglGetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) qwglGetProcAddress( "glUnlockArraysEXT" );
			if (!qglLockArraysEXT || !qglUnlockArraysEXT) {
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	if ( strstr( glConfig.extensions_string, "GL_EXT_point_parameters" ) )
	{
		if ( r_ext_compiled_vertex_array->integer || 1)
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_point_parameters\n" );
			qglPointParameterfEXT = ( void ( APIENTRY * )( GLenum, GLfloat) ) qwglGetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void ( APIENTRY * )( GLenum, GLfloat *) ) qwglGetProcAddress( "glPointParameterfvEXT" );
			if (!qglPointParameterfEXT || !qglPointParameterfvEXT) 
			{
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_point_parameters\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_point_parameters not found\n" );
	}

	typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
	PFNWGLGETEXTENSIONSSTRINGARBPROC			qwglGetExtensionsStringARB;
	qwglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC) qwglGetProcAddress("wglGetExtensionsStringARB");

#ifdef JEDIACADEMY_GLOW
	bool bNVRegisterCombiners = false;
	// Register Combiners.
	if ( GL_CheckForExtension( "GL_NV_register_combiners" ) )
	{
		// NOTE: This extension requires multitexture support (over 2 units).
		if ( glConfig.maxActiveTextures >= 2 )
		{
			bNVRegisterCombiners = true;
			// Register Combiners function pointer address load.	- AReis
			// NOTE: VV guys will _definetly_ not be able to use regcoms. Pixel Shaders are just as good though :-)
			// NOTE: Also, this is an nVidia specific extension (of course), so fragment shaders would serve the same purpose
			// if we needed some kind of fragment/pixel manipulation support.
			qglCombinerParameterfvNV = ( PFNGLCOMBINERPARAMETERFVNVPROC ) qwglGetProcAddress( "glCombinerParameterfvNV" );
			qglCombinerParameterivNV = ( PFNGLCOMBINERPARAMETERIVNVPROC ) qwglGetProcAddress( "glCombinerParameterivNV" );
			qglCombinerParameterfNV = ( PFNGLCOMBINERPARAMETERFNVPROC ) qwglGetProcAddress( "glCombinerParameterfNV" );
			qglCombinerParameteriNV = ( PFNGLCOMBINERPARAMETERINVPROC ) qwglGetProcAddress( "glCombinerParameteriNV" );
			qglCombinerInputNV = ( PFNGLCOMBINERINPUTNVPROC ) qwglGetProcAddress( "glCombinerInputNV" );
			qglCombinerOutputNV = ( PFNGLCOMBINEROUTPUTNVPROC ) qwglGetProcAddress( "glCombinerOutputNV" );
			qglFinalCombinerInputNV = ( PFNGLFINALCOMBINERINPUTNVPROC ) qwglGetProcAddress( "glFinalCombinerInputNV" );
			qglGetCombinerInputParameterfvNV	= ( PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC ) qwglGetProcAddress( "glGetCombinerInputParameterfvNV" );
			qglGetCombinerInputParameterivNV	= ( PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC ) qwglGetProcAddress( "glGetCombinerInputParameterivNV" );
			qglGetCombinerOutputParameterfvNV = ( PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC ) qwglGetProcAddress( "glGetCombinerOutputParameterfvNV" );
			qglGetCombinerOutputParameterivNV = ( PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC ) qwglGetProcAddress( "glGetCombinerOutputParameterivNV" );
			qglGetFinalCombinerInputParameterfvNV = ( PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC ) qwglGetProcAddress( "glGetFinalCombinerInputParameterfvNV" );
			qglGetFinalCombinerInputParameterivNV = ( PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC ) qwglGetProcAddress( "glGetFinalCombinerInputParameterivNV" );

			// Validate the functions we need.
			if ( !qglCombinerParameterfvNV || !qglCombinerParameterivNV || !qglCombinerParameterfNV || !qglCombinerParameteriNV || !qglCombinerInputNV ||
				 !qglCombinerOutputNV || !qglFinalCombinerInputNV || !qglGetCombinerInputParameterfvNV || !qglGetCombinerInputParameterivNV ||
				 !qglGetCombinerOutputParameterfvNV || !qglGetCombinerOutputParameterivNV || !qglGetFinalCombinerInputParameterfvNV || !qglGetFinalCombinerInputParameterivNV )
			{
				bNVRegisterCombiners = false;
				qglCombinerParameterfvNV = NULL;
				qglCombinerParameteriNV = NULL;
				Com_Printf ("...GL_NV_register_combiners failed\n" );
			}
		}
		else
		{
			bNVRegisterCombiners = false;
			Com_Printf ("...ignoring GL_NV_register_combiners\n" );
		}
	}
	else
	{
		bNVRegisterCombiners = false;
		Com_Printf ("...GL_NV_register_combiners not found\n" );
	}

	// NOTE: Vertex and Fragment Programs are very dependant on each other - this is actually a
	// good thing! So, just check to see which we support (one or the other) and load the shared
	// function pointers. ARB rocks!

	// Vertex Programs.
	bool bARBVertexProgram = false;
	if ( GL_CheckForExtension( "GL_ARB_vertex_program" ) )
	{
		bARBVertexProgram = true;
	}
	else
	{
		bARBVertexProgram = false;
		Com_Printf ("...GL_ARB_vertex_program not found\n" );
	}

	// Fragment Programs.
	bool bARBFragmentProgram = false;
	if ( GL_CheckForExtension( "GL_ARB_fragment_program" ) )
	{
		bARBFragmentProgram = true;
	}
	else
	{
		bARBFragmentProgram = false;
		Com_Printf ("...GL_ARB_fragment_program not found\n" );
	}

	// If we support one or the other, load the shared function pointers.
	if ( bARBVertexProgram || bARBFragmentProgram )
	{
		qglProgramStringARB					= (PFNGLPROGRAMSTRINGARBPROC)  qwglGetProcAddress("glProgramStringARB");
		qglBindProgramARB					= (PFNGLBINDPROGRAMARBPROC)    qwglGetProcAddress("glBindProgramARB");
		qglDeleteProgramsARB				= (PFNGLDELETEPROGRAMSARBPROC) qwglGetProcAddress("glDeleteProgramsARB");
		qglGenProgramsARB					= (PFNGLGENPROGRAMSARBPROC)    qwglGetProcAddress("glGenProgramsARB");
		qglProgramEnvParameter4dARB			= (PFNGLPROGRAMENVPARAMETER4DARBPROC)    qwglGetProcAddress("glProgramEnvParameter4dARB");
		qglProgramEnvParameter4dvARB		= (PFNGLPROGRAMENVPARAMETER4DVARBPROC)   qwglGetProcAddress("glProgramEnvParameter4dvARB");
		qglProgramEnvParameter4fARB			= (PFNGLPROGRAMENVPARAMETER4FARBPROC)    qwglGetProcAddress("glProgramEnvParameter4fARB");
		qglProgramEnvParameter4fvARB		= (PFNGLPROGRAMENVPARAMETER4FVARBPROC)   qwglGetProcAddress("glProgramEnvParameter4fvARB");
		qglProgramLocalParameter4dARB		= (PFNGLPROGRAMLOCALPARAMETER4DARBPROC)  qwglGetProcAddress("glProgramLocalParameter4dARB");
		qglProgramLocalParameter4dvARB		= (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) qwglGetProcAddress("glProgramLocalParameter4dvARB");
		qglProgramLocalParameter4fARB		= (PFNGLPROGRAMLOCALPARAMETER4FARBPROC)  qwglGetProcAddress("glProgramLocalParameter4fARB");
		qglProgramLocalParameter4fvARB		= (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) qwglGetProcAddress("glProgramLocalParameter4fvARB");
		qglGetProgramEnvParameterdvARB		= (PFNGLGETPROGRAMENVPARAMETERDVARBPROC) qwglGetProcAddress("glGetProgramEnvParameterdvARB");
		qglGetProgramEnvParameterfvARB		= (PFNGLGETPROGRAMENVPARAMETERFVARBPROC) qwglGetProcAddress("glGetProgramEnvParameterfvARB");
		qglGetProgramLocalParameterdvARB	= (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) qwglGetProcAddress("glGetProgramLocalParameterdvARB");
		qglGetProgramLocalParameterfvARB	= (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) qwglGetProcAddress("glGetProgramLocalParameterfvARB");
		qglGetProgramivARB					= (PFNGLGETPROGRAMIVARBPROC)     qwglGetProcAddress("glGetProgramivARB");
		qglGetProgramStringARB				= (PFNGLGETPROGRAMSTRINGARBPROC) qwglGetProcAddress("glGetProgramStringARB");
		qglIsProgramARB						= (PFNGLISPROGRAMARBPROC)        qwglGetProcAddress("glIsProgramARB");
		//teh's PBO
		qglGenBuffersARB					= (PFNGLGENBUFFERSARBPROC)       qwglGetProcAddress("glGenBuffersARB");
		qglBindBufferARB					= (PFNGLBINDBUFFERARBPROC)       qwglGetProcAddress("glBindBufferARB");
		qglBufferDataARB					= (PFNGLBUFFERDATAARBPROC)       qwglGetProcAddress("glBufferDataARB");
		qglMapBufferARB						= (PFNGLMAPBUFFERARBPROC)		 qwglGetProcAddress("glMapBufferARB");
		qglUnmapBufferARB					= (PFNGLUNMAPBUFFERARBPROC)		 qwglGetProcAddress("glUnmapBufferARB");

		// Validate the functions we need.
		if ( !qglProgramStringARB || !qglBindProgramARB || !qglDeleteProgramsARB || !qglGenProgramsARB ||
			 !qglProgramEnvParameter4dARB || !qglProgramEnvParameter4dvARB || !qglProgramEnvParameter4fARB ||
             !qglProgramEnvParameter4fvARB || !qglProgramLocalParameter4dARB || !qglProgramLocalParameter4dvARB ||
             !qglProgramLocalParameter4fARB || !qglProgramLocalParameter4fvARB || !qglGetProgramEnvParameterdvARB ||
             !qglGetProgramEnvParameterfvARB || !qglGetProgramLocalParameterdvARB || !qglGetProgramLocalParameterfvARB ||
             !qglGetProgramivARB || !qglGetProgramStringARB || !qglIsProgramARB )
		{
			bARBVertexProgram = false;
			bARBFragmentProgram = false;
			qglGenProgramsARB = NULL;	//clear ptrs that get checked
			qglProgramEnvParameter4fARB = NULL;
			Com_Printf ("...ignoring GL_ARB_vertex_program\n" );
			Com_Printf ("...ignoring GL_ARB_fragment_program\n" );
		}
	}

	// Figure out which texture rectangle extension to use.
	bool bTexRectSupported = false;
	if ( Q_stricmpn( glConfig.vendor_string, "ATI Technologies",16 )==0
		&& Q_stricmpn( glConfig.version_string, "1.3.3",5 )==0 
		&& glConfig.version_string[5] < '9' ) //1.3.34 and 1.3.37 and 1.3.38 are broken for sure, 1.3.39 is not
	{
		g_bTextureRectangleHack = true;
	}

	if ( GL_CheckForExtension( "GL_NV_texture_rectangle" ) || GL_CheckForExtension( "GL_EXT_texture_rectangle" ) )
	{
		bTexRectSupported = true;
	}

	bool bHasPixelFormat = false;
	bool bHasRenderTexture = false;

	// Get the WGL extensions string.
	if ( qwglGetExtensionsStringARB )
	{
		wglExtensions = qwglGetExtensionsStringARB( glw_state.hDC );
	}

	// This externsion is used to get the wgl extension string.
	if ( wglExtensions )
	{
		// Pixel Format.
		if ( WGL_CheckForExtension( "WGL_ARB_pixel_format" ) )
		{
			qwglGetPixelFormatAttribivARB			=	(PFNWGLGETPIXELFORMATATTRIBIVARBPROC) qwglGetProcAddress("wglGetPixelFormatAttribivARB");
			qwglGetPixelFormatAttribfvARB			=	(PFNWGLGETPIXELFORMATATTRIBFVARBPROC) qwglGetProcAddress("wglGetPixelFormatAttribfvARB");
			qwglChoosePixelFormatARB				=	(PFNWGLCHOOSEPIXELFORMATARBPROC) qwglGetProcAddress("wglChoosePixelFormatARB");
	
			// Validate the functions we need.
			if ( !qwglGetPixelFormatAttribivARB || !qwglGetPixelFormatAttribfvARB || !qwglChoosePixelFormatARB )
			{
				Com_Printf ("...ignoring WGL_ARB_pixel_format\n" );
			}
			else
			{
				bHasPixelFormat = true;
			}
		}
		else
		{
			Com_Printf ("...ignoring WGL_ARB_pixel_format\n" );
		}

		// Offscreen pixel-buffer.
		// NOTE: VV guys can use the equivelant SetRenderTarget() with the correct texture surfaces.
		bool bWGLARBPbuffer = false;
		if ( WGL_CheckForExtension( "WGL_ARB_pbuffer" ) && bHasPixelFormat )
		{
			bWGLARBPbuffer = true;
			qwglCreatePbufferARB		=	(PFNWGLCREATEPBUFFERARBPROC) qwglGetProcAddress("wglCreatePbufferARB");
			qwglGetPbufferDCARB			=	(HDC (WINAPI *) (HPBUFFERARB hPbuffer)) qwglGetProcAddress("wglGetPbufferDCARB");
			qwglReleasePbufferDCARB		=	(int (WINAPI *) (HPBUFFERARB hPbuffer, HDC hDC)) qwglGetProcAddress("wglReleasePbufferDCARB");
			qwglDestroyPbufferARB		=	(BOOL (WINAPI *) (HPBUFFERARB hPbuffer)) qwglGetProcAddress("wglDestroyPbufferARB");
			qwglQueryPbufferARB			=	(BOOL (WINAPI *) (HPBUFFERARB hPbuffer, int iAttribute, int *piValue)) qwglGetProcAddress("wglQueryPbufferARB");
	
			// Validate the functions we need.
			if ( !qwglCreatePbufferARB || !qwglGetPbufferDCARB || !qwglReleasePbufferDCARB || !qwglDestroyPbufferARB || !qwglQueryPbufferARB )
			{
				bWGLARBPbuffer = false;
				Com_Printf ("...WGL_ARB_pbuffer failed\n" );
			}
		}
		else
		{
			bWGLARBPbuffer = false;
			Com_Printf ("...WGL_ARB_pbuffer not found\n" );
		}

		// Render-Texture (requires pbuffer ext (and it's dependancies of course).
		if ( WGL_CheckForExtension( "WGL_ARB_render_texture" ) && bWGLARBPbuffer )
		{
			qwglBindTexImageARB			=	(PFNWGLBINDTEXIMAGEARBPROC) qwglGetProcAddress("wglBindTexImageARB");
			qwglReleaseTexImageARB		=	(PFNWGLRELEASETEXIMAGEARBPROC) qwglGetProcAddress("wglReleaseTexImageARB");
			qwglSetPbufferAttribARB		=	(PFNWGLSETPBUFFERATTRIBARBPROC) qwglGetProcAddress("wglSetPbufferAttribARB");
	
			// Validate the functions we need.
			if ( !qwglCreatePbufferARB || !qwglGetPbufferDCARB || !qwglReleasePbufferDCARB || !qwglDestroyPbufferARB || !qwglQueryPbufferARB )
			{
				Com_Printf ("...ignoring WGL_ARB_render_texture\n" );
			}
			else
			{
				bHasRenderTexture = true;
			}
		}
		else
		{
			Com_Printf ("...ignoring WGL_ARB_render_texture\n" );
		}
	}

	// Find out how many general combiners they have.
	#define GL_MAX_GENERAL_COMBINERS_NV       0x854D
	GLint iNumGeneralCombiners = 0;
	if(bNVRegisterCombiners)
		qglGetIntegerv( GL_MAX_GENERAL_COMBINERS_NV, &iNumGeneralCombiners );

	// Only allow dynamic glows/flares if they have the hardware
	if ( bTexRectSupported && bARBVertexProgram && bHasRenderTexture && qglActiveTextureARB && glConfig.maxActiveTextures >= 4 &&
		( ( bNVRegisterCombiners && iNumGeneralCombiners >= 2 ) || bARBFragmentProgram ) )
	{
		g_bDynamicGlowSupported = true;
		// this would overwrite any achived setting gwg
		// ri.Cvar_Set( "r_DynamicGlow", "1" );
	}
	else
	{
		g_bDynamicGlowSupported = false;
		ri.Cvar_Set( "r_DynamicGlow","0" );
	}


	//mme
	if (strstr(glConfig.extensions_string, "GL_EXT_texture_filter_anisotropic"))
	{

		qglGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glMMEConfig.maxAnisotropy);
		if (glMMEConfig.maxAnisotropy <= 0) {
			ri.Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n");
		}
		else {
			ri.Printf(PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %d)\n", glMMEConfig.maxAnisotropy);
		}
	}
	else
	{
		glMMEConfig.maxAnisotropy = 0;
		ri.Printf(PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n");
	}

	glMMEConfig.framebufferObject = qfalse;
	glMMEConfig.shaderSupport = qfalse;
	if (strstr(glConfig.extensions_string, "GL_EXT_framebuffer_object") &&
		(1 || strstr(glConfig.extensions_string, "GL_ARB_texture_non_power_of_two"))) {
		ri.Printf(PRINT_ALL, "...using GL_EXT_framebuffer_object\n");
		glMMEConfig.framebufferObject = qtrue;
		qglGenFramebuffers = (void (APIENTRY*)(GLsizei, GLuint*)) qwglGetProcAddress("glGenFramebuffersEXT");
		qglBindFramebuffer = (void (APIENTRY*)(GLenum, GLuint)) qwglGetProcAddress("glBindFramebufferEXT");
		qglGenRenderbuffers = (void (APIENTRY*)(GLsizei, GLuint*)) qwglGetProcAddress("glGenRenderbuffersEXT");
		qglBindRenderbuffer = (void (APIENTRY*)(GLenum, GLuint)) qwglGetProcAddress("glBindRenderbufferEXT");
		qglRenderbufferStorage = (void (APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei)) qwglGetProcAddress("glRenderbufferStorageEXT");
		qglFramebufferRenderbuffer = (void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint)) qwglGetProcAddress("glFramebufferRenderbufferEXT");
		qglFramebufferTexture2D = (void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint)) qwglGetProcAddress("glFramebufferTexture2DEXT");
		qglCheckFramebufferStatus = (GLenum(APIENTRY*)(GLenum)) qwglGetProcAddress("glCheckFramebufferStatusEXT");
		qglDeleteFramebuffers = (void (APIENTRY*)(GLsizei, const GLuint*)) qwglGetProcAddress("glDeleteFramebuffersEXT");
		qglDeleteRenderbuffers = (void (APIENTRY*)(GLsizei, const GLuint*)) qwglGetProcAddress("glDeleteRenderbuffersEXT");

		if (!strstr(glConfig.extensions_string, "GL_ARB_depth_texture")) {
			ri.Printf(PRINT_WARNING, "WARNING: GL_ARB_depth_texture is missing\n");
		}
		if (!strstr(glConfig.extensions_string, "GL_EXT_packed_depth_stencil") ||
			!strstr(glConfig.extensions_string, "GL_NV_packed_depth_stencil")) {
			ri.Printf(PRINT_WARNING, "WARNING: packed_depth_stencil is missing\n");
		}
	}
	glMMEConfig.framebufferMultiSample = qfalse;
	if (strstr(glConfig.extensions_string, "GL_EXT_framebuffer_multisample") && strstr(glConfig.extensions_string, "GL_EXT_framebuffer_blit")) {
		qglRenderbufferStorageMultisampleEXT = (void (APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei))qwglGetProcAddress("glRenderbufferStorageMultisampleEXT");
		qglBlitFramebufferEXT = (void (APIENTRYP)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))  qwglGetProcAddress("glBlitFramebufferEXT");
		glMMEConfig.framebufferMultiSample = qfalse;
	}
	//added fragment/vertex program extensions
	if (strstr(glConfig.extensions_string, "GL_ARB_fragment_shader") &&
		strstr(glConfig.extensions_string, "GL_ARB_vertex_program") &&
		strstr(glConfig.extensions_string, "GL_ARB_vertex_shader") &&
		strstr(glConfig.extensions_string, "GL_ARB_fragment_program") &&
		strstr(glConfig.extensions_string, "GL_ARB_shading_language_100"))
	{
		ri.Printf(PRINT_ALL, "...using GL_ARB_fragment_program\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_vertex_program\n");
		ri.Printf(PRINT_ALL, "...using GL_ARB_shading_language_100\n");
		glMMEConfig.shaderSupport = qtrue;
		qglAttachShader = (void (APIENTRY*) (GLuint, GLuint)) qwglGetProcAddress("glAttachShader");
		qglBindAttribLocation = (void (APIENTRY*) (GLuint, GLuint, const GLchar*)) qwglGetProcAddress("glBindAttribLocation");
		qglCompileShader = (void (APIENTRY*) (GLuint)) qwglGetProcAddress("glCompileShader");
		qglCreateProgram = (GLuint(APIENTRY*) (void)) qwglGetProcAddress("glCreateProgram");
		qglCreateShader = (GLuint(APIENTRY*) (GLenum)) qwglGetProcAddress("glCreateShader");
		qglDeleteProgram = (void (APIENTRY*) (GLuint)) qwglGetProcAddress("glDeleteProgram");
		qglDeleteShader = (void (APIENTRY*) (GLuint)) qwglGetProcAddress("glDeleteShader");
		qglShaderSource = (void (APIENTRY*) (GLuint, GLsizei, const GLchar**, const GLint*)) qwglGetProcAddress("glShaderSource");
		qglLinkProgram = (void (APIENTRY*) (GLuint)) qwglGetProcAddress("glLinkProgram");
		qglUseProgram = (void (APIENTRY*) (GLuint)) qwglGetProcAddress("glUseProgram");
		qglGetUniformLocation = (GLint(APIENTRY*) (GLuint, const GLchar*)) qwglGetProcAddress("glGetUniformLocation");
		qglUniform1f = (void (APIENTRY*) (GLint, GLfloat)) qwglGetProcAddress("glUniform1f");
		qglUniform2f = (void (APIENTRY*) (GLint, GLfloat, GLfloat)) qwglGetProcAddress("glUniform2f");
		qglUniform1i = (void (APIENTRY*) (GLint, GLint)) qwglGetProcAddress("glUniform1i");
		qglGetProgramiv = (void (APIENTRY*) (GLuint, GLenum, GLint*)) qwglGetProcAddress("glGetProgramiv");
		qglGetProgramInfoLog = (void (APIENTRY*) (GLuint, GLsizei, GLsizei*, GLchar*)) qwglGetProcAddress("glGetProgramInfoLog");
		qglGetShaderiv = (void (APIENTRY*) (GLuint, GLenum, GLint*)) qwglGetProcAddress("glGetShaderiv");
		qglGetShaderInfoLog = (void (APIENTRY*) (GLuint, GLsizei, GLsizei*, GLchar*)) qwglGetProcAddress("glGetShaderInfoLog");
	}
#endif
}

/*
** GLW_CheckOSVersion
*/
static qboolean GLW_CheckOSVersion( void )
{
#define OSR2_BUILD_NUMBER 1111

	OSVERSIONINFO	vinfo;

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	glw_state.allowdisplaydepthchange = qfalse;

	if ( GetVersionEx( &vinfo) )
	{
		if ( vinfo.dwMajorVersion > 4 )
		{
			glw_state.allowdisplaydepthchange = qtrue;
		}
		else if ( vinfo.dwMajorVersion == 4 )
		{
			if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
			{
				glw_state.allowdisplaydepthchange = qtrue;
			}
			else if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
			{
				if ( LOWORD( vinfo.dwBuildNumber ) >= OSR2_BUILD_NUMBER )
				{
					glw_state.allowdisplaydepthchange = qtrue;
				}
			}
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "GLW_CheckOSVersion() - GetVersionEx failed\n" );
		return qfalse;
	}

	return qtrue;
}

/*
** GLW_LoadOpenGL
**
** GLimp_win.c internal function that attempts to load and use 
** a specific OpenGL DLL.
*/
static qboolean GLW_LoadOpenGL( )
{
	char buffer[1024];
	qboolean cdsFullscreen;

	Q_strlwr( strcpy( buffer, OPENGL_DRIVER_NAME ) );

	//
	// load the driver and bind our function pointers to it
	// 
	if ( QGL_Init( buffer ) ) 
	{
		cdsFullscreen = (qboolean)r_fullscreen->integer;

		// create the window and set up the context
		if ( !GLW_StartDriverAndSetMode( r_mode->integer, r_colorbits->integer, cdsFullscreen ) )
		{
			// if we're on a 24/32-bit desktop and we're going fullscreen on an ICD,
			// try it again but with a 16-bit desktop
			if ( r_colorbits->integer != 16 ||
				 cdsFullscreen != qtrue ||
				 r_mode->integer != 3 )
			{
				if ( !GLW_StartDriverAndSetMode( 3, 16, qtrue ) )
				{
					goto fail;
				}
			}
		}
		return qtrue;
	}
fail:

	QGL_Shutdown();

	return qfalse;
}

/*
** GLimp_EndFrame
*/
void GLimp_EndFrame (void)
{
	//
	// swapinterval stuff
	//
	if ( r_swapInterval->modified ) {
		r_swapInterval->modified = qfalse;

		if ( !glConfig.stereoEnabled ) {	// why?
			if ( qwglSwapIntervalEXT ) {
				qwglSwapIntervalEXT( r_swapInterval->integer );
			}
		}
	}


	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SwapBuffers( glw_state.hDC );
	}

	// check logging
	QGL_EnableLogging( (qboolean)r_logFile->integer );
}

static void GLW_StartOpenGL( void )
{
	//
	// load and initialize the specific OpenGL driver
	//
	if ( !GLW_LoadOpenGL() )
	{
		ri.Error( ERR_FATAL, "GLW_StartOpenGL() - could not load OpenGL subsystem\n" );
	}
}

/*
** GLimp_Init
**
** This is the platform specific OpenGL initialization function.  It
** is responsible for loading OpenGL, initializing it, setting
** extensions, creating a window of the appropriate size, doing
** fullscreen manipulations, etc.  Its overall responsibility is
** to make sure that a functional OpenGL subsystem is operating
** when it returns to the ref.
*/
void GLimp_Init( void )
{
	char	buf[MAX_STRING_CHARS];
	cvar_t *lastValidRenderer = ri.Cvar_Get( "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );
	cvar_t	*cv;

	ri.Printf( PRINT_ALL, "Initializing OpenGL subsystem\n" );

	//
	// check OS version to see if we can do fullscreen display changes
	//
	if ( !GLW_CheckOSVersion() )
	{
		ri.Error( ERR_FATAL, "GLimp_Init() - incorrect operating system\n" );
	}

	// save off hInstance and wndproc
	cv = ri.Cvar_Get( "win_hinstance", "", 0 );
	sscanf( cv->string, "%i", (int *)&g_wv.hInstance );

	cv = ri.Cvar_Get( "win_wndproc", "", 0 );
	sscanf( cv->string, "%i", (int *)&glw_state.wndproc );

	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );

	// load appropriate DLL and initialize subsystem
	GLW_StartOpenGL();

	// get our config strings
	const char* glstring;
	glstring = (const char *)qglGetString (GL_VENDOR);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.vendor_string, glstring, sizeof( glConfig.vendor_string ) );
	glstring = (const char *)qglGetString (GL_RENDERER);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.renderer_string, glstring, sizeof( glConfig.renderer_string ) );
	glstring = (const char *)qglGetString (GL_VERSION);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.version_string, glstring, sizeof( glConfig.version_string ) );
	glstring = (const char *)qglGetString (GL_EXTENSIONS);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.extensions_string, glstring, sizeof( glConfig.extensions_string ) );

	// OpenGL driver constants
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize );
	// stubbed or broken drivers may have reported 0...
	if ( glConfig.maxTextureSize <= 0 ) 
	{
		glConfig.maxTextureSize = 0;
	}
	GLW_InitExtensions();

	//
	// chipset specific configuration
	//
	Q_strncpyz( buf, glConfig.renderer_string, sizeof(buf) );
	Q_strlwr( buf );
	
	//
	// NOTE: if changing cvars, do it within this block.  This allows them
	// to be overridden when testing driver fixes, etc. but only sets
	// them to their default state when the hardware is first installed/run.
	//
extern qboolean Sys_LowPhysicalMemory();
	if ( Q_stricmp( lastValidRenderer->string, glConfig.renderer_string ) )
	{
		if (Sys_LowPhysicalMemory())
		{
			ri.Cvar_Set("s_khz", "11");// this will get called before S_Init
		}
		//reset to defaults
		ri.Cvar_Set( "r_picmip", "1" );

		// Savage3D and Savage4 should always have trilinear enabled
		if ( strstr( buf, "savage3d" ) || strstr( buf, "s3 savage4" ) || strstr( buf, "geforce" ))
		{
			ri.Cvar_Set( "r_texturemode", "GL_LINEAR_MIPMAP_LINEAR" );
		}
		else
		{
			ri.Cvar_Set( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" );
		}
		
		if ( strstr( buf, "kyro" ) )	
		{
			ri.Cvar_Set( "r_ext_texture_filter_anisotropic", "0");	//KYROs have it avail, but suck at it!
			ri.Cvar_Set( "r_ext_preferred_tc_method", "1");			//(Use DXT1 instead of DXT5 - same quality but much better performance on KYRO)
		}
		
		//this must be a really sucky card!
		if ( (glConfig.textureCompression == TC_NONE) || (glConfig.maxActiveTextures < 2)  || (glConfig.maxTextureSize <= 512) )
		{
			ri.Cvar_Set( "r_picmip", "2");
			ri.Cvar_Set( "r_lodbias", "2");			
			ri.Cvar_Set( "r_detailtextures", "0");
			ri.Cvar_Set( "r_colorbits", "16");
			ri.Cvar_Set( "r_texturebits", "16");
			ri.Cvar_Set( "cg_shadows", "0");
			ri.Cvar_Set( "r_mode", "3");	//force 640
		}
	}
	
	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );

	WG_CheckHardwareGamma();
}

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.
*/
void GLimp_Shutdown( void )
{
//	const char *strings[] = { "soft", "hard" };
	const char *success[] = { "failed", "success" };
	int retVal;

	// FIXME: Brian, we need better fallbacks from partially initialized failures
	if ( !qwglMakeCurrent ) {
		return;
	}

	ri.Printf( PRINT_ALL, "Shutting down OpenGL subsystem\n" );

	// restore gamma.  We do this first because 3Dfx's extension needs a valid OGL subsystem
	WG_RestoreGamma();

	// set current context to NULL
	if ( qwglMakeCurrent )
	{
		retVal = qwglMakeCurrent( NULL, NULL ) != 0;

		ri.Printf( PRINT_ALL, "...wglMakeCurrent( NULL, NULL ): %s\n", success[retVal] );
	}


	if ( glw_state.pbuf.hGLRC )
	{
		retVal = qwglDeleteContext( glw_state.pbuf.hGLRC ) != 0;
		glw_state.pbuf.hGLRC = NULL;
	}

	if ( glw_state.pbuf.hDC )
	{
		retVal = qwglReleasePbufferDCARB( glw_state.pbuf.buffer, glw_state.pbuf.hDC ) != 0;
		glw_state.pbuf.hDC = NULL;
	}

	if ( glw_state.pbuf.buffer )
	{
		retVal = qwglDestroyPbufferARB( glw_state.pbuf.buffer ) != 0;
		glw_state.pbuf.buffer = NULL;
	}


	// delete HGLRC
	if ( glw_state.hGLRC )
	{
		retVal = qwglDeleteContext( glw_state.hGLRC ) != 0;
		ri.Printf( PRINT_ALL, "...deleting GL context: %s\n", success[retVal] );
		glw_state.hGLRC = NULL;
	}

	// release DC
	if ( glw_state.hDC )
	{
		retVal = ReleaseDC( g_wv.hWnd, glw_state.hDC ) != 0;
		ri.Printf( PRINT_ALL, "...releasing DC: %s\n", success[retVal] );
		glw_state.hDC   = NULL;
	}

	// destroy window
	if ( g_wv.hWnd )
	{
		ri.Printf( PRINT_ALL, "...destroying window\n" );
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;
		glw_state.pixelFormatSet = qfalse;
	}

	// close the r_logFile
	if ( glw_state.log_fp )
	{
		fclose( glw_state.log_fp );
		glw_state.log_fp = 0;
	}

	// reset display settings
	if ( glw_state.cdsFullscreen )
	{
		ri.Printf( PRINT_ALL, "...resetting display\n" );
		ChangeDisplaySettings( 0, 0 );
		glw_state.cdsFullscreen = qfalse;
	}

	// shutdown QGL subsystem
	QGL_Shutdown();

	memset( &glConfig, 0, sizeof( glConfig ) );
	memset( &glState, 0, sizeof( glState ) );
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
        if ( g_wv.hWnd ) {
                // Todo with viewlog maybe should try to unminimize but mer.
                ShowWindow( g_wv.hWnd, SW_MINIMIZE );
        }
}

/*
** GLimp_LogComment
*/
void GLimp_LogComment( char *comment ) 
{
	if ( glw_state.log_fp ) {
		fprintf( glw_state.log_fp, "%s", comment );
	}
}


/*
===========================================================

SMP acceleration

===========================================================
*/

HANDLE	renderCommandsEvent;
HANDLE	renderCompletedEvent;
HANDLE	renderActiveEvent;

void (*glimpRenderThread)( void );

void GLimp_RenderThreadWrapper( void ) {
	glimpRenderThread();

	// unbind the context before we die
	qwglMakeCurrent( glw_state.hDC, NULL );
}

/*
=======================
GLimp_SpawnRenderThread
=======================
*/
HANDLE	renderThreadHandle;
int		renderThreadId;
qboolean GLimp_SpawnRenderThread( void (*function)( void ) ) {

	renderCommandsEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	renderCompletedEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	renderActiveEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	glimpRenderThread = function;

	renderThreadHandle = CreateThread(
	   NULL,	// LPSECURITY_ATTRIBUTES lpsa,
	   0,		// DWORD cbStack,
	   (LPTHREAD_START_ROUTINE)GLimp_RenderThreadWrapper,	// LPTHREAD_START_ROUTINE lpStartAddr,
	   0,			// LPVOID lpvThreadParm,
	   0,			//   DWORD fdwCreate,
	   (unsigned long *)&renderThreadId );

	if ( !renderThreadHandle ) {
		return qfalse;
	}

	return qtrue;
}

static	void	*smpData;
static	int		wglErrors;

void *GLimp_RendererSleep( void ) {
	void	*data;

	if ( !qwglMakeCurrent( glw_state.hDC, NULL ) ) {
		wglErrors++;
	}

	ResetEvent( renderActiveEvent );

	// after this, the front end can exit GLimp_FrontEndSleep
	SetEvent( renderCompletedEvent );

	WaitForSingleObject( renderCommandsEvent, INFINITE );

	if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) ) {
		wglErrors++;
	}

	ResetEvent( renderCompletedEvent );
	ResetEvent( renderCommandsEvent );

	data = smpData;

	// after this, the main thread can exit GLimp_WakeRenderer
	SetEvent( renderActiveEvent );

	return data;
}


void GLimp_FrontEndSleep( void ) {
	WaitForSingleObject( renderCompletedEvent, INFINITE );

	if ( !qwglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) ) {
		wglErrors++;
	}
}


void GLimp_WakeRenderer( void *data ) {
	smpData = data;

	if ( !qwglMakeCurrent( glw_state.hDC, NULL ) ) {
		wglErrors++;
	}

	// after this, the renderer can continue through GLimp_RendererSleep
	SetEvent( renderCommandsEvent );

	WaitForSingleObject( renderActiveEvent, INFINITE );
}

