#include <windows.h>
#include <roapi.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <memory>
#include <atomic>
#include <spdlog/spdlog.h>

#include "../../../common/scope_guard.h"
#include "../../shared/interface.h"
#include "../shared/load.h"
#include "../shared/icons.h"
#include "nucleus.h"
#include "stream_resolver.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::UI;
using namespace winrt::Windows::Web::UI::Interop;

#define AUDIENCE_WINDOW_CLASSNAME L"audience_edge"
#define AUDIENCE_MESSAGE_WINDOW_CLASSNAME L"audience_edge_message"

#define WM_AUDIENCE_DISPATCH (WM_APP + 1)
#define WM_AUDIENCE_DESTROY_WINDOW (WM_APP + 2)

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MessageWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Rect GetWebViewTargetPosition(const AudienceWindowContext context);
void UpdateWebViewPosition(const AudienceWindowContext context);

std::atomic<HWND> _audience_message_window = nullptr;

bool nucleus_impl_init(AudienceNucleusProtocolNegotiation &negotiation, const NucleusImplAppDetails &details)
{
  // negotiate protocol
  negotiation.nucleus_handles_webapp_type_directory = true;
  negotiation.nucleus_handles_messaging = true;

  // initialize COM
  auto r = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (r != S_OK && r != S_FALSE)
  {
    SPDLOG_ERROR("CoInitializeEx() failed");
    return false;
  }

  // test if required COM objects are available (will throw COM exception if not available)
  WebViewControlProcess().GetWebViewControls().Size();
  SPDLOG_INFO("COM initialization succeeded");

  // create message window
  WNDCLASSEXW wndcls_msg = {};
  wndcls_msg.cbSize = sizeof(WNDCLASSEX);
  wndcls_msg.lpfnWndProc = NUCLEUS_SAFE_FN(MessageWndProc, SAFE_FN_DEFAULT(LRESULT));
  wndcls_msg.hInstance = hInstanceEXE;
  wndcls_msg.lpszClassName = AUDIENCE_MESSAGE_WINDOW_CLASSNAME;

  if (RegisterClassExW(&wndcls_msg) == 0)
  {
    SPDLOG_ERROR("RegisterClassExW() for message window failed");
    return false;
  }

  _audience_message_window = CreateWindowExW(0, AUDIENCE_MESSAGE_WINDOW_CLASSNAME, AUDIENCE_MESSAGE_WINDOW_CLASSNAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
  if (!_audience_message_window.load())
  {
    SPDLOG_ERROR("CreateWindowExW() for message window failed");
    return false;
  }

  // load icon handles
  HICON small_icon = nullptr, large_icon = nullptr;
  load_icon_handles(details, small_icon, large_icon);

  // register window class
  WNDCLASSEXW wndcls;

  wndcls.cbSize = sizeof(WNDCLASSEX);
  wndcls.style = CS_HREDRAW | CS_VREDRAW;
  wndcls.lpfnWndProc = NUCLEUS_SAFE_FN(WndProc, SAFE_FN_DEFAULT(LRESULT));
  wndcls.cbClsExtra = 0;
  wndcls.cbWndExtra = 0;
  wndcls.hInstance = hInstanceEXE;
  wndcls.hIcon = large_icon;
  wndcls.hIconSm = small_icon;
  wndcls.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wndcls.lpszMenuName = 0;
  wndcls.lpszClassName = AUDIENCE_WINDOW_CLASSNAME;

  if (RegisterClassExW(&wndcls) == 0)
  {
    SPDLOG_ERROR("RegisterClassExW() for main window failed");
    return false;
  }

  SPDLOG_INFO("initialized");
  return true;
}

AudienceScreenList nucleus_impl_screen_list()
{
  AudienceScreenList result{};

  HMONITOR primary_monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
  HMONITOR focused_monitor = nullptr;

  HWND foreground_window = GetForegroundWindow();
  if (foreground_window != nullptr)
  {
    focused_monitor = MonitorFromWindow(foreground_window, MONITOR_DEFAULTTONEAREST);
  }

  auto monitor_enum_proc_lambda = [&](
                                      HMONITOR hMonitor,
                                      HDC hdcMonitor,
                                      LPRECT lprcMonitor) {
    if (result.count >= AUDIENCE_SCREEN_LIST_ENTRIES)
    {
      return;
    }

    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);

    if (!GetMonitorInfoW(hMonitor, &monitor_info))
    {
      SPDLOG_ERROR("could not retrieve info of monitor... skipping");
      return;
    }

    if (hMonitor == primary_monitor)
    {
      result.primary = result.count;
    }

    if (hMonitor == focused_monitor)
    {
      result.focused = result.count;
    }

    result.screens[result.count].frame = {{(double)monitor_info.rcMonitor.left, (double)monitor_info.rcMonitor.top},
                                          {(double)monitor_info.rcMonitor.right - (double)monitor_info.rcMonitor.left, (double)monitor_info.rcMonitor.bottom - (double)monitor_info.rcMonitor.top}};

    result.screens[result.count].workspace = {{(double)monitor_info.rcWork.left, (double)monitor_info.rcWork.top},
                                              {(double)monitor_info.rcWork.right - (double)monitor_info.rcWork.left, (double)monitor_info.rcWork.bottom - (double)monitor_info.rcWork.top}};

    result.count += 1;
  };

  auto monitor_enum_proc = [](
                               HMONITOR hMonitor,
                               HDC hdcMonitor,
                               LPRECT lprcMonitor,
                               LPARAM dwData) {
    (*static_cast<decltype(monitor_enum_proc_lambda) *>((void *)dwData))(hMonitor, hdcMonitor, lprcMonitor);
    return TRUE;
  };

  EnumDisplayMonitors(NULL, NULL, (BOOL(*)(HMONITOR, HDC, LPRECT, LPARAM))monitor_enum_proc, (LPARAM)&monitor_enum_proc_lambda);

  return result;
}

AudienceWindowContext nucleus_impl_window_create(const NucleusImplWindowDetails &details)
{
  scope_guard scope_fail(scope_guard::execution::exception);

  // create window
  AudienceWindowContext context = std::make_shared<AudienceWindowContextData>();

  HWND window = CreateWindowW(AUDIENCE_WINDOW_CLASSNAME, details.loading_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstanceEXE, &context);
  if (!window)
  {
    return {};
  }

  scope_fail += [window]() { DestroyWindow(window); };

  // create browser widget
  auto webview_op = WebViewControlProcess().CreateWebViewControlAsync((std::uint64_t)window, GetWebViewTargetPosition(context));

  if (webview_op.Status() == AsyncStatus::Started)
  {
    auto event = CreateEventW(nullptr, false, false, nullptr);
    webview_op.Completed([event](auto, auto) { SetEvent(event); });
    DWORD lpdwindex = 0;
    if (CoWaitForMultipleHandles(
            COWAIT_DISPATCH_WINDOW_MESSAGES | COWAIT_DISPATCH_CALLS | COWAIT_INPUTAVAILABLE,
            INFINITE, 1, &event, &lpdwindex) != S_OK)
    {
      throw std::runtime_error("CoWaitForMultipleHandles failed");
    }
  }

  if (webview_op.Status() != AsyncStatus::Completed)
  {
    throw std::runtime_error("creation of web view failed");
  }

  context->webview = webview_op.GetResults();

  // settings
  context->webview.Settings().IsIndexedDBEnabled(true);
  context->webview.Settings().IsJavaScriptEnabled(true);
  context->webview.Settings().IsScriptNotifyAllowed(true);

  // add integration hooks
  context->webview.AddInitializeScript(L"window._audienceWebviewSignature = 'edge';");
  context->webview.ScriptNotify([context](auto sender, auto args) {
    emit_window_message(context, std::wstring(args.Value()));
  });

  // set window styles
  if (details.styles.not_decorated)
  {
    LONG window_style = GetWindowLongPtr(context->window, GWL_STYLE) & ~WS_CAPTION;
    if (window_style != 0)
    {
      SetWindowLongPtr(context->window, GWL_STYLE, window_style);
      SetWindowPos(context->window, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_DRAWFRAME);
    }
    else
    {
      SPDLOG_ERROR("GetWindowLongPtr failed");
    }
  }

  if (details.styles.not_resizable)
  {
    LONG window_style = GetWindowLongPtr(context->window, GWL_STYLE) & ~(WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    if (window_style != 0)
    {
      SetWindowLongPtr(context->window, GWL_STYLE, window_style);
      SetWindowPos(context->window, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_DRAWFRAME);
    }
    else
    {
      SPDLOG_ERROR("GetWindowLongPtr failed");
    }
  }

  if (details.styles.always_on_top)
  {
    SetWindowPos(context->window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  }

  // position window
  if (details.position.size.width > 0 && details.position.size.height > 0)
  {
    nucleus_impl_window_update_position(context, details.position);
  }

  // update position of web view
  UpdateWebViewPosition(context);

  // navigate to initial URL
  auto uri = context->webview.BuildLocalStreamUri(L"webapp", L"index.html");
  context->webview.NavigateToLocalStreamUri(uri, winrt::make<WebViewUriToStreamResolver>(details.webapp_location));

  SPDLOG_INFO("web widget created successfully");

  // show window
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);
  SetActiveWindow(window);

  SPDLOG_INFO("window created successfully");
  return context;
}

NucleusImplWindowStatus
nucleus_impl_window_status(AudienceWindowContext context)
{
  NucleusImplWindowStatus result{};

  result.has_focus = GetForegroundWindow() == context->window;

  RECT rect{};
  if (!GetWindowRect(context->window, &rect))
  {
    SPDLOG_ERROR("could not retrieve window rect");
  }
  else
  {
    result.frame = {
        {(double)rect.left, (double)rect.top},
        {(double)rect.right - (double)rect.left, (double)rect.bottom - (double)rect.top}};
  }

  if (!GetClientRect(context->window, &rect))
  {
    SPDLOG_ERROR("could not retrieve client rect");
  }
  else
  {
    result.workspace = {(double)rect.right - (double)rect.left, (double)rect.bottom - (double)rect.top};
  }

  return result;
}

void nucleus_impl_window_update_position(AudienceWindowContext context,
                                         AudienceRect position)
{
  SPDLOG_DEBUG("window_update_position: origin={},{} size={}x{}", position.origin.x, position.origin.y, position.size.width, position.size.height);

  if (!MoveWindow(context->window, position.origin.x, position.origin.y, position.size.width, position.size.height, TRUE))
  {
    SPDLOG_ERROR("could not move window");
  }
}

void nucleus_impl_window_post_message(AudienceWindowContext context, const std::wstring &message)
{
  auto op = context->webview.InvokeScriptAsync(L"_audienceWebviewMessageHandler", std::vector<winrt::hstring>{winrt::hstring(message)});

  if (op.Status() == AsyncStatus::Started)
  {
    auto event = CreateEventW(nullptr, false, false, nullptr);
    op.Completed([event](auto, auto) { SetEvent(event); });
    DWORD lpdwindex = 0;
    if (CoWaitForMultipleHandles(
            COWAIT_DISPATCH_WINDOW_MESSAGES | COWAIT_DISPATCH_CALLS | COWAIT_INPUTAVAILABLE,
            INFINITE, 1, &event, &lpdwindex) != S_OK)
    {
      throw std::runtime_error("CoWaitForMultipleHandles failed");
    }
  }

  if (op.Status() != AsyncStatus::Completed)
  {
    throw std::runtime_error("execution of javascript failed");
  }
}

void nucleus_impl_window_destroy(AudienceWindowContext context)
{
  // destroy window
  SPDLOG_INFO("delaying call of DestroyWindow()");
  PostMessageW(_audience_message_window.load(), WM_AUDIENCE_DESTROY_WINDOW, (WPARAM) new AudienceWindowContext(context), 0);
}

void nucleus_impl_quit()
{
  SPDLOG_INFO("delaying call of PostQuitMessage()");
  SetTimer(_audience_message_window.load(), 1, 100, nullptr);
}

void nucleus_impl_main()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // trigger final event
  emit_app_quit();

  // lets quit now
  SPDLOG_INFO("calling ExitProcess()");
  ExitProcess(0);
}

void nucleus_impl_dispatch_sync(void (*task)(void *context), void *context)
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
  };

  // execute wrapper
  SPDLOG_INFO("dispatching task on main queue (sync)");
  PostMessageW(_audience_message_window.load(), WM_AUDIENCE_DISPATCH, (WPARAM)(void (*)(void *))wrapper, (LPARAM)&wrapper_lambda);

  // wait for ready signal
  std::unique_lock<std::mutex> wait_lock(mutex);
  condition.wait(wait_lock, [&] { return ready; });
}

void nucleus_impl_dispatch_async(void (*task)(void *context), void *context)
{
  SPDLOG_INFO("dispatching task on main queue (async)");
  PostMessageW(_audience_message_window.load(), WM_AUDIENCE_DISPATCH, (WPARAM)task, (LPARAM)context);
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
  else if (message == WM_AUDIENCE_DESTROY_WINDOW)
  {
    auto context = reinterpret_cast<AudienceWindowContext*>(wParam);
    if ((*context)->window != nullptr)
    {
      SPDLOG_INFO("calling DestroyWindow()");
      DestroyWindow((*context)->window);
    }
    delete context;
    return 0;
  }
  else if (message == WM_TIMER && wParam == 1)
  {
    size_t window_count = 0;
    bool enum_success = EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
          size_t &window_count = *reinterpret_cast<size_t *>(lParam);
          DWORD hwnd_pid = 0;
          GetWindowThreadProcessId(hwnd, &hwnd_pid);
          wchar_t hwnd_class[250]{};
          if (GetClassNameW(hwnd, hwnd_class, 250) != 0)
          {
            if (GetCurrentProcessId() == hwnd_pid && IsWindowVisible(hwnd) && std::wstring(hwnd_class) == std::wstring(AUDIENCE_MESSAGE_WINDOW_CLASSNAME))
            {
              window_count += 1;
            }
          }
          return TRUE;
        },
        (LPARAM)&window_count);
    if (!enum_success)
    {
      return 0;
    }
    SPDLOG_DEBUG("found {} windows", window_count);
    if (window_count == 0)
    {
      KillTimer(_audience_message_window.load(), 1);
      SPDLOG_DEBUG("calling PostQuitMessage() now");
      PostQuitMessage(0);
    }
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
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
      SPDLOG_INFO("private context installed in GWLP_USERDATA");
    }
    else
    {
      SPDLOG_ERROR("context invalid");
    }
  }
  break;

  case WM_CREATE:
  {
    // install timer for title updates
    SetTimer(window, 0x1, 1000, nullptr);
  }
  break;

  case WM_SIZE:
  {
    // resize web widget
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview)
    {
      UpdateWebViewPosition(*context_priv);
    }
    else
    {
      SPDLOG_WARN("private context invalid");
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
      if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview)
      {
        auto doctitle = (*context_priv)->webview.DocumentTitle();
        SetWindowTextW(window, doctitle.c_str());
      }
      else
      {
        SPDLOG_ERROR("private context invalid");
      }
    }
    break;
    }
  }
  break;

  case WM_CLOSE:
  {
    // trigger event
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      emit_window_close_intent(*context_priv);
    }
    else
    {
      SPDLOG_ERROR("private context invalid");
      DestroyWindow(window);
    }
    return 0;
  }
  break;

  case WM_DESTROY:
  {
    // clear timer for title updates
    KillTimer(window, 0x1);

    // clean up installed private context
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      // trigger event
      emit_window_close(*context_priv, util_is_only_window(*context_priv));

      // reset referenced window and widget
      (*context_priv)->webview = nullptr;
      (*context_priv)->window = nullptr;

      // remove private context from window user data
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) nullptr);
      delete context_priv;

      SPDLOG_INFO("private context removed from GWLP_USERDATA");
    }
    else
    {
      SPDLOG_ERROR("private context invalid");
    }
  }
  break;
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
}

Rect GetWebViewTargetPosition(const AudienceWindowContext context)
{
  RECT rect;
  if (!GetClientRect(context->window, &rect))
  {
    SPDLOG_ERROR("could not retrieve window client rect");
    return winrt::Windows::Foundation::Rect(0, 0, 0, 0);
  }
  return winrt::Windows::Foundation::Rect(0, 0, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
}

void UpdateWebViewPosition(const AudienceWindowContext context)
{
  auto rect = GetWebViewTargetPosition(context);
  context->webview.as<IWebViewControlSite>().Bounds(rect);
}
