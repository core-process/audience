#include <memory>
#include <stdexcept>
#include <mutex>
#include <condition_variable>

#include "../../../common/trace.h"
#include "../../../common/scope_guard.h"
#include "../../shared/interface.h"
#include "../shared/init.h"
#include "../shared/resource.h"
#include "webview.h"
#include "nucleus.h"

#define WM_AUDIENCE_DISPATCH (WM_APP + 1)

bool fix_ie_compat_mode();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MessageWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

HWND _audience_message_window = nullptr;

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation)
{
  // negotiate protocol
  negotiation->nucleus_handles_webapp_type_url = true;

  // fix ie compat mode
  if (!fix_ie_compat_mode())
  {
    return false;
  }

  // initialize COM
  auto r = OleInitialize(NULL);
  if (r != S_OK && r != S_FALSE)
  {
    return false;
  }

  TRACEA(info, "COM initialization succeeded");

  // create message window
  WNDCLASSEXW wndcls = {};
  wndcls.cbSize = sizeof(WNDCLASSEX);
  wndcls.lpfnWndProc = NUCLEUS_SAFE_FN(MessageWndProc, 0);
  wndcls.hInstance = hInstanceEXE;
  wndcls.lpszClassName = L"audience_ie11_message";

  if (RegisterClassExW(&wndcls) == 0)
  {
    return false;
  }

  _audience_message_window = CreateWindowExW(0, wndcls.lpszClassName, L"audience_ie11_message", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
  if (!_audience_message_window)
  {
    return false;
  }

  return true;
}

AudienceWindowContext internal_window_create(const InternalWindowDetails &details)
{
  scope_guard scope_fail(scope_guard::execution::exception);

  // register window class
  WNDCLASSEXW wndcls;

  wndcls.cbSize = sizeof(WNDCLASSEX);
  wndcls.style = CS_HREDRAW | CS_VREDRAW;
  wndcls.lpfnWndProc = NUCLEUS_SAFE_FN(WndProc, 0);
  wndcls.cbClsExtra = 0;
  wndcls.cbWndExtra = 0;
  wndcls.hInstance = hInstanceEXE;
  wndcls.hIcon = LoadIcon(hInstanceDLL, MAKEINTRESOURCE(IDI_AUDIENCE));
  wndcls.hIconSm = LoadIcon(hInstanceDLL, MAKEINTRESOURCE(IDI_AUDIENCE));
  wndcls.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wndcls.lpszMenuName = MAKEINTRESOURCEW(IDC_AUDIENCE);
  wndcls.lpszClassName = L"audience_ie11";

  if (RegisterClassExW(&wndcls) == 0)
  {
    return {};
  }

  // create window
  AudienceWindowContext context = std::make_shared<AudienceWindowContextData>();

  HWND window = CreateWindowW(wndcls.lpszClassName, details.loading_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstanceEXE, &context);
  if (!window)
  {
    return {};
  }

  scope_fail += [window]() { DestroyWindow(window); };

  // create browser widget
  context->webview = new IEWebView();
  context->webview->AddRef();

  scope_fail += [context]() {
    if (context->webview != nullptr)
    {
      context->webview->Release();
      context->webview = nullptr;
    }
  };

  if (!context->webview->Create(window))
  {
    throw std::runtime_error("creation of web view failed");
  }

  // navigate to url
  context->webview->Navigate(details.webapp_location.c_str());

  // show window
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  TRACEA(info, "window created successfully");
  return context;
}

void internal_window_destroy(AudienceWindowContext context)
{
  // destroy window
  if (context->window != nullptr)
  {
    DestroyWindow(context->window);
    TRACEA(info, "window destroy triggered");
  }
}

void internal_main()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // trigger final event
  internal_on_process_quit();

  // lets quit now
  TRACEA(info, "calling ExitProcess()");
  ExitProcess(0);
}

void internal_dispatch_sync(void (*task)(void *context), void *context)
{
  // NOTE: We cannot use SendMessageW, because of some COM quirks. Therefore we use PostMessageW and thread based signaling.
  
  // sync utilities
  bool ready = false;
  std::condition_variable condition;
  std::mutex mutex;

  // prepare wrapper
  auto wrapper_lambda = [&]() {
    // execute task
    task(context);
    // set ready signal
    std::unique_lock<std::mutex> ready_lock(mutex);
    ready = true;
    ready_lock.unlock();
    condition.notify_one();
  };

  auto wrapper = [](void *context) {
    (*static_cast<decltype(wrapper_lambda) *>(context))();
    return FALSE;
  };

  // execute wrapper
  TRACEA(info, "dispatching task on main queue");
  PostMessageW(_audience_message_window, WM_AUDIENCE_DISPATCH, (WPARAM)task, (LPARAM)context);

  // wait for ready signal
  std::unique_lock<std::mutex> wait_lock(mutex);
  condition.wait(wait_lock, [&] { return ready; });
}

LRESULT CALLBACK MessageWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  // handle dispatched task
  if (message == WM_AUDIENCE_DISPATCH)
  {
    auto task = reinterpret_cast<void (*)(void *)>(wParam);
    auto context = reinterpret_cast<void *>(lParam);
    task(context);
    return 0;
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
}

#define KEY_FEATURE_BROWSER_EMULATION \
  L"Software\\Microsoft\\Internet "   \
  L"Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION"

bool fix_ie_compat_mode()
{
  HKEY hk = nullptr;
  DWORD ie_version = 11001;
  WCHAR exe_path[MAX_PATH + 1] = {0};
  WCHAR *exe_name = nullptr;
  if (GetModuleFileNameW(NULL, exe_path, MAX_PATH + 1) == 0)
  {
    return false;
  }
  for (exe_name = &exe_path[wcslen(exe_path) - 1]; exe_name != exe_path && *exe_name != '\\'; exe_name--)
  {
  }
  exe_name++;
  if (RegCreateKeyW(HKEY_CURRENT_USER, KEY_FEATURE_BROWSER_EMULATION, &hk) != ERROR_SUCCESS)
  {
    return false;
  }
  if (RegSetValueExW(hk, exe_name, 0, REG_DWORD, (BYTE *)&ie_version, sizeof(ie_version)) != ERROR_SUCCESS)
  {
    RegCloseKey(hk);
    return false;
  }
  RegCloseKey(hk);
  return true;
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_NCCREATE:
  {
    // install context as user data
    auto context = reinterpret_cast<AudienceWindowContext *>(((CREATESTRUCT *)lParam)->lpCreateParams);
    if (context != nullptr)
    {
      (*context)->window = window;
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) new AudienceWindowContext(*context));
      TRACEA(info, "private context installed in GWLP_USERDATA");
    }
    else
    {
      TRACEA(error, "context invalid");
    }
  }
  break;

  case WM_CREATE:
  {
    // install timer for title updates
    SetTimer(window, 0x1, 1000, nullptr);
  }
  break;

  case WM_COMMAND:
  {
    // handle menu actions
    switch (LOWORD(wParam))
    {
    case IDM_EXIT:
      DestroyWindow(window);
      return 0;
    }
  }
  break;

  case WM_SIZE:
  {
    // resize web widget
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview != nullptr)
    {
      (*context_priv)->webview->UpdateWebViewPosition();
    }
    else
    {
      TRACEA(warning, "private context invalid");
    }
  }
  break;

  case WM_TIMER:
  {
    // update application title
    switch (wParam)
    {
    case 0x1:
    {
      auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
      if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview != nullptr)
      {
        auto doctitle = (*context_priv)->webview->GetDocumentTitle();
        SetWindowTextW(window, doctitle.c_str());
      }
      else
      {
        TRACEA(error, "private context invalid");
      }
    }
    break;
    }
  }
  break;

  case WM_CLOSE:
  {
    bool prevent_close = false;

    // trigger event
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      internal_on_window_will_close(*context_priv, prevent_close);
    }
    else
    {
      TRACEA(error, "private context invalid");
    }

    // close window
    if (!prevent_close)
    {
      DestroyWindow(window);
    }

    return 0;
  }
  break;

  case WM_DESTROY:
  {
    bool prevent_quit = false;

    // clear timer for title updates
    KillTimer(window, 0x1);

    // clean up installed context
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      // trigger event
      internal_on_window_close(*context_priv, prevent_quit);

      // reset referenced window and widget
      if ((*context_priv)->webview != nullptr)
      {
        (*context_priv)->webview->Release();
        (*context_priv)->webview = nullptr;
      }
      (*context_priv)->window = nullptr;

      // remove private context from window user data
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) nullptr);
      delete context_priv;

      TRACEA(info, "private context removed from GWLP_USERDATA");
    }
    else
    {
      TRACEA(error, "context invalid");
    }

    // trigger further events
    if (!prevent_quit)
    {
      // trigger process will quit event
      prevent_quit = false;
      internal_on_process_will_quit(prevent_quit);

      // quit message loop
      if (!prevent_quit)
      {
        TRACEA(info, "posting WM_QUIT");
        PostQuitMessage(0);
      }
    }
  }
  break;
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
}
