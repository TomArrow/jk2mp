/*
 *      tr_framebuffer.c
 *      
 *      Copyright 2007 Gord Allott <gordallott@gmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
*/
// tr_framebuffer.c: framebuffer object rendering path code
// Okay i am going to try and document what I doing here, appologies to anyone 
// that already understands this. basically the idea is that normally everything
// opengl renders will be rendered into client memory, that is the space the 
// graphics card reserves for anything thats going to be sent to the monitor.
// Using this method we instead redirect all the rendering to a seperate bit of 
// memory called a frame buffer. 
// we can then bind this framebuffer to a texture and render that texture to the
// client memory again so that the image will be sent to the monitor. this 
// redirection allows for some neat effects to be applied.

// Some ideas for what to use this path for:
//		- Bloom	-done
//		- Rotoscope cartoon effects (edge detect + colour mapping)
//		- Fake anti-aliasing. (edge detect and blur positive matches)
//		- Motion blur
//			- generate a speed vector based on how the camera has moved since 
//			  the last frame and use that to compute per pixel blur vectors 
//		- These would require mods to use some sort of framebuffer qvm api
//			- special effects for certain powerups 
//			- Image Blur when the player is hit
//			- Depth of field blur

//Sjoerd
//Butchered this up a bit for MME needs and only really has been tested for my ATI videocard

//#pragma once

#include "tr_local.h"
#include "tr_glsl.h"

#include <vector>

#ifdef CAPTURE_FLOAT
#include <cmath>
#endif

#ifdef _WIN32
#include "qgl.h"
#endif

void R_FrameBuffer_CreateRollingShutterBuffers(int width, int height, int flags);

cvar_t *r_convertToHDR;
cvar_t *r_floatBuffer;
cvar_t *r_fbo;
cvar_t *r_fboExposure;
cvar_t *r_fboCompensateSkyTint;
cvar_t *r_fboSuperSample;
cvar_t *r_fboSuperSampleMipMap;
cvar_t *r_fboDepthBits;
cvar_t *r_fboDepthPacked;
cvar_t *r_fboStencilWhenNotPacked;
cvar_t *r_fboMultiSample;
cvar_t *r_fboBlur;
cvar_t *r_fboWidth;
cvar_t *r_fboHeight;
cvar_t *r_glDepthClamp;

qboolean mipMapsAlreadyGeneratedThisFrame = qfalse;

// This is a bit unstable/glitchy I think. But inside the game it seems to work. Consider experimental.
// Maybe also needs stepwise downscaling for values over 2? because GL_LINEAR takes only closest 4 pixels into account?
// Or figure how that mipmap stuff works?
int superSampleMultiplier =1; // outside of this file, only READ this. 


#define GL_DEPTH_STENCIL_EXT					0x84F9
#define GL_UNSIGNED_INT_24_8_EXT				0x84FA
#define GL_DEPTH24_STENCIL8_EXT					0x88F0
#define GL_DEPTH32_STENCIL8						0x8CAD
#define GL_DEPTH_COMPONENT24					0x81A6
#define GL_DEPTH_COMPONENT32					0x81A7
#define GL_DEPTH_COMPONENT32F					0x8CAC
#define GL_DEPTH32F_STENCIL8					0x8CAD // Oh.. same value as GL_DEPTH32_STENCIL8! hmm..
#define GL_DEPTH_COMPONENT32F_NV				0x8DAB
#define GL_DEPTH32F_STENCIL8_NV					0x8DAC
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV		0x8DAD
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV	0x8DAD
#define GL_DEPTH_CLAMP							0x864F

#define RGBA32F_ARB                      0x8814
#define RGBA16F_ARB                      0x881A

#ifndef GL_DEPTH_STENCIL_EXT
#define GL_DEPTH_STENCIL_EXT GL_DEPTH_STENCIL_NV
#endif

#ifndef GL_UNSIGNED_INT_24_8_EXT
#define GL_UNSIGNED_INT_24_8_EXT GL_UNSIGNED_INT_24_8_NV
#endif

//typedef frameBufferData_t* doubleFrameBufferData_t[2];
typedef struct {
	frameBufferData_t* current;
	frameBufferData_t* next;
} doubleFrameBufferData_t;

static struct {
	frameBufferData_t *multiSample;
	frameBufferData_t *main;
	frameBufferData_t *exposure;
	frameBufferData_t *blur;
	frameBufferData_t *dof;
	frameBufferData_t *colorSpaceConv;
	frameBufferData_t *colorSpaceConvResult;
	std::vector<doubleFrameBufferData_t> rollingShutterBuffers;
	int screenWidth, screenHeight;
} fbo;

extern std::vector<int> pboRollingShutterProgresses;
extern std::vector<float> pboRollingShutterDrifts;
extern int rollingShutterBufferCount;
extern int progressOvershoot;
extern float drift;




R_GLSL* hdrPqShader;
//GLuint tmpPBOtexture;

//two functions to bind and unbind the main framebuffer, generally just to be
//called externaly

void R_SetGL2DSize (int width, int height) {

	// set 2D virtual screen size
	qglViewport( 0, 0, width, height );
	qglScissor( 0, 0, width, height);
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, width, height, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();
}

void R_DrawQuad( GLuint tex, int width, int height) {
#ifdef HAVE_GLES
	//TODO
#else
	qglEnable(GL_TEXTURE_2D);
	if ( glState.currenttextures[0] != tex ) {
		GL_SelectTexture( 0 );
		qglBindTexture(GL_TEXTURE_2D, tex);
		glState.currenttextures[0] = tex; 
	};

	qglBegin(GL_QUADS);
	  qglTexCoord2f(0.0, 1.0); qglVertex2f(0.0  , 0.0   );	
	  qglTexCoord2f(1.0, 1.0); qglVertex2f(width, 0.0   );	
	  qglTexCoord2f(1.0, 0.0); qglVertex2f(width, height);	
	  qglTexCoord2f(0.0, 0.0); qglVertex2f(0.0  , height);	
	qglEnd();	
#endif
}

void R_FrameBuffer_GenerateMainMipMaps() {
#ifdef HAVE_GLES
	//TODO
#else
	if (mipMapsAlreadyGeneratedThisFrame) {
		return;
	}
	// Create mipmaps if supersampling
	if (superSampleMultiplier != 1 && r_fboSuperSampleMipMap->integer) {
		qglEnable(GL_TEXTURE_2D);
		if (glState.currenttextures[0] != fbo.main->color) {
			GL_SelectTexture(0);
			qglBindTexture(GL_TEXTURE_2D, fbo.main->color);
			glState.currenttextures[0] = fbo.main->color;
			qglGenerateMipmap(GL_TEXTURE_2D);
			mipMapsAlreadyGeneratedThisFrame = qtrue;
		};
	}

#endif
}

void R_DrawQuadPartial(GLuint tex, int width, int height,int offsetX, int offsetY) {
#ifdef HAVE_GLES
	//TODO
#else
	qglEnable(GL_TEXTURE_2D);
	if (glState.currenttextures[0] != tex) {
		GL_SelectTexture(0);
		qglBindTexture(GL_TEXTURE_2D, tex);
		glState.currenttextures[0] = tex;
	};

	float singlePixelTexWidth = 1.0f / (float)glConfig.vidWidth;
	float singlePixelTexHeight = 1.0f / (float)glConfig.vidHeight;
	int x2 = offsetX + width; // use instead of width
	int y2 = offsetY + height; // use instead of height
	int offsetYInverted = glConfig.vidHeight - 1 - offsetY;
	int y2Inverted = glConfig.vidHeight - 1 - y2;

	// NOTE: Might also need some switching around of offsetX and X and width but I don't have a use case to test it
	// so I'm leaving it in this possibly broken state bc its good enough for rolling shutter
	// Rolling shutter always reads entire width so there's that...
	qglBegin(GL_QUADS);
	qglTexCoord2f(offsetX*singlePixelTexWidth, y2 * singlePixelTexHeight); qglVertex2f(offsetX, y2Inverted);
	qglTexCoord2f(x2 * singlePixelTexWidth, y2 * singlePixelTexHeight); qglVertex2f(x2, y2Inverted);
	qglTexCoord2f(x2 * singlePixelTexWidth, offsetY * singlePixelTexHeight); qglVertex2f(x2, offsetYInverted);
	qglTexCoord2f(offsetX * singlePixelTexWidth, offsetY * singlePixelTexHeight); qglVertex2f(offsetX, offsetYInverted);
	qglEnd();
#endif
}

static void GetDesiredDepthType(GLenum& requestedType, GLenum& requestedFormat,int& requestedBitDepth, qboolean& isFloat, qboolean packed = qfalse) {
	requestedFormat = packed ? GL_UNSIGNED_INT_24_8_EXT : 0;
	isFloat = qfalse;
	if (!Q_stricmp(r_fboDepthBits->string,"32f")) {
		requestedBitDepth = 32;
		if (glConfig.depthMapFloatNV) {
			requestedType = packed ? GL_DEPTH32F_STENCIL8_NV : GL_DEPTH_COMPONENT32F_NV;
			requestedFormat = packed ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV_NV : GL_FLOAT;
			isFloat = qtrue;
		}
		else if (glConfig.depthMapFloat) {
			requestedType = packed ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F;
			requestedFormat = packed ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_FLOAT;
			isFloat = qtrue;
		}
		else {
			if (packed) {
				// Afaik there is no packed 32 bit depth buffer format.
				ri.Printf(PRINT_WARNING, "Floating point depth buffer requested, but not supported by GPU. Requesting 24 bit packed instead.\n");
				requestedBitDepth = 24;
				requestedType = GL_DEPTH24_STENCIL8_EXT;
				requestedFormat = GL_UNSIGNED_INT_24_8_EXT;
			}
			else {
				ri.Printf(PRINT_WARNING, "Floating point depth buffer requested, but not supported by GPU. Requesting 32 bit instead.\n");
				requestedType = GL_DEPTH_COMPONENT32;
				requestedFormat = GL_UNSIGNED_INT;
			}
		}
	}
	else if(!r_fboDepthBits->integer) { // Nothing requested
		requestedBitDepth = 0;
		requestedType = packed ? GL_DEPTH24_STENCIL8_EXT : GL_DEPTH_COMPONENT;
		requestedFormat = packed ? GL_UNSIGNED_INT_24_8_EXT : GL_UNSIGNED_INT;
	}
	else {
		if (r_fboDepthBits->integer == 32) {
			requestedBitDepth = 32;
			requestedType = packed ? GL_DEPTH32_STENCIL8 : GL_DEPTH_COMPONENT32; // this is WRONG for packed. GL_DEPTH32_STENCIL8 doesn't actually exist, that's a float format, just wrongly named
			requestedFormat = packed ? GL_UNSIGNED_INT_24_8_EXT : GL_UNSIGNED_INT;
		}
		else if (r_fboDepthBits->integer == 24) {
			requestedBitDepth = 24;
			requestedType = packed ? GL_DEPTH24_STENCIL8_EXT : GL_DEPTH_COMPONENT24;
			requestedFormat = packed ? GL_UNSIGNED_INT_24_8_EXT : GL_UNSIGNED_INT;
		}
		else {
			requestedBitDepth = 0;
			requestedType = packed ? GL_DEPTH24_STENCIL8_EXT : GL_DEPTH_COMPONENT;
			requestedFormat = packed ? GL_UNSIGNED_INT_24_8_EXT : GL_UNSIGNED_INT;
			ri.Printf(PRINT_WARNING, "Invalid value of %d for r_fboDepthBits.\n", r_fboDepthBits->integer);
		}
	}
}

/*static qboolean ProcessGLDepthEnumBits(GLenum& bindType) {
	qboolean isDepth = (qboolean)(bindType == GL_DEPTH_COMPONENT || bindType == GL_DEPTH24_STENCIL8_EXT);
	if (isDepth && r_fboDepthBits->integer) {
		if (bindType == GL_DEPTH_COMPONENT) {

			switch (r_fboDepthBits->integer) {
			case 32:
				bindType = GL_DEPTH_COMPONENT32;
				break;
			case 24:
				bindType = GL_DEPTH_COMPONENT24;
				break;
			}
		}
		else if (bindType == GL_DEPTH24_STENCIL8_EXT) {

			switch (r_fboDepthBits->integer) {
			case 32:
				bindType = GL_DEPTH32_STENCIL8;
				break;
			}
		}
	}
	return isDepth;
}*/

static int CreateTextureBuffer( int width, int height, GLenum internalFormat, GLenum format, GLenum type, int superSample ) {
	int ret = 0;
	int error = qglGetError();

	qglGenTextures( 1, (GLuint *)&ret );
	qglBindTexture(	GL_TEXTURE_2D, ret );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, superSample == 1 ?  GL_NEAREST : (r_fboSuperSampleMipMap->integer ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR) );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	qglTexImage2D(	GL_TEXTURE_2D, 0, internalFormat, width* superSample, height* superSample, 0, format, type, 0 );
	if (superSample != 1 && r_fboSuperSampleMipMap->integer) {
		qglGenerateMipmap(GL_TEXTURE_2D); 
	}
	error = qglGetError();
	return ret;
}

static int CreateRenderBuffer( int samples, int width, int height, GLenum bindType, int superSample) {
	int ret = 0;
#ifdef HAVE_GLES
	//TODO
#else
	qglGenRenderbuffers( 1, (GLuint *)&ret );
	qglBindRenderbuffer( GL_RENDERBUFFER_EXT, ret );

	if ( samples ) {
		qglRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, samples, bindType, width* superSample, height * superSample);
	} else {
		qglRenderbufferStorage(	GL_RENDERBUFFER_EXT, bindType, width * superSample, height * superSample);
	}

#endif
	return ret;
}

//------------------------------
// better framebuffer creation
//------------------------------
// for this we do a more opengl way of figuring out what level of framebuffer
// objects are supported. we try each mode from 'best' to 'worst' until we 
// get a mode that works.


void R_FrameBufferDelete( frameBufferData_t* buffer ) {  
#ifdef HAVE_GLES
	//TODO
#else
	if ( !buffer )
		return;
	qglDeleteFramebuffers(1, &(buffer->fbo));
	if ( buffer->color ) {
		if ( buffer->flags & FB_MULTISAMPLE ) {
			qglDeleteRenderbuffers(1, &(buffer->color) );
		} else {
			qglDeleteTextures( 1, &(buffer->color) );
		}
	}
	if ( buffer->depth ) {
		qglDeleteRenderbuffers(1, &(buffer->depth) );
	}
	if ( buffer->stencil ) {
		qglDeleteRenderbuffers(1, &(buffer->stencil) );
	}
	if ( buffer->packed ) {
		if ( buffer->flags & FB_MULTISAMPLE ) {
			qglDeleteRenderbuffers(1, &(buffer->packed) );
		} else {
			qglDeleteTextures( 1, &(buffer->packed) );
		}
	}
	free( buffer );
#endif
}

frameBufferData_t* R_FrameBufferCreate( int width, int height, int flags, int superSample=1 ) {
#ifdef HAVE_GLES
	//TODO
	return NULL;
#else
	frameBufferData_t *buffer;
	GLuint status;
	GLenum desiredDepthType,desiredDepthFormat;
	qboolean desiringFloatDepth;
	int desiredDepthBitDepth = 0;
	int samples = 0;

	buffer = (frameBufferData_t *)malloc( sizeof( *buffer ) );
	memset( buffer, 0, sizeof( *buffer ) );
	buffer->flags = flags;
	buffer->width = width;
	buffer->height = height;

	//gen the frame buffer
	qglGenFramebuffers(1, &(buffer->fbo) );
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, buffer->fbo);

	if ( flags & FB_MULTISAMPLE ) {
		samples = r_fboMultiSample->integer;
	}

	if ( flags & FB_PACKED ) {
		GetDesiredDepthType(desiredDepthType, desiredDepthFormat,desiredDepthBitDepth, desiringFloatDepth, qtrue);
		if ( samples ) {
			buffer->packed = CreateRenderBuffer( samples, width, height, desiredDepthType, superSample );
			qglFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffer->packed );
			qglFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffer->packed );
		} else {
			// Setup depth_stencil texture (not mipmap)
			buffer->packed = CreateTextureBuffer( width, height, desiredDepthType, GL_DEPTH_STENCIL_EXT, desiredDepthFormat,superSample );
			qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, buffer->packed, 0);
			qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, buffer->packed, 0);
		}
	} else {
		GetDesiredDepthType(desiredDepthType, desiredDepthFormat, desiredDepthBitDepth, desiringFloatDepth, qfalse);
		if (1 /*samples*/) {
			if (flags & FB_DEPTH) {
				buffer->depth = CreateRenderBuffer(samples, width, height, desiredDepthType, superSample);
				qglFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffer->depth);
			}
			if (flags & FB_STENCIL) {
				buffer->stencil = CreateRenderBuffer(samples, width, height, GL_STENCIL_INDEX8_EXT, superSample);
				qglFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, buffer->stencil);
			}
		}
		/*else { // This was just an attempt. Didn't really help with anything
			if (flags & FB_DEPTH) {
				//buffer->depth = CreateRenderBuffer(samples, width, height, desiredDepthType, superSample);
				buffer->depth = CreateTextureBuffer(width, height, desiredDepthType, GL_DEPTH_COMPONENT, desiredDepthFormat, superSample);
				//buffer->depth = CreateTextureBuffer(width, height, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, superSample);
				qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, buffer->depth,0);
			}
			if (flags & FB_STENCIL) {
				//buffer->stencil = CreateRenderBuffer(samples, width, height, GL_STENCIL_INDEX8_EXT, superSample);
				buffer->stencil = CreateTextureBuffer(width, height, GL_STENCIL_INDEX8_EXT, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, superSample);
				qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, buffer->stencil,0);
			}
		}*/		
	}
	/* Attach the color buffer */
	
	if ( samples ) {
		buffer->color = CreateRenderBuffer( samples, width, height, GL_RGBA, superSample);
		qglFramebufferRenderbuffer(	GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, buffer->color );
	} else if ( flags & FB_FLOAT16 ) {
		buffer->color = CreateTextureBuffer( width, height, RGBA16F_ARB, GL_RGBA, GL_FLOAT, superSample);
		qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, buffer->color, 0);
	} else if ( flags & FB_FLOAT32 ) {
		buffer->color = CreateTextureBuffer( width, height, RGBA32F_ARB, GL_RGBA, GL_FLOAT, superSample);
		qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, buffer->color, 0);
	} else {
		buffer->color = CreateTextureBuffer( width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, superSample);
		qglFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, buffer->color, 0);
	}
		
	status = qglCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);

	if ( status != GL_FRAMEBUFFER_COMPLETE_EXT ) {
		switch(status) {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            ri.Printf( PRINT_ALL, "Unsupported framebuffer format\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, missing attachment\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, duplicate attachment\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, attached images must have same dimensions\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, attached images must have same format\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, missing draw buffer\n" );
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            ri.Printf( PRINT_ALL, "Framebuffer incomplete, missing read buffer\n" );
            break;
	    }
		R_FrameBufferDelete( buffer );
		return 0;
	}

	GLint depthBufferBits;
	qglGetIntegerv(GL_DEPTH_BITS, &depthBufferBits);
	if (flags & FB_PACKED || flags & FB_DEPTH) {
		if (desiredDepthBitDepth && desiredDepthBitDepth != depthBufferBits) {
			ri.Printf(PRINT_WARNING, "FBO: Tried to create %d bit depth buffer, OpenGL used %d bits instead.\n", r_fboDepthBits->integer, depthBufferBits);
		}
		else {
			if (desiredDepthBitDepth == 32 && desiringFloatDepth && glConfig.depthMapFloatNV) {
				glConfig.depthMapFloatNVActive = qtrue;
			}
			if (glConfig.depthMapFloatNVActive || r_zinvert->integer > 1) {
				// Activate zinvert trick for nvidia cards. Scaling the depth map to -1:1 instead of 0:1 along with a changed projection matrix (elsewhere in code).
				// This relies on detecting the nvidia extension but r_zinvert > 1 can force it since technically other GPUs are allowed not to clamp those values now (?)
				// however most GPUs likely still do.
				qglDepthRange = dllDepthRange = depthRangeScaledNV;
			}
			
			ri.Printf(PRINT_ALL, "FBO: %d bit depth buffer created.\n", depthBufferBits);
		}
	}



	return buffer;
#endif
}

void R_FrameBuffer_Init( void ) {
#ifdef HAVE_GLES
	//TODO
#else
	int flags, width, height;
	GLenum tmp;

	memset( &fbo, 0, sizeof( fbo ) );
	r_fbo = ri.Cvar_Get( "r_fbo", "0", CVAR_ARCHIVE | CVAR_LATCH);
	r_fboDepthBits = ri.Cvar_Get( "r_fboDepthBits", "32", CVAR_ARCHIVE | CVAR_LATCH);
	r_fboDepthPacked = ri.Cvar_Get( "r_fboDepthPacked", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_fboStencilWhenNotPacked = ri.Cvar_Get( "r_fboStencilWhenNotPacked", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_fboExposure = ri.Cvar_Get("r_fboExposure", "1.0", CVAR_ARCHIVE);
	r_fboCompensateSkyTint = ri.Cvar_Get("r_fboCompensateSkyTint", "0", CVAR_ARCHIVE);
	r_fboSuperSample = ri.Cvar_Get( "r_fboSuperSample", "0", CVAR_ARCHIVE | CVAR_LATCH);	
	r_fboSuperSampleMipMap = ri.Cvar_Get( "r_fboSuperSampleMipMap", "1", CVAR_ARCHIVE | CVAR_LATCH);	
	r_fboBlur = ri.Cvar_Get( "r_fboBlur", "0", CVAR_ARCHIVE | CVAR_LATCH);	
	r_fboWidth = ri.Cvar_Get( "r_fboWidth", "0", CVAR_ARCHIVE | CVAR_LATCH);	
	r_fboHeight = ri.Cvar_Get( "r_fboHeight", "0", CVAR_ARCHIVE | CVAR_LATCH);	
	r_glDepthClamp = ri.Cvar_Get( "r_glDepthClamp", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_fboMultiSample = ri.Cvar_Get( "r_fboMultiSample", "0", CVAR_ARCHIVE | CVAR_LATCH);	
	r_floatBuffer = ri.Cvar_Get( "r_floatBuffer", "1", CVAR_ARCHIVE | CVAR_LATCH);
	r_convertToHDR = ri.Cvar_Get( "r_convertToHDR", "1", CVAR_ARCHIVE | CVAR_LATCH);

	superSampleMultiplier = pow(2, r_fboSuperSample->integer);

	// make sure all the commands added here are also		

	if (!glMMEConfig.framebufferObject ) {
		ri.Printf( PRINT_WARNING, "WARNING: Framebuffer rendering path disabled (no FBO support)\n");
		return;
	} else if (!glMMEConfig.shaderSupport) {
		ri.Printf(PRINT_WARNING, "WARNING: Framebuffer rendering path disabled (no shader support)\n");
		return;
	}

	if ( !r_fbo->integer ) {
		return;
	}

	ri.Printf( PRINT_ALL, "----- Enabling FrameBuffer Path -----\n" );


	//set our main screen flags
	qboolean floatDepthRequested;
	int requestedDepthBits;
	GetDesiredDepthType(tmp,tmp,requestedDepthBits,floatDepthRequested);
	qboolean requestingIntegral32 = (qboolean)(requestedDepthBits == 32 && !floatDepthRequested);
	flags = 0;
	if ( (glConfig.stencilBits > 0) ) {
		if (glConfig.packedDepthStencil && r_fboDepthPacked->integer && !requestingIntegral32) { // Not sure if I got this all right
			flags |= FB_PACKED;
		}
		else {
			flags |= FB_DEPTH;
			if (r_fboStencilWhenNotPacked->integer) {
				flags |= FB_STENCIL;
			}
		}
	} else {
		flags |= FB_DEPTH;
	}
	
	fbo.screenWidth = glMMEConfig.glWidth;
	fbo.screenHeight = glMMEConfig.glHeight;

	width = r_fboWidth->integer;
	height = r_fboHeight->integer;
	//Illegal width/height use original opengl one
	if ( width <= 0 || height <= 0 ) {
		width = fbo.screenWidth;
		height = fbo.screenHeight;
	}

	if (r_floatBuffer->integer > 2) {
		flags |= FB_FLOAT32;
	}
	else if (r_floatBuffer->integer) {
		flags |= FB_FLOAT16;
	}

	//create our main frame buffer
	fbo.main = R_FrameBufferCreate( width, height, flags,superSampleMultiplier );
	fbo.exposure = R_FrameBufferCreate( width, height, flags,superSampleMultiplier );

	if (!fbo.main) {
		// if the main fbuffer failed then we should disable framebuffer 
		// rendering
		glMMEConfig.framebufferObject = qfalse;
		ri.Printf( PRINT_WARNING, "WARNING: Framebuffer creation failed\n");
		ri.Printf( PRINT_WARNING, "WARNING: Framebuffer rendering path disabled\n");
		//Reinit back to window rendering in case status fails
		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, 0 );
		return;
	}

	if ( r_fboMultiSample->integer ) {
		flags |= FB_MULTISAMPLE;
		flags &= ~FB_PACKED;
		flags |= FB_DEPTH;
		fbo.multiSample = R_FrameBufferCreate( width, height, flags );
	}

	glConfig.vidWidth = width;
	glConfig.vidHeight = height;

	if ( r_fboBlur->integer > 1 ) {
		flags = FB_FLOAT32;
		fbo.blur = R_FrameBufferCreate( width, height, flags );
	} else if ( r_fboBlur->integer  ) {
		flags = FB_FLOAT16;
		fbo.blur = R_FrameBufferCreate( width, height, flags );
	}

	if (r_floatBuffer->integer > 1) { // Rolling shutter buffers eat a lot of memory. Need r_floatBuffer of at least 3 to activate 32 bit floating points for rolling shutter.
		flags = FB_FLOAT32;
	}
	else if (r_floatBuffer->integer) {
		flags = FB_FLOAT16;
	}
	R_FrameBuffer_CreateRollingShutterBuffers(width, height,flags);

	if (r_convertToHDR->integer) {
		flags = FB_FLOAT16;
		fbo.colorSpaceConv = R_FrameBufferCreate(width, height, flags);
		fbo.colorSpaceConvResult = R_FrameBufferCreate(width, height, flags);
		hdrPqShader = new R_GLSL("hdrpq-vertex.glsl","hdrpq-fragment.glsl");
		if (!hdrPqShader->IsWorking()) {
			ri.Printf(PRINT_WARNING, "WARNING: HDR PQ Shader could not be compiled. HDR conversion disabled.\n");
		}
	}

	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, 0 );
#endif
}



void R_FrameBuffer_CreateRollingShutterBuffers(int width, int height, int flags) {
	int bufferCountNeededForRollingshutter = (int)(ceil(mme_rollingShutterMultiplier->value) + 0.5f); // ceil bc if value is 1.1 we need 2 buffers. +.5 to avoid float issues..
	rollingShutterBufferCount = bufferCountNeededForRollingshutter;

	int rollingShutterFactor = glConfig.vidHeight / mme_rollingShutterPixels->integer; // For example: 1080/1 = 1080

	// For example: (1080/9.8*10)-1080 = 22.040816326530612244897959183673
	// Or: (360/9.8*10)-360 = 7.3469387755102040816326530612245
	float progressOvershootFloat = ((float)rollingShutterFactor / mme_rollingShutterMultiplier->value * (float)bufferCountNeededForRollingshutter) - (float)rollingShutterFactor;
	progressOvershoot = (int)progressOvershootFloat;
	// For example: 0.040816326530612244897959183673
	drift = progressOvershootFloat - (float)progressOvershoot;

	// create more pixel buffers if we need that for rolling shutter
	if (rollingShutterBufferCount > fbo.rollingShutterBuffers.size()) {
		fbo.rollingShutterBuffers.resize(rollingShutterBufferCount);
		pboRollingShutterProgresses.resize(rollingShutterBufferCount);
		pboRollingShutterDrifts.resize(rollingShutterBufferCount);
	}

	for (int i = 0; i < fbo.rollingShutterBuffers.size(); i++) {
		// Each rolling shutter buffer gets two buffers. That way we can make an ultra long motion blur (as long as the rolling shutter multiplier)
		fbo.rollingShutterBuffers[i].current = R_FrameBufferCreate(width, height, flags);
		fbo.rollingShutterBuffers[i].next = R_FrameBufferCreate(width, height, flags);

		// For example: -1 * (1080 / 10) = -108
		pboRollingShutterProgresses[i] = (int)(-(float)i * ((float)rollingShutterFactor / (float)bufferCountNeededForRollingshutter));
		pboRollingShutterDrifts[i] = 0.0f;
	}

}

static qboolean usedFloat;

/* Startframe checks if the framebuffer is still active or was just activated */
void R_FrameBuffer_StartFrame( void ) {
#ifdef HAVE_GLES
	//TODO
#else
	if ( !fbo.main ) {
		return;
	}
	if ( fbo.multiSample ) {
		//Bind the framebuffer at the beginning to be drawn in
		qglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo.multiSample->fbo );
	} else {
		qglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo.main->fbo );
	}
	qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	if (glConfig.depthClamp && r_glDepthClamp->integer) {
		qglEnable(GL_DEPTH_CLAMP);
	}
	qglClampColor(GL_CLAMP_VERTEX_COLOR_ARB,GL_FALSE);
	qglClampColor(GL_CLAMP_FRAGMENT_COLOR_ARB,GL_FALSE);
	qglClampColor(GL_CLAMP_READ_COLOR_ARB,GL_FALSE);
	usedFloat = qfalse;
#endif
}




qboolean R_FrameBuffer_HDRConvert(HDRConvertSource source, int param) {
#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	if (!hdrPqShader->IsWorking() || (source==HDRCONVSOURCE_FBO &&  !fbo.rollingShutterBuffers[param].current))
		return qfalse;
	
	if (source == HDRCONVSOURCE_FBO) {
		
		//qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.colorSpaceConvResult->fbo);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_DEPTHTEST_DISABLE);
		R_SetGL2DSize(glConfig.vidWidth, glConfig.vidHeight);
		qglUseProgram(hdrPqShader->ShaderId());
		R_DrawQuad(fbo.rollingShutterBuffers[param].current->color, glConfig.vidWidth, glConfig.vidHeight);
		qglUseProgram(0);

		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
		qglReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	else if (source == HDRCONVSOURCE_PBO) { // We assume the PBO is bound!

		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.colorSpaceConv->fbo);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

		// Fix random black image when saber flare happens
		// Credit: https://community.khronos.org/t/gldrawpixels-or-how-to-lose-your-time-infinitely/44513/7
		//qglClear(GL_DEPTH_BUFFER_BIT); // this alone does NOT fix it
		qglDisable(GL_TEXTURE_2D);  //this is the one that fixed the black frame on its own
		//qglDisable(GL_LIGHTING);
		//qglDisable(GL_DEPTH_TEST);

		//The color used to blur add this frame
		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE);
		R_SetGL2DSize(glConfig.vidWidth, glConfig.vidHeight);


		qglDrawPixels(glConfig.vidWidth, glConfig.vidHeight, GL_BGR_EXT, GL_FLOAT, 0);

		//qglFinish();

		
		//R_DrawQuad(fbo.main->color, glConfig.vidWidth, glConfig.vidHeight);
		//Reset fbo
		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.colorSpaceConvResult->fbo);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE);
		qglUseProgram(hdrPqShader->ShaderId());
		R_DrawQuad(fbo.colorSpaceConv->color, glConfig.vidWidth, glConfig.vidHeight);
		qglUseProgram(0);
		//qglFinish();
	}
	else if(source == HDRCONVSOURCE_MAINFBO) {
		
		R_FrameBuffer_GenerateMainMipMaps();

		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.colorSpaceConv->fbo);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		//The color used to blur add this frame
		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_DEPTHTEST_DISABLE);

		R_SetGL2DSize(glConfig.vidWidth, glConfig.vidHeight);
		R_DrawQuad(fbo.main->color, glConfig.vidWidth, glConfig.vidHeight);
		//Reset fbo
		//qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
		qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
		qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_DEPTHTEST_DISABLE);
		qglUseProgram(hdrPqShader->ShaderId());
		R_DrawQuad(fbo.colorSpaceConv->color, glConfig.vidWidth, glConfig.vidHeight);
		qglUseProgram(0);
	}

	
	return qtrue;
#endif
}

qboolean R_FrameBuffer_StartHDRRead() {
#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	if (!hdrPqShader->IsWorking())
		return qfalse;
	
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.colorSpaceConvResult->fbo);
	qglReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

	return qtrue;
#endif
}

qboolean R_FrameBuffer_EndHDRRead() {
#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	if (!hdrPqShader->IsWorking())
		return qfalse;
	
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
	qglReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	//qglReadBuffer(GL_BACK);

	return qtrue;
#endif
}

// We use a double buffer for the rolling shutter so that we can do ultra long motion blur by writing to the current AND next frame.
// When one frame is finished, we flip.
void R_FrameBuffer_RollingShutterFlipDoubleBuffer(int bufferIndex) {
	frameBufferData_t* tmp = fbo.rollingShutterBuffers[bufferIndex].next;
	fbo.rollingShutterBuffers[bufferIndex].next = fbo.rollingShutterBuffers[bufferIndex].current;
	fbo.rollingShutterBuffers[bufferIndex].current = tmp;

	// Clear the buffer for the next image so we can always use ADD blending.
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.rollingShutterBuffers[bufferIndex].next->fbo);
	qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	qglClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	qglClear(GL_COLOR_BUFFER_BIT);
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
	qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
}


qboolean R_FrameBuffer_RollingShutterCapture(int bufferIndex, int offset, int height,bool additive,bool toNextFrame,float weight) {

#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	frameBufferData_t* selectedFrameBufferData = toNextFrame ? fbo.rollingShutterBuffers[bufferIndex].next : fbo.rollingShutterBuffers[bufferIndex].current;
	
	float c;
	if (!selectedFrameBufferData)
		return qfalse;


	R_FrameBuffer_GenerateMainMipMaps();

	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, selectedFrameBufferData->fbo);
	qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	//The color used to blur add this frame
	//c = 1.0f;
	//c = 1.0f/(float)mme_rollingShutterBlur->integer;
	c = weight;
	qglColor4f(c, c , c, 1);
	if (!additive) {
		GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE);
	}
	else {
		GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE);
	}
	R_SetGL2DSize(glConfig.vidWidth, glConfig.vidHeight);
	R_DrawQuadPartial(fbo.main->color, glConfig.vidWidth, height, 0,offset);
	//Reset fbo
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.main->fbo);
	qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	/*usedFloat = qtrue;
	if (frame == total - 1) {
		qglColor4f(1, 1, 1, 1);
		GL_State(GLS_DEPTHTEST_DISABLE);
		R_DrawQuad(fbo.blur->color, glConfig.vidWidth, glConfig.vidHeight);
	}*/
	return qtrue;
#endif
}

qboolean R_FrameBuffer_Blur( float scale, int frame, int total ) {
#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	float c;
	if ( !fbo.blur )
		return qfalse;


	R_FrameBuffer_GenerateMainMipMaps();

	qglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo.blur->fbo );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	//The color used to blur add this frame
	c = scale;
	qglColor4f( c , c , c , 1 );
	if ( frame == 0 ) {
		GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_DEPTHTEST_DISABLE );
	} else {
		GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHTEST_DISABLE );
	}
	R_SetGL2DSize( glConfig.vidWidth, glConfig.vidHeight );
	R_DrawQuad(	fbo.main->color, glConfig.vidWidth, glConfig.vidHeight );
	//Reset fbo
	qglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo.main->fbo );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	usedFloat = qtrue;
	if ( frame == total - 1 ) {
		qglColor4f( 1, 1, 1, 1 );
		GL_State( GLS_DEPTHTEST_DISABLE );
		R_DrawQuad(	fbo.blur->color, glConfig.vidWidth, glConfig.vidHeight );
	}
	return qtrue;
#endif
}
#ifdef RELDEBUG
//#pragma optimize("", off)
#endif
const vec3_t brightnessWeightsPhotometric{0.2126f,0.7162f,0.0722f};
qboolean R_FrameBuffer_ApplyExposure( ) { // really kinda useless unless you want to reduce exposure sadly. Numbers are clamped.
#ifdef HAVE_GLES
	//TODO
	return qfalse;
#else
	if ( !fbo.exposure )
		return qfalse;

	r_fboExposure = ri.Cvar_Get("r_fboExposure", "1.0", CVAR_ARCHIVE);
	r_fboCompensateSkyTint = ri.Cvar_Get("r_fboCompensateSkyTint", "0", CVAR_ARCHIVE);

	if (!Q_stricmp(r_fboExposure->string, "1.0") || !Q_stricmp(r_fboExposure->string, "1")) { // Determine if we need to do this at all.
		if (!tr.mmeFBOImageTintIsSet) {
			if (!r_fboCompensateSkyTint->integer) {
				return qtrue;
			}
			else if (r_fboCompensateSkyTint->integer && !tr.mmeSkyTintIsSet) {
				return qtrue;
			}
		}
	}

	//R_FrameBuffer_GenerateMainMipMaps();

	// First copy image into exposure FBO and apply exposure
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo.exposure->fbo);
	qglDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	//The color used to blur add this frame 
	float multiplier = r_fboExposure->value;
	vec3_t tint{ 1.0f,1.0f,1.0f };
	VectorScale(tint,multiplier,tint);
	if (r_fboCompensateSkyTint->integer && tr.mmeSkyTintIsSet ) {

		vec3_t invertedSkyTint;
		VectorInvert(tr.mmeSkyTint, invertedSkyTint);
		if (r_fboCompensateSkyTint->integer == 3) {
			// compensate brightness - "photometric" mode
			float resultingBrightness = DotProduct(brightnessWeightsPhotometric, invertedSkyTint);
			VectorScale(invertedSkyTint, 1.0f / resultingBrightness,invertedSkyTint);
		}

		VectorMultiply(invertedSkyTint,tint,tint);
	}
	if (tr.mmeFBOImageTintIsSet ) {
		VectorMultiply(tr.mmeFBOImageTint,tint,tint);
	}
	qglColor4f(tint[0], tint[1], tint[2], 1.0f);
	GL_State(/*GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO |*/ GLS_DEPTHTEST_DISABLE);
	R_SetGL2DSize(glConfig.vidWidth*superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);
	R_DrawQuad(fbo.main->color, glConfig.vidWidth * superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);

	// Now copy it back
	qglBindFramebuffer( GL_FRAMEBUFFER_EXT, fbo.main->fbo );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0_EXT );
	qglColor4f(1, 1, 1, 1);
	GL_State(GLS_DEPTHTEST_DISABLE );
	R_SetGL2DSize( glConfig.vidWidth * superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);
	R_DrawQuad(	fbo.exposure->color, glConfig.vidWidth * superSampleMultiplier, glConfig.vidHeight * superSampleMultiplier);
	mipMapsAlreadyGeneratedThisFrame = qfalse;
	return qtrue;
#endif
}
#ifdef RELDEBUG
//#pragma optimize("", on)
#endif



void R_FrameBuffer_EndFrame( void ) {
#ifdef HAVE_GLES
	//TODO
#else
	if ( !fbo.main ) {
		return;
	}

	R_FrameBuffer_GenerateMainMipMaps();

	if ( fbo.multiSample ) {
		const frameBufferData_t* src = fbo.multiSample;
		const frameBufferData_t* dst = fbo.main;

		qglBindFramebuffer( GL_READ_FRAMEBUFFER_EXT, src->fbo );
		qglBindFramebuffer( GL_DRAW_FRAMEBUFFER_EXT, dst->fbo );
		qglBlitFramebufferEXT(0, 0, src->width, src->height, 0, 0, dst->width, dst->height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	}

	GL_State( GLS_DEPTHTEST_DISABLE );
	if (r_fbo->integer && r_fboOverbright->integer) {
		qglColor4f(tr.overbrightBitsMultiplier, tr.overbrightBitsMultiplier, tr.overbrightBitsMultiplier, 1);
	}
	else {
		qglColor4f(1, 1, 1, 1);
	}
	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	qglEnable(GL_FRAMEBUFFER_SRGB);
	R_SetGL2DSize( fbo.screenWidth, fbo.screenHeight );
	if ( usedFloat ) {
		R_DrawQuad(	fbo.blur->color, fbo.screenWidth, fbo.screenHeight );
	} else {
		R_DrawQuad(	fbo.main->color, fbo.screenWidth, fbo.screenHeight );
	}
	usedFloat = qfalse;
	mipMapsAlreadyGeneratedThisFrame = qfalse;
#endif
}

void R_FrameBuffer_Shutdown( void ) {
//	qglBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
	R_FrameBufferDelete( fbo.main );
	R_FrameBufferDelete( fbo.exposure );
	R_FrameBufferDelete( fbo.blur );
	R_FrameBufferDelete( fbo.multiSample );
	R_FrameBufferDelete( fbo.colorSpaceConv );
	R_FrameBufferDelete( fbo.colorSpaceConvResult );

	for (int i = 0; i < fbo.rollingShutterBuffers.size(); i++) {
		R_FrameBufferDelete(fbo.rollingShutterBuffers[i].current);
		R_FrameBufferDelete(fbo.rollingShutterBuffers[i].next);

	}
}