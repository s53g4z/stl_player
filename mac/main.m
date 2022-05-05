//
//  main.m
//  STLPlayerMac
//
//  Created by user1 on 5/1/22.
//  Copyright __MyCompanyName__ 2022. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

int main(int argc, char *argv[])
{
	[NSApplication sharedApplication];
	[NSApp setDelegate:[[AppDelegate alloc]init]];
	[NSApp run];
    //return NSApplicationMain(argc,  (const char **) argv);
}
