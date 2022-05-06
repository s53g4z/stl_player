//
//  main.m
//  STLPlayerMac
//
//  Created by user1 on 5/1/22.
//

#import <Cocoa/Cocoa.h>
#import "AppDelegate.h"

int main(void)
{
    [NSApplication sharedApplication];
    [NSApp setDelegate:[[AppDelegate alloc]init]];
    [NSApp run];
}
