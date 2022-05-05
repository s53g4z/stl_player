//
//  AppDelegate.m
//  STLPlayerMac
//
//  Created by user1 on 5/1/22.
//  Copyright 2022 __MyCompanyName__. All rights reserved.
//

#import "AppDelegate.h"


@implementation AppDelegate

- (OGLView *)createOGLView {
	NSRect frameRect;
	NSPoint frameRectOrigin;
	frameRectOrigin.x = 0.0f;
	frameRectOrigin.y = 0.0f;
	frameRect.origin = frameRectOrigin;
	NSSize frameRectSize;
	frameRectSize.width = 640.0f;
	frameRectSize.height = 480.0f;
	frameRect.size = frameRectSize;
	
	const NSOpenGLPixelFormatAttribute pfas[] = {
		NSOpenGLPFADoubleBuffer,
		NSOpenGLPFAMinimumPolicy,
		NSOpenGLPFAAlphaSize, 8,
		NSOpenGLPFADepthSize, 24,
		NSOpenGLPFAStencilSize, 8,
		NSOpenGLPFAColorSize, 8,
		//NSOpenGLPFARendererID, kCGLRendererGenericID,
		0,
	};
	NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc]initWithAttributes:pfas];
	assert(pixelFormat != nil);
	
	OGLView *oglView = [[OGLView alloc]initWithFrame:frameRect
										 pixelFormat:pixelFormat
	];
	assert(oglView != nil);
	return oglView;
}

- (NSWindow *)createNSWindow {
	NSRect winRect;
	NSPoint winRectOrigin;
	winRectOrigin.x = 0.0;
	winRectOrigin.y = 0.0;
	winRect.origin = winRectOrigin;
	NSSize winRectSize;
	winRectSize.width = 640.0;
	winRectSize.height = 480.0;
	winRect.size = winRectSize;
	
	NSUInteger winStyle =
		NSTitledWindowMask |
		NSMiniaturizableWindowMask |
		NSResizableWindowMask |
	0;
	
	NSBackingStoreType winBacking = NSBackingStoreRetained;
	
	NSWindow *win = [[NSWindow alloc] initWithContentRect:winRect
		styleMask:winStyle
		backing:winBacking
		defer:NO
	 ];
	assert(win != nil);
	return win;
}

- (void)createTimerOnLoop:(OGLView *)oglView {
	[NSTimer scheduledTimerWithTimeInterval:1/60.0
									 target:oglView
								   selector:@selector(draw)
								   userInfo:nil
									repeats:YES
	 ];
}

- (void)applicationDidFinishLaunching:(NSNotification *)unused {
	fprintf(stderr, "%s\n", "AppDelegate init start");
	
	OGLView *oglView = [self createOGLView];
	NSWindow *win = [self createNSWindow];
	[win setContentView:oglView];
	[win makeKeyAndOrderFront:nil];
	[win flushWindow];
	[self createTimerOnLoop:oglView];
	
	fprintf(stderr, "%s\n", "AppDelegate init end");
}

@end
