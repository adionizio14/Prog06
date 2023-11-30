//
//  gl_frontEnd.h
//  GL threads
//
//  Created by Jean-Yves Hervé on 2017-04-24.
//  Copyright © 2017 Jean-Yves Hervé. All rights reserved.
//

#ifndef GL_FRONT_END_H
#define GL_FRONT_END_H


//------------------------------------------------------------------------------
//	Find out whether we are on Linux or macOS (sorry, Windows people)
//	and load the OpenGL & glut headers.
//	For the macOS, lets us choose which glut to use
//------------------------------------------------------------------------------
#if (defined(__dest_os) && (__dest_os == __mac_os )) || \
	defined(__APPLE_CPP__) || defined(__APPLE_CC__)
	//	Either use the Apple-provided---but deprecated---glut
	//	or the third-party freeglut install
	#if 1
		#include <GLUT/GLUT.h>
	#else
		#include <GL/freeglut.h>
		#include <GL/gl.h>
	#endif
#elif defined(__linux__)
	#include <GL/glut.h>
#else
	#error unknown OS
#endif

#include "RasterImage.h"

//-----------------------------------------------------------------------------
//	Function prototypes
//-----------------------------------------------------------------------------


void drawState(int numMessages, char** message);

void initializeFrontEnd(int argc, char** argv, RasterImage* imageOut);

//	Functions implemented in main.cpp
void displayImage(GLfloat scaleX, GLfloat scaleY);
void displayState(void);
void handleKeyboardEvent(unsigned char c, int x, int y);

#endif // GL_FRONT_END_H

