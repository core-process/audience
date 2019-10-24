#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

@class AudienceWindow;

@interface AudienceWindowContextData : NSObject
@property(strong) AudienceWindow *window;
@property(strong) WKWebView *webview;
@property(strong) NSTimer *titletimer;
@end

typedef AudienceWindowContextData *AudienceWindowContext;
