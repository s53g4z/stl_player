//
//  OGLView.h
//  Untitled4
//
//  Created by user1 on 5/1/22.
//  Copyright 2022 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#include "util.h"
#include "stlplayer.h"

@interface OGLView : NSOpenGLView {
	time_t prevTime;
	unsigned long long fps;
	keys k;
}

- (id)initWithFrame:(NSRect)frameRect pixelFormat:(NSOpenGLPixelFormat *)format;
- (void)drawRect:(NSRect)bounds;
- (void)reshape;
- (void)update;
- (void)draw;  // call me

@end
