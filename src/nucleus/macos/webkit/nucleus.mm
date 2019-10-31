#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "../../../common/trace.h"
#include "../../shared/interface.h"
#include "nucleus.h"

#define NUCLEUS_COORD_FLIP_Y(rect)                                             \
  {                                                                            \
    {rect.origin.x, (NSScreen.screens[0].frame.size.height -                   \
                     rect.size.height - rect.origin.y)},                       \
    {                                                                          \
      rect.size.width, rect.size.height                                        \
    }                                                                          \
  }

@implementation AudienceWindowContextData
@end

@interface AudienceAppDelegate : NSObject <NSApplicationDelegate>
@property(strong) NSImage *appIcon;
- (void)applicationDidFinishLaunching:(NSNotification *)notification;
- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)notification;
@end

@implementation AudienceAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  NSApp.applicationIconImage = self.appIcon;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication *)sender {
  bool prevent_quit = false;
  emit_app_will_quit(prevent_quit);
  return prevent_quit ? NSTerminateCancel : NSTerminateNow;
}

- (void)applicationWillTerminate:(NSNotification *)notification {
  emit_app_quit();
  TRACEA(info, "cocoa will call exit() for us now");
}
@end

@interface AudienceWindow : NSWindow <NSWindowDelegate>
@property(strong) AudienceWindowContext context;
- (void)timedWindowTitleUpdate:(NSTimer *)timer;
- (BOOL)windowShouldClose:(NSWindow *)sender;
- (void)windowWillClose:(NSNotification *)notification;
@end

@implementation AudienceWindow
- (void)timedWindowTitleUpdate:(NSTimer *)timer {
  // update window title with document title
  if (self.context != NULL && self.context.webview != NULL) {
    [self setTitle:[self.context.webview title]];
  }
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
  // trigger event
  bool prevent_close = false;
  if (self.context != NULL) {
    emit_window_will_close(self.context, prevent_close);
  }
  return !prevent_close;
}

- (void)windowWillClose:(NSNotification *)notification {
  bool prevent_quit = false;

  if (self.context != NULL) {
    // trigger event
    emit_window_close(self.context, prevent_quit);

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
  if (!prevent_quit) {
    dispatch_async(dispatch_get_main_queue(), ^{
      TRACEA(info, "calling NSApp.terminate()");
      [NSApp terminate:NULL];
    });
  }

  TRACEA(info, "window closed");
}
@end

AudienceAppDelegate *_nucleus_app_delegate = nullptr;

bool nucleus_impl_init(AudienceNucleusProtocolNegotiation &negotiation,
                       const NucleusImplAppDetails &details) {
  // negotiate protocol
  negotiation.nucleus_handles_webapp_type_url = true;

  // init shared application object
  [NSApplication sharedApplication];

  // set delegate (we need a strong reference here)
  _nucleus_app_delegate = [[AudienceAppDelegate alloc] init];
  [NSApp setDelegate:_nucleus_app_delegate];

  // apply icon (we simply pick the largest icon available)
  NSImage *select_app_icon = nullptr;
  for (auto &icon_path : details.icon_set) {
    TRACEW(info, "loading icon " << icon_path);
    NSImage *app_icon = [[NSImage alloc]
        initWithContentsOfFile:
            [[NSString alloc] initWithBytes:icon_path.c_str()
                                     length:icon_path.length() * sizeof(wchar_t)
                                   encoding:NSUTF32LittleEndianStringEncoding]];
    if (app_icon.size.width == 0) {
      continue;
    }
    if (select_app_icon == nullptr ||
        select_app_icon.size.width < app_icon.size.width) {
      select_app_icon = app_icon;
    }
  }

  if (select_app_icon != nullptr) {
    TRACEW(info, "selecting icon with width = " << select_app_icon.size.width);
    _nucleus_app_delegate.appIcon = select_app_icon;
  }

  TRACEA(info, "initialized");
  return true;
}

AudienceScreenList nucleus_impl_screen_list() {
  AudienceScreenList result{};

  for (NSScreen *screen in NSScreen.screens) {
    if (result.count >= AUDIENCE_SCREEN_LIST_ENTRIES) {
      break;
    }

    if (screen == NSScreen.mainScreen) {
      result.focused = result.count;
    }

    // NOTE: first screen in array is always primary screen, no need to set
    //       anything

    AudienceRect frame{{screen.frame.origin.x, screen.frame.origin.y},
                       {screen.frame.size.width, screen.frame.size.height}};

    AudienceRect workspace{
        {screen.visibleFrame.origin.x, screen.visibleFrame.origin.y},
        {screen.visibleFrame.size.width, screen.visibleFrame.size.height}};

    result.screens[result.count].frame = NUCLEUS_COORD_FLIP_Y(frame);
    result.screens[result.count].workspace = NUCLEUS_COORD_FLIP_Y(workspace);

    result.count += 1;
  }

  return result;
}

AudienceWindowContext
nucleus_impl_window_create(const NucleusImplWindowDetails &details) {
  // prepare context object
  AudienceWindowContext context = [[AudienceWindowContextData alloc] init];

  // create window
  NSRect screenSize = NSScreen.mainScreen.frame;
  NSRect targetRect =
      CGRectMake(0, 0, screenSize.size.width / 2, screenSize.size.height / 2);

  context.window = [[AudienceWindow alloc]
      initWithContentRect:targetRect
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

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

  // enable dev tools
  if (details.dev_mode) {
    [context.webview.configuration.preferences
        setValue:@YES
          forKey:@"developerExtrasEnabled"];
  }

  // attach webview
  [context.window.contentView addSubview:context.webview];

  // set window styles
  if (details.styles.not_decorated) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    context.window.styleMask |= NSFullSizeContentViewWindowMask;
#pragma clang diagnostic pop
    context.window.titlebarAppearsTransparent = TRUE;
    context.window.titleVisibility = NSWindowTitleHidden;
    [context.window standardWindowButton:NSWindowCloseButton].hidden = TRUE;
    [context.window standardWindowButton:NSWindowMiniaturizeButton].hidden =
        TRUE;
    [context.window standardWindowButton:NSWindowZoomButton].hidden = TRUE;
  }

  if (details.styles.not_resizable) {
    context.window.styleMask &= ~NSWindowStyleMaskResizable;
  }

  if (details.styles.always_on_top) {
    context.window.level = NSFloatingWindowLevel;
  }

  // position window
  if (details.position.size.width > 0 && details.position.size.height > 0) {
    nucleus_impl_window_update_position(context, details.position);
  }

  // create title update timer
  context.titletimer =
      [NSTimer scheduledTimerWithTimeInterval:1
                                       target:context.window
                                     selector:@selector(timedWindowTitleUpdate:)
                                     userInfo:NULL
                                      repeats:YES];

  // show window and activate app
  [context.window orderFrontRegardless];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:YES];

  TRACEA(info, "window created");
  return context;
}

NucleusImplWindowStatus
nucleus_impl_window_status(AudienceWindowContext context) {
  NucleusImplWindowStatus result{};

  result.has_focus = context.window.isKeyWindow;

  AudienceRect frame{
      {context.window.frame.origin.x, context.window.frame.origin.y},
      {context.window.frame.size.width, context.window.frame.size.height}};

  result.frame = NUCLEUS_COORD_FLIP_Y(frame);

  result.workspace = {context.window.contentView.frame.size.width,
                      context.window.contentView.frame.size.height};

  return result;
}

void nucleus_impl_window_update_position(AudienceWindowContext context,
                                         AudienceRect position) {

  TRACEA(debug, "window_update_position: origin="
                    << position.origin.x << "," << position.origin.y << " size="
                    << position.size.width << "x" << position.size.height);

  NSRect frame{{position.origin.x, position.origin.y},
               {position.size.width, position.size.height}};

  frame = NUCLEUS_COORD_FLIP_Y(frame);

  [context.window setFrame:frame display:TRUE];
}

void nucleus_impl_window_post_message(AudienceWindowContext context,
                                      const std::wstring &message) {}

void nucleus_impl_window_destroy(AudienceWindowContext context) {
  // perform close operation
  dispatch_async(dispatch_get_main_queue(), ^{
    if (context.window != NULL) {
      [context.window close];
      TRACEA(info, "window close triggered");
    }
  });
}

void nucleus_impl_main() {
  [NSApp run];
  // NSApp.run() calls exit() by itself
}

void nucleus_impl_dispatch_sync(void (*task)(void *context), void *context) {
  TRACEA(info, "dispatching task on main queue (sync)");
  dispatch_sync(dispatch_get_main_queue(), ^{
    task(context);
  });
}

void nucleus_impl_dispatch_async(void (*task)(void *context), void *context) {
  TRACEA(info, "dispatching task on main queue (async)");
  dispatch_async(dispatch_get_main_queue(), ^{
    task(context);
  });
}
