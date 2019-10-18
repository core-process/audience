#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <wchar.h>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

@interface WindowHandle : NSObject
@property NSWindow *window;
@property WKWebView *webview;
@property NSTimer *title_timer;
@end

@implementation WindowHandle
@end

@interface WindowWebviewDelegate : NSObject
- (void)timedWindowTitleUpdate:(NSTimer *)timer;
@end

@implementation WindowWebviewDelegate
- (void)timedWindowTitleUpdate:(NSTimer *)timer {
  WindowHandle *handle = [timer userInfo];
  [handle.window setTitle:[handle.webview title]];
}
@end

bool audience_inner_init() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    return true;
  }
}

void *audience_inner_window_create(const wchar_t *const title,
                                   const wchar_t *const url) {
  @autoreleasepool {
    // create window
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, 500, 500)
                                    styleMask:NSWindowStyleMaskTitled |
                                              NSWindowStyleMaskClosable |
                                              NSWindowStyleMaskMiniaturizable |
                                              NSWindowStyleMaskResizable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [window setTitle:[[NSString alloc]
                         initWithBytes:title
                                length:wcslen(title) * sizeof(*title)
                              encoding:NSUTF32LittleEndianStringEncoding]];
    [window center];

    // create webview
    WKWebViewConfiguration *config = [WKWebViewConfiguration new];
    WKWebView *webview =
        [[WKWebView alloc] initWithFrame:CGRectMake(0, 0, 500, 500)
                           configuration:config];
    [webview
        loadRequest:
            [NSURLRequest
                requestWithURL:
                    [NSURL
                        URLWithString:
                            [[NSString alloc]
                                initWithBytes:url
                                       length:wcslen(url) * sizeof(*url)
                                     encoding:
                                         NSUTF32LittleEndianStringEncoding]]]];
    [webview setAutoresizesSubviews:YES];
    [webview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    // attach webview and put window in front
    [[window contentView] addSubview:webview];
    [window orderFrontRegardless];

    // activate and finish launching
    [[NSApplication sharedApplication]
        setActivationPolicy:NSApplicationActivationPolicyRegular];
    [[NSApplication sharedApplication] finishLaunching];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];

    // prepare handle
    WindowHandle *handle = [[WindowHandle alloc] init];
    handle.window = window;
    handle.webview = webview;

    // create title update timer
    WindowWebviewDelegate *delegate = [[WindowWebviewDelegate alloc] init];
    handle.title_timer =
        [NSTimer scheduledTimerWithTimeInterval:1
                                         target:delegate
                                       selector:@selector(timedWindowTitleUpdate:)
                                       userInfo:handle
                                        repeats:YES];

    return (__bridge_retained void *)handle;
  }
}

void audience_inner_window_destroy(void *window) {
  @autoreleasepool {
    WindowHandle *handle = (__bridge_transfer WindowHandle *)window;
    [handle.title_timer invalidate];
  }
}

void audience_inner_loop() {
  while (YES) {
    @autoreleasepool {
      NSEvent *event = [[NSApplication sharedApplication]
          nextEventMatchingMask:NSEventMaskAny
                      untilDate:[NSDate distantFuture]
                         inMode:NSDefaultRunLoopMode
                        dequeue:YES];

      [[NSApplication sharedApplication] sendEvent:event];
      [[NSApplication sharedApplication] updateWindows];
    }
  }
}
