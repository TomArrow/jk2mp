#include "tr_local.h"
#include "glext.h"
#include "tr_mme.h"

extern shotData_t shotData;
extern int superSampleMultiplier;

#ifndef DEDICATED
#if !defined __TR_WORLDEFFECTS_H
	#include "tr_WorldEffects.h"
#endif
#endif

backEndData_t	*backEndData[SMP_FRAMES];
backEndState_t	backEnd;

#ifdef JEDIACADEMY_GLOW
static void RB_DrawGlowOverlay();
static void RB_BlurGlowTexture();

// Whether we are currently rendering only glowing objects or not.
bool g_bRenderGlowingObjects = false;

// Whether the current hardware supports dynamic glows/flares.
bool g_bDynamicGlowSupported = false;

// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).
bool g_bTextureRectangleHack = false;
#endif

static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

#ifndef DEDICATED

/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
	int texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		image->frameUsed = tr.frameCount;
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
}

/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
	if ( glState.currenttmu == unit )
	{
		return;
	}

	if ( unit == 0 )
	{
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE0_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE0_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE0_ARB )\n" );
	}
	else if ( unit == 1 )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glActiveTextureARB( GL_TEXTURE1_ARB )\n" );
		qglClientActiveTextureARB( GL_TEXTURE1_ARB );
		GLimp_LogComment( "glClientActiveTextureARB( GL_TEXTURE1_ARB )\n" );
	} else {
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}

	glState.currenttmu = unit;
}


/*
** GL_BindMultitexture
*/
void GL_BindMultitexture( image_t *image0, GLuint env0, image_t *image1, GLuint env1 ) {
	int		texnum0, texnum1;

	texnum0 = image0->texnum;
	texnum1 = image1->texnum;

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum0 = texnum1 = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[1] != texnum1 ) {
		GL_SelectTexture( 1 );
		image1->frameUsed = tr.frameCount;
		glState.currenttextures[1] = texnum1;
		qglBindTexture( GL_TEXTURE_2D, texnum1 );
	}
	if ( glState.currenttextures[0] != texnum0 ) {
		GL_SelectTexture( 0 );
		image0->frameUsed = tr.frameCount;
		glState.currenttextures[0] = texnum0;
		qglBindTexture( GL_TEXTURE_2D, texnum0 );
	}
}


/*
** GL_Cull
*/
void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;
	if (backEnd.projection2D){	//don't care, we're in 2d when it's always disabled
		return;	
	}

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}

/*
** GL_TexEnv
*/
void GL_TexEnv( int env )
{
	if ( env == glState.texEnv[glState.currenttmu] )
	{
		return;
	}

	glState.texEnv[glState.currenttmu] = env;


	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned long stateBits )
{
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc(GL_LEQUAL);
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
	{
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits\n" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits\n" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		case GLS_ATEST_GE_C0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.75f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}



/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


static void SetFinalProjection( void ) {
	float	xmin, xmax, ymin, ymax;
	float	width, height, depth;
	float	zNear, zFar, zProj, stereoSep;
	float	dx, dy;
	vec2_t	pixelJitter, eyeJitter;
	
	//
	// set up projection matrix
	//
	zNear = r_znear->value;
	zFar = backEnd.viewParms.zFar;
	
	
	zProj	= r_zproj->value;
	stereoSep = r_stereoSeparation->value;

	ymax = zNear * tan( backEnd.viewParms.fovY * M_PI / 360.0f );
	ymin = -ymax;

	xmax = zNear * tan( backEnd.viewParms.fovX * M_PI / 360.0f );
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;
	depth = zFar - zNear;

	pixelJitter[0] = pixelJitter[1] = 0;
	eyeJitter[0] = eyeJitter[1] = 0;


	/* Jitter the view */
	if ( stereoSep <= 0.0f) {
		R_MME_JitterView( pixelJitter, eyeJitter );
	} else if ( stereoSep > 0.0f) {
		R_MME_JitterViewStereo( pixelJitter, eyeJitter );
	}

	dx = ( pixelJitter[0]*width ) / backEnd.viewParms.viewportWidth;
	dy = ( pixelJitter[1]*height ) / backEnd.viewParms.viewportHeight;
	dx += eyeJitter[0];
	dy += eyeJitter[1];

	if (r_fboFishEye->integer) {

		vec3_t jitterOrigin = { dx,dy,0 };
		float dofRadius = shotData.dofRadius, dofFocus = shotData.dofFocus;
		R_MME_ClampDof(&dofFocus, &dofRadius);
		R_FrameBuffer_ActivateFisheye(jitterOrigin, dofFocus, dofRadius, backEnd.viewParms.fovX, backEnd.viewParms.fovY);//Doesn't work. Needs fixing.
	}

	xmin += dx; xmax += dx;
	ymin += dy; ymax += dy;

	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
	qglGetFloatv(GL_PROJECTION_MATRIX, backEnd.viewParms.projectionMatrix );
	qglPopMatrix();

	backEnd.viewParms.projectionMatrix[0] = 2 * zNear / width;
	backEnd.viewParms.projectionMatrix[4] = 0;
	backEnd.viewParms.projectionMatrix[8] = ( xmax + xmin + 2 * stereoSep ) / width;	// normally 0
	backEnd.viewParms.projectionMatrix[12] = 2 * zProj * stereoSep / width;

	backEnd.viewParms.projectionMatrix[1] = 0;
	backEnd.viewParms.projectionMatrix[5] = 2 * zNear / height;
	backEnd.viewParms.projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	backEnd.viewParms.projectionMatrix[13] = 0;

	backEnd.viewParms.projectionMatrix[2] = 0;
	backEnd.viewParms.projectionMatrix[6] = 0;
	if (r_zinvert->integer) {
		backEnd.viewParms.projectionMatrix[10] = -(zNear) / depth;
		backEnd.viewParms.projectionMatrix[14] = -zFar * zNear / depth;
	}
	else {
		backEnd.viewParms.projectionMatrix[10] = -(zFar + zNear) / depth;
		backEnd.viewParms.projectionMatrix[14] = -2 * zFar * zNear / depth;
	}

	backEnd.viewParms.projectionMatrix[3] = 0;
	backEnd.viewParms.projectionMatrix[7] = 0;
	backEnd.viewParms.projectionMatrix[11] = -1;
	backEnd.viewParms.projectionMatrix[15] = 0;
}

void SetViewportAndScissor( void ) {
	qglMatrixMode(GL_PROJECTION);
	
	R_SetupFrustum();
	R_SetupProjection();
	SetFinalProjection();
	
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);


	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX * superSampleMultiplier, backEnd.viewParms.viewportY * superSampleMultiplier,
		backEnd.viewParms.viewportWidth* superSampleMultiplier, backEnd.viewParms.viewportHeight * superSampleMultiplier);
	qglScissor( backEnd.viewParms.viewportX * superSampleMultiplier, backEnd.viewParms.viewportY * superSampleMultiplier,
		backEnd.viewParms.viewportWidth * superSampleMultiplier, backEnd.viewParms.viewportHeight * superSampleMultiplier);
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_measureOverdraw->integer || r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	if (!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)
#ifdef JEDIACADEMY_GLOW
		&& !g_bRenderGlowingObjects
#endif
		) {
		if (mme_skykey->string[0] != '0') {
			vec3_t skyColor;
			clearBits |= GL_COLOR_BUFFER_BIT;
			Q_parseColor( mme_skykey->string, defaultColors, skyColor );
			qglClearColor( skyColor[0], skyColor[1], skyColor[2], 1.0f );
		} else if (r_fastsky->integer || r_fboFishEye->integer) {
			clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
			qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
			if (tr.mmeSkyColorIsSet) {
				qglClearColor(tr.mmeSkyColor[0], tr.mmeSkyColor[1], tr.mmeSkyColor[2], tr.mmeSkyColor[3]);	// FIXME: get color of sky
			}
			else {
				qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);	// FIXME: get color of sky
			}
			
#endif
		}
	}

#ifdef JEDIACADEMY_GLOW
	if ( !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) && r_DynamicGlow->integer && !g_bRenderGlowingObjects )
	{
		if (tr.world && tr.world->globalFog != -1)
		{ //this is because of a bug in multiple scenes I think, it needs to clear for the second scene but it doesn't normally.
			const fog_t		*fog = &tr.world->fogs[tr.world->globalFog];

			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor(fog->parms.color[0],  fog->parms.color[1], fog->parms.color[2], 1.0f );
		}
	}

	// If this pass is to just render the glowing objects, don't clear the depth buffer since
	// we're sharing it with the main scene (since the main scene has already been rendered). -AReis
	if ( g_bRenderGlowingObjects )
	{
		clearBits &= ~GL_DEPTH_BUFFER_BIT;
	}
#endif

	qglClear( clearBits );

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	// clip to the plane of the portal
	if ( backEnd.viewParms.isPortal ) {
		float	plane[4];
		double	plane2[4];

		plane[0] = backEnd.viewParms.portalPlane.normal[0];
		plane[1] = backEnd.viewParms.portalPlane.normal[1];
		plane[2] = backEnd.viewParms.portalPlane.normal[2];
		plane[3] = backEnd.viewParms.portalPlane.dist;

		plane2[0] = DotProduct (backEnd.viewParms.ori.axis[0], plane);
		plane2[1] = DotProduct (backEnd.viewParms.ori.axis[1], plane);
		plane2[2] = DotProduct (backEnd.viewParms.ori.axis[2], plane);
		plane2[3] = DotProduct (plane, backEnd.viewParms.ori.origin) - plane[3];

		qglLoadMatrixf( s_flipMatrix );
		qglClipPlane (GL_CLIP_PLANE0, plane2);
		qglEnable (GL_CLIP_PLANE0);
	} else {
		qglDisable (GL_CLIP_PLANE0);
	}
}


#define	MAC_EVENT_PUMP_MSEC		5

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int64_t			fogNum, oldFogNum;
	int64_t			entityNum, oldEntityNum;
	int64_t			dlighted, oldDlighted;
	int				depthRange, oldDepthRange;
	int				i;
	drawSurf_t		*drawSurf;
	uint64_t		oldSort;
	double			originalTime;
#ifdef JEDIACADEMY_GLOW
	bool			didShadowPass = false;
#endif
#ifdef __MACOS__
	int				macEventTime;

	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size

	// we don't want to pump the event loop too often and waste time, so
	// we are going to check every shader change
	macEventTime = ri.Milliseconds()*ri.Cvar_VariableValue( "timescale" ) + MAC_EVENT_PUMP_MSEC;
#endif

#ifdef JEDIACADEMY_GLOW
	if (g_bRenderGlowingObjects) {
	//only shadow on initial passes
		didShadowPass = true;
	}
#endif

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = (uint64_t) -1;
	depthRange = qfalse;
	
	// Clear for endsurface first run
	tess.numIndexes = 0;

	backEnd.pc.c_surfaces += numDrawSurfs;
	if (!(backEnd.refdef.rdflags & RDF_NOWORLDMODEL)) {
		backEnd.sceneZfar = backEnd.viewParms.zFar;
	}

	for (i = 0, drawSurf = drawSurfs; i < numDrawSurfs && drawSurf; i++, drawSurf++) {
/*		if (!drawSurf->surface)
			continue;
		if (!*drawSurf->surface)
			continue;
		if (*drawSurf->surface < SF_BAD || *drawSurf->surface >= SF_NUM_SURFACE_TYPES)
			continue;
*/		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
#ifdef JEDIACADEMY_GLOW
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
		// If we're rendering glowing objects, but this shader has no stages with glow, skip it!
		if ( g_bRenderGlowingObjects && !shader->hasGlow ) {
			shader = oldShader;
			entityNum = oldEntityNum;
			fogNum = oldFogNum;
			dlighted = oldDlighted;
			continue;
		}
		oldSort = drawSurf->sort;
#else
		oldSort = drawSurf->sort;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
#endif

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from seperate
		// entities merged into a single batch, like smoke and blood puff sprites
		if (shader != oldShader || fogNum != oldFogNum || dlighted != oldDlighted 
			|| ( entityNum != oldEntityNum && !shader->entityMergable ) ) {
			if (oldShader != NULL) {
#ifdef __MACOS__	// crutch up the mac's limited buffer queue size
				int		t;

				t = ri.Milliseconds()*ri.Cvar_VariableValue( "timescale" );
				if ( t > macEventTime ) {
					macEventTime = t + MAC_EVENT_PUMP_MSEC;
					Sys_PumpEvents();
				}
#endif
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );

			oldShader = shader;
			oldFogNum = fogNum;
			oldDlighted = dlighted;
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = 0;

//			if ( entityNum != ENTITYNUM_WORLD ) {
			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				backEnd.refdef.floatTime = originalTime - backEnd.currentEntity->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.ori );

				// set up the dynamic lighting if needed
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
				}

				if ( backEnd.currentEntity->e.renderfx & RF_NODEPTH ) {
					// No depth at all, very rare but some things for seeing through walls
					depthRange = 2;
				}
				else if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = 1;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.ori = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.ori );
			}

			qglLoadMatrixf( backEnd.ori.modelMatrix ); 

			//
			// change depthrange if needed
			//
			if ( oldDepthRange != depthRange ) {
				switch ( depthRange ) {
					default:
					case 0:
						qglDepthRange (0, 1);	
						break;

					case 1:
						qglDepthRange (0, .3);
						break;

					case 2:
						qglDepthRange(0, 0);
						break;
				}

				oldDepthRange = depthRange;
			}

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
	}

	backEnd.refdef.floatTime = originalTime;

	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}

#if 0
	RB_DrawSun();
#endif
#ifdef JEDIACADEMY_GLOW
	if (!didShadowPass) {
		// darken down any stencil shadows
		RB_ShadowFinish();
		didShadowPass = true;
	}
#endif

	// add light flares on lights that aren't obscured

	// rww - 9-13-01 [1-26-01-sof2]
//	RB_RenderFlares();

#ifdef __MACOS__
	Sys_PumpEvents();		// crutch up the mac's limited buffer queue size
#endif
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D (void) {
	backEnd.projection2D = qtrue;

	static float zInvertedIdentity[] = {1,0,0,0,
										0,1,0,0,
										0,0,-1,0,
										0,0,0,1,};

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth* superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);
	qglScissor( 0, 0, glConfig.vidWidth* superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	qglOrtho (0, 640, 480, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	GL_State( GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	qglDisable( GL_CULL_FACE );
	qglDisable( GL_CLIP_PLANE0 );

	R_FrameBuffer_DeactivateFisheye();

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001;
}


/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) 
{
	int			start, end;

	if ( !tr.registered ) {
		return;
	}
	R_SyncRenderThread();

	// we definately want to sync every frame for the cinematics
	qglFinish();

	start = end = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	// make sure rows and cols are powers of 2
	if ( (cols&(cols-1)) || (rows&(rows-1)) )
	{
		ri.Error (ERR_DROP, "Draw_StretchRaw: size not a power of 2: %i by %i", cols, rows);
	}

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, r_gammaSrgbTextures->integer ? GL_SRGB8 : GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		if (mme_cinNoClamp->integer) {

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		else {

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "qglTexSubImage2D %i, %i: %i msec\n", cols, rows, end - start );
	}

	RB_SetGL2D();

	qglColor3f( tr.identityLight, tr.identityLight, tr.identityLight );

	qglBegin (GL_QUADS);
	qglTexCoord2f ( 0.5f / cols,  0.5f / rows );
	qglVertex2f (x, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols ,  0.5f / rows );
	qglVertex2f (x+w, y);
	qglTexCoord2f ( ( cols - 0.5f ) / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x+w, y+h);
	qglTexCoord2f ( 0.5f / cols, ( rows - 0.5f ) / rows );
	qglVertex2f (x, y+h);
	qglEnd ();
}

void RE_UploadCinematic (int cols, int rows, const byte *data, int client, qboolean dirty) {

	GL_Bind( tr.scratchImage[client] );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != tr.scratchImage[client]->width || rows != tr.scratchImage[client]->height ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D(GL_TEXTURE_2D, 0, r_gammaSrgbTextures->integer ? GL_SRGB8 : GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		if (mme_cinNoClamp->integer) {

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		else {

			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		}
	} else {
		if (dirty) {
			// otherwise, just subimage upload it so that drivers can tell we are going to be changing
			// it and don't try and do a texture compression
			qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
		}
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255.0f;
	backEnd.color2D[1] = cmd->color[1] * 255.0f;
	backEnd.color2D[2] = cmd->color[2] * 255.0f;
	backEnd.color2D[3] = cmd->color[3] * 255.0f;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
const void *RB_StretchPic ( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;
	int		numVerts, numIndexes;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

	RB_CHECKOVERFLOW( 4, 6 );
	numVerts = tess.numVertexes;
	numIndexes = tess.numIndexes;

	tess.numVertexes += 4;
	tess.numIndexes += 6;

	tess.indexes[ numIndexes ] = numVerts + 3;
	tess.indexes[ numIndexes + 1 ] = numVerts + 0;
	tess.indexes[ numIndexes + 2 ] = numVerts + 2;
	tess.indexes[ numIndexes + 3 ] = numVerts + 2;
	tess.indexes[ numIndexes + 4 ] = numVerts + 0;
	tess.indexes[ numIndexes + 5 ] = numVerts + 1;

	Vector4Copy(backEnd.color2D, tess.vertexColors[numVerts]);
	Vector4Copy(backEnd.color2D, tess.vertexColors[numVerts+1]);
	Vector4Copy(backEnd.color2D, tess.vertexColors[numVerts+2]);
	Vector4Copy(backEnd.color2D, tess.vertexColors[numVerts+3]);

	//Debug
	/*static vec4_t tmp{255,255,255,255};
	Vector4Copy(tmp, tess.vertexColors[numVerts]);
	Vector4Copy(tmp, tess.vertexColors[numVerts + 1]);
	Vector4Copy(tmp, tess.vertexColors[numVerts + 2]);
	Vector4Copy(tmp, tess.vertexColors[numVerts + 3]);*/
	/**(int *)tess.vertexColors[ numVerts ] =
		*(int *)tess.vertexColors[ numVerts + 1 ] =
		*(int *)tess.vertexColors[ numVerts + 2 ] =
		*(int *)tess.vertexColors[ numVerts + 3 ] = *(int *)backEnd.color2D;*/

	tess.xyz[ numVerts ][0] = cmd->x;
	tess.xyz[ numVerts ][1] = cmd->y;
	tess.xyz[ numVerts ][2] = 0; // Setting these to -1 makes it work with the inverted z buffer with the default RB_SetGL2D

	tess.texCoords[ numVerts ][0][0] = cmd->s1;
	tess.texCoords[ numVerts ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 1 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 1 ][1] = cmd->y;
	tess.xyz[ numVerts + 1 ][2] = 0;

	tess.texCoords[ numVerts + 1 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 1 ][0][1] = cmd->t1;

	tess.xyz[ numVerts + 2 ][0] = cmd->x + cmd->w;
	tess.xyz[ numVerts + 2 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 2 ][2] = 0;

	tess.texCoords[ numVerts + 2 ][0][0] = cmd->s2;
	tess.texCoords[ numVerts + 2 ][0][1] = cmd->t2;

	tess.xyz[ numVerts + 3 ][0] = cmd->x;
	tess.xyz[ numVerts + 3 ][1] = cmd->y + cmd->h;
	tess.xyz[ numVerts + 3 ][2] = 0;

	tess.texCoords[ numVerts + 3 ][0][0] = cmd->s1;
	tess.texCoords[ numVerts + 3 ][0][1] = cmd->t2;

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawRotatePic
=============
*/
const void *RB_RotatePic ( const void *data ) 
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;
	image = shader->stages[0]->bundle[0].image[0];

	if ( image ) {
		if ( !backEnd.projection2D ) {
			RB_SetGL2D();
		}

		//qglColor4ubv( backEnd.color2D );
		qglColor4f( backEnd.color2D[0]/255.0f, backEnd.color2D[1] / 255.0f, backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f);
		qglPushMatrix();

		qglTranslatef(cmd->x+cmd->w,cmd->y,0);
		qglScalef((640.0*glConfig.vidHeight)/(480.0*glConfig.vidWidth), 1.0, 1.0); 
		qglRotatef(cmd->a, 0.0, 0.0, 1.0);
		
		GL_Bind( image );
		qglBegin (GL_QUADS);
		qglTexCoord2f( cmd->s1, cmd->t1);
		qglVertex2f( -cmd->w, 0 );
		qglTexCoord2f( cmd->s2, cmd->t1 );
		qglVertex2f( 0, 0 );
		qglTexCoord2f( cmd->s2, cmd->t2 );
		qglVertex2f( 0, cmd->h );
		qglTexCoord2f( cmd->s1, cmd->t2 );
		qglVertex2f( -cmd->w, cmd->h );
		qglEnd();
		
		qglPopMatrix();
	}

	return (const void *)(cmd + 1);
}

/*
=============
RB_DrawRotatePic2
=============
*/
const void *RB_RotatePic2 ( const void *data ) 
{
	const rotatePicCommand_t	*cmd;
	image_t *image;
	shader_t *shader;

	cmd = (const rotatePicCommand_t *)data;

	shader = cmd->shader;

	if ( shader->stages[0] )
	{
		image = shader->stages[0]->bundle[0].image[0];

		if ( image ) {
			if ( !backEnd.projection2D ) {
				RB_SetGL2D();
			}

			// Get our current blend mode, etc.
			GL_State( shader->stages[0]->stateBits );

			//qglColor4ubv( backEnd.color2D );
			qglColor4f(backEnd.color2D[0] / 255.0f, backEnd.color2D[1] / 255.0f, backEnd.color2D[2] / 255.0f, backEnd.color2D[3] / 255.0f);
			qglPushMatrix();

			// rotation point is going to be around the center of the passed in coordinates
			qglTranslatef( cmd->x, cmd->y, 0 );
			qglRotatef( cmd->a, 0.0, 0.0, 1.0 );
		
			GL_Bind( image );
			qglBegin( GL_QUADS );
				qglTexCoord2f( cmd->s1, cmd->t1);
				qglVertex2f( -cmd->w * 0.5f, -cmd->h * 0.5f );

				qglTexCoord2f( cmd->s2, cmd->t1 );
				qglVertex2f( cmd->w * 0.5f, -cmd->h * 0.5f );

				qglTexCoord2f( cmd->s2, cmd->t2 );
				qglVertex2f( cmd->w * 0.5f, cmd->h * 0.5f );

				qglTexCoord2f( cmd->s1, cmd->t2 );
				qglVertex2f( -cmd->w * 0.5f, cmd->h * 0.5f );
			qglEnd();
		
			qglPopMatrix();

			// Hmmm, this is not too cool
			GL_State( GLS_DEPTHTEST_DISABLE |
				  GLS_SRCBLEND_SRC_ALPHA |
				  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
		}
	}

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;
	//Jitter the camera origin
	if ( !backEnd.viewParms.isPortal && !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		float x, y;
		if ( (r_stereoSeparation->value <= 0 && R_MME_JitterOrigin( &x, &y ))
			|| (r_stereoSeparation->value > 0 && R_MME_JitterOriginStereo( &x, &y ))) {
			orientationr_t* wor = &backEnd.viewParms.ori;
			orientationr_t* world = &backEnd.viewParms.world;

			VectorMA( wor->origin, x, wor->axis[1], wor->origin );
			VectorMA( wor->origin, y, wor->axis[2], wor->origin );
			R_RotateForWorld( wor, world );
		}
	}
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

#ifdef JEDIACADEMY_GLOW
	// Dynamic Glow/Flares:
	/*
		The basic idea is to render the glowing parts of the scene to an offscreen buffer, then take
		that buffer and blur it. After it is sufficiently blurred, re-apply that image back to
		the normal screen using a additive blending. To blur the scene I use a vertex program to supply
		four texture coordinate offsets that allow 'peeking' into adjacent pixels. In the register
		combiner (pixel shader), I combine the adjacent pixels using a weighting factor. - Aurelio
	*/

	// Render dynamic glowing/flaring objects.
	if ( !(backEnd.refdef.rdflags & RDF_NOWORLDMODEL) && g_bDynamicGlowSupported && r_DynamicGlow->integer )
	{
		// Copy the normal scene to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage ); 
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );    

		// Just clear colors, but leave the depth buffer intact so we can 'share' it.
		qglClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		qglClear( GL_COLOR_BUFFER_BIT ); 

		// Render the glowing objects.
		g_bRenderGlowingObjects = true;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );  
		g_bRenderGlowingObjects = false;

		qglFinish();

		// Copy the glow scene to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.screenGlow ); 
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, glConfig.vidWidth, glConfig.vidHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
		
		// Resize the viewport to the blur texture size.
		const int oldViewWidth = backEnd.viewParms.viewportWidth;
		const int oldViewHeight = backEnd.viewParms.viewportHeight;
		backEnd.viewParms.viewportWidth = r_DynamicGlowWidth->integer;
		backEnd.viewParms.viewportHeight = r_DynamicGlowHeight->integer;
		SetViewportAndScissor();

		// Blur the scene.
		RB_BlurGlowTexture();

		// Copy the finished glow scene back to texture.
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
		
		// Set the viewport back to normal.
		backEnd.viewParms.viewportWidth = oldViewWidth;
		backEnd.viewParms.viewportHeight = oldViewHeight;
		SetViewportAndScissor();
		qglClear( GL_COLOR_BUFFER_BIT ); 

		// Draw the glow additively over the screen.
		RB_DrawGlowOverlay(); 
	}
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

#ifndef HAVE_GLES
	qglDrawBuffer(cmd->buffer);
#endif
#ifdef CAPTURE_FLOAT
	R_FrameBuffer_StartFrame();
#endif
	// clear screen for debugging
	if (tr.world && tr.world->globalFog != -1)
	{
		unsigned	i = tr.world->fogs[tr.world->globalFog].colorInt;

		qglClearColor( ( (byte *)&i )[0] / 255.0, ( (byte *)&i )[1] / 255.0, ( (byte *)&i )[2] / 255.0,  1.0 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
	else if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	image_t	*image;
	float	x, y, w, h;
	int		start, end;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();


	int i=0;
	   				 R_Images_StartIteration();
	while ( (image = R_Images_GetNextIteration()) != NULL)
	{
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0;
			h *= image->uploadHeight / 512.0;
		}

		GL_Bind( image );
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
		i++;
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );

}

/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	if ( tess.numIndexes ) {
		RB_EndSurface();
	}

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	RB_RenderWorldEffects();

	cmd = (const swapBuffersCommand_t *)data;
	
	backEnd.projection2D = qfalse;

	tr.capturingDofOrStereo = qfalse;
	tr.latestDofOrStereoFrame = qfalse;

	R_FrameBuffer_ApplyExposure();

	/* Take and merge DOF frames */
	if ( r_stereoSeparation->value <= 0.0f && !tr.finishStereo) {
		if ( R_MME_MultiPassNext() ) {
			return (const void *)NULL;
		}
	} else if ( r_stereoSeparation->value > 0.0f) {
		if ( R_MME_MultiPassNextStereo() ) {
			return (const void *)NULL;
		}
	}
	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = (unsigned char *)ri.Hunk_AllocateTempMemory( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i];
		}

		backEnd.pc.c_overDraw += sum;
		ri.Hunk_FreeTempMemory( stencilReadback );
	}

	/* Allow MME to take a screenshot */
	qboolean shotDataTakeTmp = shotData.take;
	qboolean trFinishStereoTmp = tr.finishStereo;
	if ( r_stereoSeparation->value < 0.0f && tr.finishStereo) {
		tr.capturingDofOrStereo = qtrue;
		tr.latestDofOrStereoFrame = qtrue;
		Cvar_SetValue("r_stereoSeparation", -r_stereoSeparation->value);
		return (const void *)NULL;
	} else if ( r_stereoSeparation->value <= 0.0f) {
		if ( R_MME_TakeShot( ) && r_stereoSeparation->value != 0.0f) {
			tr.capturingDofOrStereo = qtrue;
			tr.latestDofOrStereoFrame = qfalse;
			Cvar_SetValue("r_stereoSeparation", -r_stereoSeparation->value);
			tr.finishStereo = qtrue;
			return (const void *)NULL;
		}
	} else if ( r_stereoSeparation->value > 0.0f) {
		if ( tr.finishStereo) {
			R_MME_TakeShotStereo( );
			R_MME_DoNotTake( );
			Cvar_SetValue("r_stereoSeparation", -r_stereoSeparation->value);
			tr.finishStereo = qfalse;
		}
	}
	tr.captureIsActive = (qboolean)(shotDataTakeTmp || trFinishStereoTmp || tr.capturingDofOrStereo); // Ent probably wouldn't like this, just my way of hacking things together.


    if ( !glState.finishCalled ) {
        qglFinish();
	}
#ifdef CAPTURE_FLOAT
	R_FrameBuffer_EndFrame();
#endif
    GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

    GLimp_EndFrame();

	return (const void *)(cmd + 1);
}


/*
====================
RB_ExecuteRenderCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *oldData ) {
	int		t1, t2;
	const void* data;

	t1 = ri.Milliseconds ();

	if ( !r_smp->integer || oldData == backEndData[0]->commands.cmds ) {
		backEnd.smpFrame = 0;
	} else {
		backEnd.smpFrame = 1;
	}
again:
	data = oldData;
	while ( 1 ) {
		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_ROTATE_PIC:
			data = RB_RotatePic( data );
			break;
		case RC_ROTATE_PIC2:
			data = RB_RotatePic2( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			if ( (int)data == 0 )
				goto again;
			break;
		case RC_SCREENSHOT:
			data = RB_ScreenShotCmd( data );
			break;
		case RC_CAPTURE:
			data = R_MME_CaptureShotCmd( data );
			break;
		case RC_CAPTURE_STEREO:
			data = R_MME_CaptureShotCmdStereo( data );
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}


/*
================
RB_RenderThread
================
*/
void RB_RenderThread( void ) {
	const void	*data;

	// wait for either a rendering command or a quit command
	while ( 1 ) {
		// sleep until we have work to do
		data = GLimp_RendererSleep();

		if ( !data ) {
			return;	// all done, renderer is shutting down
		}

		renderThreadActive = qtrue;

		RB_ExecuteRenderCommands( data );

		renderThreadActive = qfalse;
	}
}

#endif //!DEDICATED


// What Pixel Shader type is currently active (regcoms or fragment programs).
GLuint g_uiCurrentPixelShaderType = 0x0;

// Begin using a Pixel Shader.
void BeginPixelShader( GLuint uiType, GLuint uiID )
{
	switch ( uiType )
	{
		// Using Register Combiners, so call the Display List that stores it.
		case GL_REGISTER_COMBINERS_NV:
		{
			// Just in case...
			if ( !qglCombinerParameterfvNV )
				return;

			// Call the list with the regcom in it.
			qglEnable( GL_REGISTER_COMBINERS_NV );
			qglCallList( uiID );

			g_uiCurrentPixelShaderType = GL_REGISTER_COMBINERS_NV;
		}
		return;

		// Using Fragment Programs, so call the program.
		case GL_FRAGMENT_PROGRAM_ARB:
		{
			// Just in case...
			if ( !qglGenProgramsARB )
				return;

			qglEnable( GL_FRAGMENT_PROGRAM_ARB );
			qglBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, uiID );

			g_uiCurrentPixelShaderType = GL_FRAGMENT_PROGRAM_ARB;
		}
		return;
	}
}

// Stop using a Pixel Shader and return states to normal.
void EndPixelShader()
{
	if ( g_uiCurrentPixelShaderType == 0x0 )
		return;

	qglDisable( g_uiCurrentPixelShaderType );
}

#ifdef JEDIACADEMY_GLOW
// Hack variable for deciding which kind of texture rectangle thing to do (for some
// reason it acts different on radeon! It's against the spec!).

static inline void RB_BlurGlowTexture()
{
	qglDisable (GL_CLIP_PLANE0);
	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST );

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	/////////////////////////////////////////////////////////
	// Setup vertex and pixel programs.
	/////////////////////////////////////////////////////////

	// NOTE: The 0.25 is because we're blending 4 textures (so = 1.0) and we want a relatively normalized pixel
	// intensity distribution, but this won't happen anyways if intensity is higher than 1.0.
	float fBlurDistribution = r_DynamicGlowIntensity->value * 0.25f;
	float fBlurWeight[4] = { fBlurDistribution, fBlurDistribution, fBlurDistribution, 1.0f };

	// Enable and set the Vertex Program.
	qglEnable( GL_VERTEX_PROGRAM_ARB );
	qglBindProgramARB( GL_VERTEX_PROGRAM_ARB, tr.glowVShader );

	// Apply Pixel Shaders.
	if ( qglCombinerParameterfvNV )
	{
		BeginPixelShader( GL_REGISTER_COMBINERS_NV, tr.glowPShader );

		// Pass the blur weight to the regcom.
		qglCombinerParameterfvNV( GL_CONSTANT_COLOR0_NV, (float*)&fBlurWeight );
	}
	else if ( qglProgramEnvParameter4fARB )
	{
		BeginPixelShader( GL_FRAGMENT_PROGRAM_ARB, tr.glowPShader );

		// Pass the blur weight to the Fragment Program.
		qglProgramEnvParameter4fARB( GL_FRAGMENT_PROGRAM_ARB, 0, fBlurWeight[0], fBlurWeight[1], fBlurWeight[2], fBlurWeight[3] );
	}

	/////////////////////////////////////////////////////////
	// Set the blur texture to the 4 texture stages.
	/////////////////////////////////////////////////////////

	// How much to offset each texel by.
	float fTexelWidthOffset = 0.1f, fTexelHeightOffset = 0.1f;

	GLuint uiTex = tr.screenGlow;  

	qglActiveTextureARB( GL_TEXTURE3_ARB );  
	qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
	
	qglActiveTextureARB( GL_TEXTURE2_ARB ); 
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_2D );  
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex ); 
	
	/////////////////////////////////////////////////////////
	// Draw the blur passes (each pass blurs it more, increasing the blur radius ).
	/////////////////////////////////////////////////////////
	
	//int iTexWidth = backEnd.viewParms.viewportWidth, iTexHeight = backEnd.viewParms.viewportHeight;
	int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight; 
	
	for ( int iNumBlurPasses = 0; iNumBlurPasses < r_DynamicGlowPasses->integer; iNumBlurPasses++ )       
	{
		// Load the Texel Offsets into the Vertex Program.
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 0, -fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 1, -fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 2, fTexelWidthOffset, -fTexelWidthOffset, 0.0f, 0.0f );
		qglProgramEnvParameter4fARB( GL_VERTEX_PROGRAM_ARB, 3, fTexelWidthOffset, fTexelWidthOffset, 0.0f, 0.0f );

		// After first pass put the tex coords to the viewport size.
		if ( iNumBlurPasses == 1 )
		{
			// OK, very weird, but dependent on which texture rectangle extension we're using, the
			// texture either needs to be always texure correct or view correct...
			if ( !g_bTextureRectangleHack ) 
			{
				iTexWidth = backEnd.viewParms.viewportWidth;
				iTexHeight = backEnd.viewParms.viewportHeight;
			}

			uiTex = tr.blurImage;
			qglActiveTextureARB( GL_TEXTURE3_ARB );  
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT ); 
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
			qglActiveTextureARB( GL_TEXTURE2_ARB ); 
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );			
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );
			qglActiveTextureARB(GL_TEXTURE0_ARB );
			qglDisable( GL_TEXTURE_2D );
			qglEnable( GL_TEXTURE_RECTANGLE_EXT );
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex ); 

			// Copy the current image over.
			qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, uiTex );     
			qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
		}

		// Draw the fullscreen quad.
		qglBegin( GL_QUADS ); 
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, iTexHeight );  
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, 0 ); 
			qglVertex2f( backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, iTexWidth, iTexHeight );
			qglVertex2f( backEnd.viewParms.viewportWidth, 0 ); 
		qglEnd();

		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );       
		qglCopyTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, 0, 0, backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );    

		// Increase the texel offsets.
		// NOTE: This is possibly the most important input to the effect. Even by using an exponential function I've been able to
		// make it look better (at a much higher cost of course). This is cheap though and still looks pretty great. In the future 
		// I might want to use an actual gaussian equation to correctly calculate the pixel coefficients and attenuates, texel
		// offsets, gaussian amplitude and radius...
		fTexelWidthOffset += r_DynamicGlowDelta->value;
		fTexelHeightOffset += r_DynamicGlowDelta->value;
	}

	// Disable multi-texturing.
	qglActiveTextureARB( GL_TEXTURE3_ARB );   
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB( GL_TEXTURE2_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB( GL_TEXTURE1_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );

	qglActiveTextureARB(GL_TEXTURE0_ARB );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );
	qglEnable( GL_TEXTURE_2D );

	qglDisable( GL_VERTEX_PROGRAM_ARB );
	EndPixelShader();
	
	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglDisable( GL_BLEND );
	qglEnable( GL_DEPTH_TEST );

	glState.currenttmu = 0;	//this matches the last one we activated
}

// Draw the glow blur over the screen additively.
static inline void RB_DrawGlowOverlay()
{
	qglDisable (GL_CLIP_PLANE0);
	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_DEPTH_TEST ); 

	// Go into orthographic 2d mode.
	qglMatrixMode(GL_PROJECTION);
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho(0, glConfig.vidWidth, glConfig.vidHeight, 0, -1, 1);
	qglMatrixMode(GL_MODELVIEW);
	qglPushMatrix();
	qglLoadIdentity();

	GL_State(0);

	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );

	// For debug purposes.
	if ( r_DynamicGlow->integer != 2 )
	{
		// Render the normal scene texture.
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage ); 
		qglBegin(GL_QUADS);    
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglTexCoord2f( 0, glConfig.vidHeight ); 
			qglVertex2f( 0, 0 );

			qglTexCoord2f( 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, 0 );
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglTexCoord2f( glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 );
		qglEnd();
	}

	// One and Inverse Src Color give a very soft addition, while one one is a bit stronger. With one one we can
	// use additive blending through multitexture though.
	if ( r_DynamicGlowSoft->integer )
	{
		qglBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_COLOR );
	}
	else
	{
		qglBlendFunc( GL_ONE, GL_ONE );
	}
	qglEnable( GL_BLEND );  

	// Now additively render the glow texture.
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );     
	qglBegin(GL_QUADS);    
		qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );  
		qglTexCoord2f( 0, r_DynamicGlowHeight->integer ); 
		qglVertex2f( 0, 0 );

		qglTexCoord2f( 0, 0 );
		qglVertex2f( 0, glConfig.vidHeight );

		qglTexCoord2f( r_DynamicGlowWidth->integer, 0 );
		qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

		qglTexCoord2f( r_DynamicGlowWidth->integer, r_DynamicGlowHeight->integer );
		qglVertex2f( glConfig.vidWidth, 0 );
	qglEnd();

	qglDisable( GL_TEXTURE_RECTANGLE_EXT );
	qglEnable( GL_TEXTURE_2D );
	qglBlendFunc( GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR );
	qglDisable( GL_BLEND );

	// NOTE: Multi-texture wasn't that much faster (we're obviously not bottlenecked by transform pipeline),
	// and besides, soft glow looks better anyways.
/*	else
	{
		int iTexWidth = glConfig.vidWidth, iTexHeight = glConfig.vidHeight;
		if ( GL_TEXTURE_RECTANGLE_EXT == GL_TEXTURE_RECTANGLE_NV ) 
		{
			iTexWidth = r_DynamicGlowWidth->integer;
			iTexHeight = r_DynamicGlowHeight->integer;
		}

		qglActiveTextureARB( GL_TEXTURE1_ARB ); 
		qglDisable( GL_TEXTURE_2D ); 
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.screenGlow ); 
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );  

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglDisable( GL_TEXTURE_2D );
		qglEnable( GL_TEXTURE_RECTANGLE_EXT );
		qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );

		qglBegin(GL_QUADS);    
			qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, iTexHeight );  
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, glConfig.vidHeight );  
			qglVertex2f( 0, 0 );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, 0, 0 );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, 0, 0 );
			qglVertex2f( 0, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, 0 ); 
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, 0 ); 
			qglVertex2f( glConfig.vidWidth, glConfig.vidHeight );

			qglMultiTexCoord2fARB( GL_TEXTURE1_ARB, iTexWidth, iTexHeight );
			qglMultiTexCoord2fARB( GL_TEXTURE0_ARB, glConfig.vidWidth, glConfig.vidHeight );
			qglVertex2f( glConfig.vidWidth, 0 ); 
		qglEnd();

		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );

		qglActiveTextureARB(GL_TEXTURE0_ARB );
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE ); 
		qglDisable( GL_TEXTURE_RECTANGLE_EXT );
		qglEnable( GL_TEXTURE_2D );
	}*/

	qglMatrixMode(GL_PROJECTION);
	qglPopMatrix();
	qglMatrixMode(GL_MODELVIEW);
	qglPopMatrix();

	qglEnable( GL_DEPTH_TEST );
}
#endif
