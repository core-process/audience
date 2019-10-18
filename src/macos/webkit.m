#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <wchar.h>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

bool audience_inner_init() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    return true;
  }
}

void *audience_inner_window_create(const wchar_t *const title,
                                   const wchar_t *const url) {
  @autoreleasepool {
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

    [[window contentView] addSubview:webview];
    [window orderFrontRegardless];

    [[NSApplication sharedApplication]
        setActivationPolicy:NSApplicationActivationPolicyRegular];
    [[NSApplication sharedApplication] finishLaunching];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];

    return (__bridge_retained void *)window;
  }
}

void audience_inner_window_destroy(void *window) {
  @autoreleasepool {
    NSWindow *w = (__bridge_transfer NSWindow *)window;
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
