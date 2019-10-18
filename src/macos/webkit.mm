#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include <wchar.h>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"
#include "../trace.h"

@class WindowHandle;
@class BrowserWindow;

@interface WindowHandle : NSObject
@property(strong) BrowserWindow *window;
@property(strong) WKWebView *webview;
@property(strong) NSTimer *titletimer;
@end
@implementation WindowHandle
@end

@interface BrowserWindow : NSWindow <NSWindowDelegate>
@property(strong) WindowHandle *handle;
- (void)timedWindowTitleUpdate:(NSTimer *)timer;
- (void)windowWillClose:(NSNotification *)notification;
@end
@implementation BrowserWindow
- (void)timedWindowTitleUpdate:(NSTimer *)timer {
  // update window title with document title
  if (self.handle != NULL && self.handle.webview != NULL) {
    [self setTitle:[self.handle.webview title]];
  }
}
- (void)windowWillClose:(NSNotification *)notification {
  // clear all references on handle and remove handle reference
  if (self.handle != NULL) {
    if (self.handle.titletimer != NULL) {
      [self.handle.titletimer invalidate];
      self.handle.titletimer = NULL;
    }
    self.handle.window = NULL;
    self.handle.webview = NULL;
    self.handle = NULL;
  }
  // post application quit event
  [NSApp postEvent:[NSEvent
                       otherEventWithType:NSEventTypeApplicationDefined
                                 location:NSMakePoint(0, 0)
                            modifierFlags:0
                                timestamp:NSProcessInfo.processInfo.systemUptime
                             windowNumber:self.windowNumber
                                  context:NULL
                                  subtype:SHRT_MAX
                                    data1:0
                                    data2:0]
           atStart:NO];
  TRACEA(info, "window closed");
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
    // prepare handle object
    WindowHandle *handle = [[WindowHandle alloc] init];

    // create window
    NSRect screenSize = NSScreen.mainScreen.frame;
    NSRect targetRect =
        CGRectMake(0, 0, screenSize.size.width / 2, screenSize.size.height / 2);

    handle.window = [[BrowserWindow alloc]
        initWithContentRect:targetRect
                  styleMask:NSWindowStyleMaskTitled |
                            NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable |
                            NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO
                     screen:NSScreen.mainScreen];

    [handle.window setReleasedWhenClosed:NO];

    handle.window.delegate = handle.window;
    handle.window.handle = handle;

    [handle.window
        setTitle:[[NSString alloc]
                     initWithBytes:title
                            length:wcslen(title) * sizeof(*title)
                          encoding:NSUTF32LittleEndianStringEncoding]];
    [handle.window center];

    // create webview
    WKWebViewConfiguration *config = [WKWebViewConfiguration new];
    handle.webview = [[WKWebView alloc] initWithFrame:targetRect
                                        configuration:config];
    [handle.webview
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
    [handle.webview setAutoresizesSubviews:YES];
    [handle.webview
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    // attach webview and put window in front
    [handle.window.contentView addSubview:handle.webview];
    [handle.window orderFrontRegardless];

    // activate and finish launching
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp finishLaunching];
    [NSApp activateIgnoringOtherApps:YES];

    // create title update timer
    handle.titletimer = [NSTimer
        scheduledTimerWithTimeInterval:1
                                target:handle.window
                              selector:@selector(timedWindowTitleUpdate:)
                              userInfo:NULL
                               repeats:YES];

    TRACEA(info, "window created");
    return (__bridge_retained void *)handle;
  }
}

void audience_inner_window_destroy(void *window) {
  @autoreleasepool {
    WindowHandle *handle = (__bridge_transfer WindowHandle *)window;
    if (handle.window != NULL) {
      [handle.window close];
    }
    TRACEA(info, "public window handle destroyed");
  }
}

void audience_inner_loop() {
  bool stop_soon = false;
  while (true) {
    @autoreleasepool {
      NSEvent *event = [NSApp
          nextEventMatchingMask:NSEventMaskAny
                      untilDate:!stop_soon
                                    ? [NSDate distantFuture]
                                    : [NSDate dateWithTimeIntervalSinceNow:0.2]
                         inMode:NSDefaultRunLoopMode
                        dequeue:YES];

      if (event != NULL) {
        if (event.type == NSEventTypeApplicationDefined &&
            event.subtype == SHRT_MAX) {
          TRACEA(info, "quit event received");
          stop_soon = true;
        } else {
          [NSApp sendEvent:event];
          [NSApp updateWindows];
        }
      } else if (stop_soon) {
        break;
      }
    }
  }
}
