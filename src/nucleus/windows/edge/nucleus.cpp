#include <windows.h>
#include <roapi.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <memory>

#include "../../../common/trace.h"
#include "../../../common/scope_guard.h"
#include "../../shared/interface.h"
#include "../shared/init.h"
#include "../shared/resource.h"
#include "nucleus.h"
#include "stream_resolver.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::UI;
using namespace winrt::Windows::Web::UI::Interop;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Rect GetWebViewTargetPosition(const AudienceWindowContext context);
void UpdateWebViewPosition(const AudienceWindowContext context);

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation)
{
  // negotiate protocol
  negotiation->nucleus_handles_webapp_type_directory = true;
  negotiation->nucleus_handles_messaging = true;

  // initialize COM
  auto r = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (r != S_OK && r != S_FALSE)
  {
    return false;
  }

  // test if required COM objects are available (will throw COM exception if not available)
  WebViewControlProcess().GetWebViewControls().Size();

  TRACEA(info, "COM initialization succeeded");
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
  wndcls.lpszClassName = L"audience_edge";

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

  // update position of web view
  UpdateWebViewPosition(context);

  // navigate to initial URL
  auto uri = context->webview.BuildLocalStreamUri(L"webapp", L"index.html");
  context->webview.NavigateToLocalStreamUri(uri, winrt::make<WebViewUriToStreamResolver>(details.webapp_location));

  TRACEA(info, "web widget created successfully");

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
    if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview)
    {
      UpdateWebViewPosition(*context_priv);
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
      if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview)
      {
        auto doctitle = (*context_priv)->webview.DocumentTitle();
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

    // clean up installed private context
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      // trigger event
      internal_on_window_close(*context_priv, prevent_quit);

      // reset referenced window and widget
      (*context_priv)->webview = nullptr;
      (*context_priv)->window = nullptr;

      // remove private context from window user data
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) nullptr);
      delete context_priv;

      TRACEA(info, "private context removed from GWLP_USERDATA");
    }
    else
    {
      TRACEA(error, "private context invalid");
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

Rect GetWebViewTargetPosition(const AudienceWindowContext context)
{
  RECT rect;
  if (!GetClientRect(context->window, &rect))
  {
    TRACEA(error, "could not retrieve window client rect");
    return winrt::Windows::Foundation::Rect(0, 0, 0, 0);
  }
  return winrt::Windows::Foundation::Rect(0, 0, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
}

void UpdateWebViewPosition(const AudienceWindowContext context)
{
  auto rect = GetWebViewTargetPosition(context);
  context->webview.as<IWebViewControlSite>().Bounds(rect);
}
