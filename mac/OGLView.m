//
//  OGLView.m
//  Untitled4
//
//  Created by user1 on 5/1/22.
//  Copyright 2022 __MyCompanyName__. All rights reserved.
//

#import "OGLView.h"

@implementation OGLView : NSOpenGLView {
	time_t prevTime;
	unsigned long long fps;
	keys k;
}

- (void)checkCGLErrCode:(CGLError *)errCode {
	if (*errCode != 0) {
		unsigned shim;
		memcpy(
			   &shim,
			   errCode,
			   sizeof(unsigned) <= sizeof(CGLError) ?
				sizeof(unsigned) :
				sizeof(CGLError)
		);
		fprintf(stderr, "ERROR: CGL returned %u\n", shim);
	}
}

- (time_t)getCurrentTime {
	time_t ret = time(NULL);
	must(ret != (time_t)(-1));
	return ret;
}

- (id)init {
	must(false);
	return nil;
}

- (void)findSelfOnMac {
	NSBundle *bundle = [NSBundle mainBundle];
	must(bundle != nil);
	NSString *resPathNS = [bundle resourcePath];
	const char *resPath = [resPathNS cStringUsingEncoding:NSUTF8StringEncoding];
	must(resPath != NULL && strlen(resPath) < 4096 - strlen("/"));
	strcpy(gSelf, resPath);
	strcpy(gSelf + strlen(gSelf), "/");
	gSelf_len = strlen(gSelf);
}

- (void)enableGLblend {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	assert(glGetError() == GL_NO_ERROR);
}

- (id)initWithFrame:(NSRect)frameRect pixelFormat:(NSOpenGLPixelFormat *)format {
	self = [super initWithFrame:frameRect pixelFormat:format];
	if (self) {
		srand((unsigned)[self getCurrentTime]);
		prevTime = [self getCurrentTime];
		fps = 0;
		
		[self enableGLblend];
		const int val = 0;
		[[self openGLContext] setValues:&val forParameter:NSOpenGLCPSurfaceOpacity];
		
		[self findSelfOnMac];
	}
	return self;
}

- (double)randBetween0and1 {
	return (((double)rand()) / (RAND_MAX));
}

- (void)maybePrintFPS {
	time_t currTime = [self getCurrentTime];
	if (currTime - prevTime >= 1.0) {
		fprintf(stderr, "DEBUG: fps is %llu\n", fps);
		prevTime = currTime;
		fps = 0;
	}
	fps++;
}

- (void)checkGLErrCode {
	GLenum glErr = glGetError();
	if (glErr != GL_NO_ERROR)
		fprintf(stderr, "ERROR: GL generated error code %d\n", glErr);
}

- (void)draw_ {
	[self maybePrintFPS];
	
	CGLContextObj cxt = [[self openGLContext] CGLContextObj];
	
	CGLError errCode;
	errCode = CGLSetCurrentContext(cxt);
	[self checkCGLErrCode:&errCode];
	
	//double red = [self randBetween0and1];
	//double green = [self randBetween0and1];
	double blue = [self randBetween0and1] / 2.0;
	glClearColor(0, 0, blue, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	glColor3f(1, 0, 0);
	glBegin(GL_TRIANGLES);
	glVertex2f(-0.5, 0.0);
	glVertex2f(0.5, 0.0);
	glVertex2f(0.0, 0.5);
	glEnd();
	
	glFlush();
	[[self openGLContext]flushBuffer];
	[self checkGLErrCode];
}

- (void)draw {
	[self maybePrintFPS];
	
	CGLContextObj cxtCGL = [[self openGLContext] CGLContextObj];
	
	CGLError errCode;
	errCode = CGLSetCurrentContext(cxtCGL);
	[self checkCGLErrCode:&errCode];
	
	//keys k;  // todo: wire up
	//memset(&k, 0x00, sizeof(k));
	int resWidth = 640, resHeight = 480;  // todo: wire up
	
	must(sizeof(double) == sizeof(NSTimeInterval));
	static double then = 0.0, now = 0.0;
	now = [[NSDate date]timeIntervalSince1970];
	if (!then) {
		then = now;
		do {
			fprintf(stderr, "%s\n", "DEBUG: bootstrapping timer ...");
			now = [[NSDate date]timeIntervalSince1970];
		} while (then >= now);
	}
	
	uint8_t physicsRanTimes = 0;
	while (then < now) {
		core(&k, !displayingMessage, &resWidth, &resHeight);
		then += 1.0 / 60;
		physicsRanTimes++;
		
		if (physicsRanTimes >= 5) {
			then = now;
			break;
		}
	}
	if (physicsRanTimes == 0) {
		fprintf(stderr, "DEBUG: physics ran 0 times, drawing dummy frame\n");
		core(&k, false, &resWidth, &resHeight);
	} else if (physicsRanTimes > 1) {
		fprintf(stderr, "DEBUG: physics ran %d times\n", physicsRanTimes);
	}
	
	[[self openGLContext]flushBuffer];
	[self checkGLErrCode];
}

- (void)drawRect:(NSRect)bounds {
	[super drawRect:bounds];
	[self draw];
	fprintf(stderr, "%s\n", "DEBUG: drawRect called");
}

- (void)reshape {
	[super reshape];
	[self draw];
	fprintf(stderr, "%s\n", "DEBUG: reshape called");
}

- (void)update {
	[super update];
	[self draw];
	fprintf(stderr, "%s\n", "DEBUG: update called");
}

- (BOOL)acceptsFirstResponder {
	return YES;
}

- (void)updateStructKeys:(bool)keyState keyCode:(int16_t)keyCode {
	switch (keyCode) {
		case 13:
		case 126:
		case 49:
			k.keyW = keyState;
			break;
		case 0:
		case 123:
			k.keyA = keyState;
			break;
		case 1:
		case 125:
			k.keyS = keyState;
			break;
		case 2:
		case 124:
			k.keyD = keyState;
			break;
		case 116:
			k.keyPgUp = keyState;
			break;
		case 121:
			k.keyPgDown = keyState;
			break;
		case 53:
			exit(0);  // xxx dirty exit
			k.keyEsc = keyState;
			break;
		case 36:
			k.keyEnter = keyState;
			break;
		default:
			fprintf(stderr, "DEBUG: received unexpected keyCode %hu\n", keyCode);
	}
}

- (void)keyUp:(NSEvent *)event {
	assert([event type] == NSKeyUp);	
	[self updateStructKeys:false keyCode:[event keyCode]];
}

- (void)keyDown:(NSEvent *)event {
	assert([event type] == NSKeyDown);
	must(sizeof(short) == sizeof(int16_t));
	[self updateStructKeys:true keyCode:[event keyCode]];
}

- (void)flagsChanged:(NSEvent *)event {
	assert([event type] == NSFlagsChanged);
	must(sizeof(unsigned int) == sizeof(uint32_t));
	uint32_t modifierFlags = [event modifierFlags];
	if (modifierFlags & NSControlKeyMask)
		k.keyCTRL = true;
	else
		k.keyCTRL = false;
}

@end
