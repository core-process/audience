# Audience
A tiny adaptive cross-platform webview window library for C/C++ to build modern cross-platform user interfaces.

- It is **adaptive**: Audience adapts to its environment using the best available webview technology based on a priority list. E.g., on Windows, it can be configured to try embedding of EdgeHTML first and fall back to the embedding of IE11.

- It supports two-way **messaging**: the web app can post messages to the native backend, and the native backend can post messages to the web app.

- It supports web apps provided via a filesystem folder or URL. Audience provides its custom web server and websocket service tightly integrated into the library. But if you prefer a regular http URL schema, you can use Express or any other http based framework you like.

The following screenshots show a simple web app based on [jQuery Terminal](https://terminal.jcubic.pl/).

<table><tr><td><img src="examples/ping/screenshots/macos.png"></td><td><img src="examples/ping/screenshots/windows.png"></td></tr><tr><td><img src="examples/ping/screenshots/ubuntu.png"></td></tr></table>

## Compatibility

Internally the library is split into a shell (shell, as in wrapper, not as in command shell) and multiple nuclei. All nuclei share the same internal API, and each nucleus implements a specific webview technology. The shell probes the available nuclei on after another according to a priority list.

Currently, the following nuclei are implemented:

Platform | Technology | Restrictions
--- | --- | ---
Windows | EdgeHTML (WinRT) | (1)
Windows | MSHTML (OLE) | none
MacOS | WebKit (WKWebView) | none
Unix | WebKit (WebKitGTK) | none

1. EdgeHTML supports directory based provisioning only. It is not possible to serve localhost based web apps to an EdgeHTML based webview. It is meant to be a security feature of the App Container sandbox introduced by Microsoft.

The following nuclei will be implemented soon:

Platform | Technology | Expected Restrictions | Issue #
--- | --- | --- | ---
All Platforms | Chrome App Mode | (1) | [#8][i8]
Unix | QtWebEngine | none | [#14][i14]
Unix | Generic Browser Fallback | (2) | [#1][i1]

[i1]: https://github.com/core-process/audience/issues/1
[i8]: https://github.com/core-process/audience/issues/8
[i14]: https://github.com/core-process/audience/issues/14

1. Chrome App Mode: A custom window menu and icon configuration will be ignored. Use favicon to influence the window icon.
2. Generic Browser Fallback: Window positioning via Audience API will not be possible. A custom window menu and icon configuration will be ignored. Use favicon to influence the window icon.

## Simple Example

### Backend

```c++
int main(int argc, char **argv)
{
  // create and show window
  AudienceEventHandler peh{};
  if (!audience_init(&peh))
    return 1;

  AudienceWindowDetails wd{};
  wd.webapp_type = AUDIENCE_WEBAPP_TYPE_DIRECTORY;
  wd.webapp_location = "./webapp";

  AudienceWindowEventHandler weh{};
  weh.on_message.handler = [](AudienceWindowHandle handle, void *context, const char *message) {
    audience_window_post_message(handle, "pong");
  };

  if (!audience_window_create(&wd, &weh))
    return 1;

  audience_main(); // calls exit by itself
  return 0;        // just for the compiler
}
```

See [here](src/shell/app.cpp) for a complete example.

### Frontend

```html
<!DOCTYPE html>
<html>
<head>
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <title>Ping Pong</title>
  <script src="/audience.js"></script>
  <script src="/jquery-3.3.1.min.js"></script>
  <script src="/terminal/js/jquery.terminal.min.js"></script>
  <link rel="stylesheet" type="text/css" href="/terminal/css/jquery.terminal.min.css" />
  <style>
    body {
      margin: 0;
      background-color: black;
    }
  </style>
</head>
<body>
  <div id="terminal"></div>
  <script>
      var terminal = $('#terminal').terminal(function (command) {
        audience.postMessage(command);
      });
      audience.onMessage(function (message) {
        terminal.echo(message);
      });
  </script>
</body>
</html>
```

See [here](examples/ping/webapp/) for the complete example.

## API

### Backend

```c
bool audience_init(const AudienceEventHandler *event_handler);

AudienceWindowHandle audience_window_create(const AudienceWindowDetails *details, const AudienceWindowEventHandler *event_handler);

void audience_window_post_message(AudienceWindowHandle handle, const char* message);

void audience_window_destroy(AudienceWindowHandle handle);

void audience_main(); // will not return
```

See [audience_details.h](include/audience_details.h) for a specification of the data types used above.

**Events**: Audience emits process level and window level events. Use `audience_init` to register process level events and `audience_window_create` to register window level events.

| Level | Name | Signature |
| --- | --- | --- |
| Process | will_quit | ``void (*handler)(void *context, bool *prevent_quit)``|
| Process | quit | ``void (*handler)(void *context)``|
| Window | message | ``void (*handler)(AudienceWindowHandle handle, void *context, const char *message)``|
| Window | will_close | ``void (*handler)(AudienceWindowHandle handle, void *context, bool *prevent_close)``|
| Window | close | ``void (*handler)(AudienceWindowHandle handle, void *context, bool *prevent_quit)``|

**Multithreading**: `audience_init` and `audience_main` need to be called from the main thread of the process. All other methods can be called from any arbitrary thread. They will be dispatched to the main thread automatically.

### Frontend (Web App)

```js
window.audience.postMessage(message /* string */)

window.audience.onMessage(handler /* function(string) */)

window.audience.offMessage(handler /* function(string) or undefined */)
```

Currently, the implementation has to be loaded via ``<script src="/audience.js"></script>``. Path `/audience.js` is a virtual file provided by the backend. We will migrate the implementation to an NPM module. See issue [#10][i10].

[i10]: https://github.com/core-process/audience/issues/10

## Build

### Requirements

- CMake 3.15 or newer
- **Windows**: Visual Studio 2019 / MSVC
- **MacOS**: XCode / Clang
- **Unix**: GCC and make

### Windows

```bat
git clone https://github.com/core-process/audience
cd audience
.\build.bat Release
```

- The build output will be placed in `.\build\Release`.
- Add `<audience_dir>/include` to your include paths and link `audience.lib`.
- All `audience_windows_*.dll` files need to reside next to your executable.

### MacOS

```sh
git clone https://github.com/core-process/audience
cd audience
./build.sh Release
```

- The build output will be placed in `./build/Release`.
- Add `<audience_dir>/include` to your include paths and link `libaudience.a`.
- All `audience_macos_*.dylib` files need to reside next to your executable.

### Unix

```sh
git clone https://github.com/core-process/audience
cd audience
./build.sh Release
```

- The build output will be placed in `./build/Release`.
- Add `<audience_dir>/include` to your include paths and link `libaudience.a`.
- All `audience_unix_*.so` files need to reside next to your executable.
