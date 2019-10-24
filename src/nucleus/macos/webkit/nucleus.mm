#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "../../../common/trace.h"
#include "../../shared/interface.h"
#include "nucleus.h"

@implementation AudienceWindowContextData
@end

@interface BrowserWindow : NSWindow <NSWindowDelegate>
@property(strong) AudienceWindowContext context;
- (void)timedWindowTitleUpdate:(NSTimer *)timer;
- (void)windowWillClose:(NSNotification *)notification;
@end

@implementation BrowserWindow
- (void)timedWindowTitleUpdate:(NSTimer *)timer {
  // update window title with document title
  if (self.context != NULL && self.context.webview != NULL) {
    [self setTitle:[self.context.webview title]];
  }
}

- (void)windowWillClose:(NSNotification *)notification {
  if (self.context != NULL) {
    // trigger event
    internal_on_window_destroyed(self.context);

    // invalidate title timer
    if (self.context.titletimer != NULL) {
      [self.context.titletimer invalidate];
      self.context.titletimer = NULL;
    }

    // remove all references
    self.context.window = NULL;
    self.context.webview = NULL;
    self.context = NULL;
  }

  // post application quit event
  [NSApp terminate:self];
  TRACEA(info, "window closed");
}
@end

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation) {
  // negotiate protocol
  negotiation->nucleus_handles_webapp_type_url = true;

  // init shared application object
  [NSApplication sharedApplication];
  TRACEA(info, "initialized");

  return true;
}

AudienceWindowContext
internal_window_create(const InternalWindowDetails &details) {
  // prepare context object
  AudienceWindowContext context = [[AudienceWindowContextData alloc] init];

  // create window
  NSRect screenSize = NSScreen.mainScreen.frame;
  NSRect targetRect =
      CGRectMake(0, 0, screenSize.size.width / 2, screenSize.size.height / 2);

  context.window = [[BrowserWindow alloc]
      initWithContentRect:targetRect
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO
                   screen:NSScreen.mainScreen];

  [context.window setReleasedWhenClosed:NO];

  context.window.delegate = context.window;
  context.window.context = context;

  [context.window
      setTitle:[[NSString alloc]
                   initWithBytes:details.loading_title.c_str()
                          length:details.loading_title.length() *
                                 sizeof(wchar_t)
                        encoding:NSUTF32LittleEndianStringEncoding]];
  [context.window center];

  // create webview
  WKWebViewConfiguration *config = [WKWebViewConfiguration new];
  context.webview = [[WKWebView alloc] initWithFrame:targetRect
                                       configuration:config];
  [context.webview
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
  [context.webview setAutoresizesSubviews:YES];
  [context.webview
      setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  // attach webview and put window in front
  [context.window.contentView addSubview:context.webview];
  [context.window orderFrontRegardless];

  // activate and finish launching
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];
  [NSApp activateIgnoringOtherApps:YES];

  // create title update timer
  context.titletimer =
      [NSTimer scheduledTimerWithTimeInterval:1
                                       target:context.window
                                     selector:@selector(timedWindowTitleUpdate:)
                                     userInfo:NULL
                                      repeats:YES];

  TRACEA(info, "window created");
  return context;
}

void internal_window_destroy(AudienceWindowContext context) {
  // perform close operation
  if (context != NULL && context.window != NULL) {
    [context.window close];
    TRACEA(info, "window close triggered");
  }
}

void internal_loop() { [NSApp run]; }
