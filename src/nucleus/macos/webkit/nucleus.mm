#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "../../../common/trace.h"
#include "../../shared/interface.h"
#include "nucleus.h"

@implementation AudienceHandle
@end

@interface BrowserWindow : NSWindow <NSWindowDelegate>
@property(strong) AudienceHandle *handle;
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
  [NSApp terminate:self];
  TRACEA(info, "window closed");
}
@end

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation) {
  // negotiate protocol
  negotiation->allow_webapp_type_url = true;

  // init shared application object
  [NSApplication sharedApplication];
  TRACEA(info, "initialized");

  return true;
}

AudienceHandle *internal_window_create(const InternalWindowDetails &details) {
  // check parameter
  if (details.webapp_type != AUDIENCE_WEBAPP_TYPE_URL) {
    TRACEA(error, "only url based web apps are supported");
    return nullptr;
  }

  // prepare handle object
  AudienceHandle *handle = [[AudienceHandle alloc] init];

  // create window
  NSRect screenSize = NSScreen.mainScreen.frame;
  NSRect targetRect =
      CGRectMake(0, 0, screenSize.size.width / 2, screenSize.size.height / 2);

  handle.window = [[BrowserWindow alloc]
      initWithContentRect:targetRect
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO
                   screen:NSScreen.mainScreen];

  [handle.window setReleasedWhenClosed:NO];

  handle.window.delegate = handle.window;
  handle.window.handle = handle;

  [handle.window setTitle:[[NSString alloc]
                              initWithBytes:details.loading_title.c_str()
                                     length:details.loading_title.length() *
                                            sizeof(wchar_t)
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
                              initWithBytes:details.webapp_location.c_str()
                                     length:details.webapp_location.length() *
                                            sizeof(wchar_t)
                                   encoding:
                                       NSUTF32LittleEndianStringEncoding]]]];
  [handle.webview setAutoresizesSubviews:YES];
  [handle.webview setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  // attach webview and put window in front
  [handle.window.contentView addSubview:handle.webview];
  [handle.window orderFrontRegardless];

  // activate and finish launching
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];
  [NSApp activateIgnoringOtherApps:YES];

  // create title update timer
  handle.titletimer =
      [NSTimer scheduledTimerWithTimeInterval:1
                                       target:handle.window
                                     selector:@selector(timedWindowTitleUpdate:)
                                     userInfo:NULL
                                      repeats:YES];

  TRACEA(info, "window created");
  return handle;
}

void internal_window_destroy(AudienceHandle *handle) {
  if (handle.window != NULL) {
    [handle.window close];
  }
  TRACEA(info, "public window handle destroyed");
}

void internal_loop() { [NSApp run]; }
