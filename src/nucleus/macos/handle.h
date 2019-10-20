#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

@class BrowserWindow;

@interface AudienceHandle : NSObject
@property(strong) BrowserWindow *window;
@property(strong) WKWebView *webview;
@property(strong) NSTimer *titletimer;
@end
