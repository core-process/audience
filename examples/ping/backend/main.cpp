#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <audience.h>

static std::string utf16_to_utf8(const std::wstring &data)
{
  static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(data);
}

static std::wstring utf8_to_utf16(const std::string &data)
{
  static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(data);
}

int main(int argc, char **argv)
{
  // retrieve arguments
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <PATH_TO_WEBAPP>" << std::endl;
    return 1;
  }

  std::wstring webapp_dir = utf8_to_utf16(argv[1]);

  // initialize audience
  AudienceAppDetails ad{};
  ad.load_order.windows[0] = AUDIENCE_NUCLEUS_WINDOWS_EDGE;
  ad.load_order.windows[1] = AUDIENCE_NUCLEUS_WINDOWS_IE11;
  ad.load_order.macos[0] = AUDIENCE_NUCLEUS_MACOS_WEBKIT;
  ad.load_order.unix[0] = AUDIENCE_NUCLEUS_UNIX_WEBKIT;

  AudienceAppEventHandler aeh{};

  if (!audience_init(&ad, &aeh))
  {
    std::cerr << "Could not initialize Audience framework." << std::endl;
    return 2;
  }

  // prepare window configuration
  AudienceWindowDetails wd{};

  wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
  wd.webapp_location = webapp_dir.c_str();

  wd.styles.always_on_top = true;
  wd.styles.not_decorated = true;
  wd.styles.not_resizable = true;

  auto screens = audience_screen_list();
  auto workspace = screens.screens[screens.focused].workspace;
  wd.position.size = {workspace.size.width * 0.5, workspace.size.height * 0.5};
  wd.position.origin = workspace.origin;
  wd.position.origin.x += workspace.size.width * 0.25;
  wd.position.origin.y += workspace.size.height * 0.25;

  // prepare window handler
  AudienceWindowEventHandler weh{};
  weh.on_close_intent.handler = [](AudienceWindowHandle handle, void *context) {
    audience_window_destroy(handle);
  };
  weh.on_close.handler = [](AudienceWindowHandle handle, void *context, bool is_last_window) {
    if (is_last_window)
    {
      audience_quit();
    }
  };

  // create window
  if (!audience_window_create(&wd, &weh))
  {
    std::cerr << "Could not create Audience window." << std::endl;
    return 2;
  }

  // run main loop
  audience_main(); // calls exit by itself
  return 0;        // just for the compiler
}
