#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <roapi.h>
#include <crtdbg.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <memory>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"

#include "resource.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::UI;
using namespace winrt::Windows::Web::UI::Interop;

struct WindowHandle
{
  HWND window;
  IWebViewControl webview;

  WindowHandle() : window(nullptr),
                   webview(nullptr)
  {
  }
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Rect GetWebViewTargetPosition(const std::shared_ptr<WindowHandle> &handle);
void UpdateWebViewPosition(const std::shared_ptr<WindowHandle> &handle);

HINSTANCE hInstanceEXE = nullptr;
HINSTANCE hInstanceDLL = nullptr;

bool audience_inner_init()
{
  try
  {
    // initialize COM
    auto r = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (r != S_OK && r != S_FALSE)
    {
      return false;
    }

    // test if required COM objects are available
    WebViewControlProcess().GetWebViewControls().Size();

    return true;
  }
  catch (hresult_error const &ex)
  {
    _RPTW1(_CRT_ERROR, L"a COM exception occured: %Ts\n", ex.message().c_str());
  }
  catch (std::exception const &ex)
  {
    _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
  }
  catch (...)
  {
    _RPT0(_CRT_ERROR, "an unknown exception occured\n");
  }
  return false;
}

void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url)
{
  try
  {
    // register window class
    WNDCLASSEXW wndcls;

    wndcls.cbSize = sizeof(WNDCLASSEX);
    wndcls.style = CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc = WndProc;
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
      return nullptr;
    }

    // create window
    std::shared_ptr<WindowHandle> handle = std::make_shared<WindowHandle>();

    HWND window = CreateWindowW(wndcls.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstanceEXE, &handle);
    if (!window)
    {
      return nullptr;
    }

    // create browser widget
    try
    {
      std::wstring url_copy = url;
      WebViewControlProcess()
          .CreateWebViewControlAsync((std::uint64_t)window, GetWebViewTargetPosition(handle))
          .Completed(
              [handle, url_copy](auto &&sender, AsyncStatus const status) {
                try
                {
                  if (status == AsyncStatus::Completed)
                  {
                    handle->webview = sender.GetResults();
                    UpdateWebViewPosition(handle);
                    handle->webview.Navigate(Uri(hstring(url_copy)));
                  }
                }
                catch (hresult_error const &ex)
                {
                  _RPTW1(_CRT_ERROR, L"a COM exception occured: %Ts\n", ex.message().c_str());
                }
                catch (std::exception const &ex)
                {
                  _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
                }
                catch (...)
                {
                  _RPT0(_CRT_ERROR, "an unknown exception occured\n");
                }
              });
    }
    catch (hresult_error const &ex)
    {
      _RPTW1(_CRT_ERROR, L"a COM exception occured: %Ts\n", ex.message().c_str());

      // clean up and abort
      DestroyWindow(window);
      return nullptr;
    }

    // install timer for title updates
    SetTimer(window, 0x1, 1000, nullptr);

    // show window
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    // return handle
    return new std::shared_ptr<WindowHandle>(handle);
  }
  catch (std::exception const &ex)
  {
    _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
  }
  catch (...)
  {
    _RPT0(_CRT_ERROR, "an unknown exception occured\n");
  }

  return nullptr;
}

void audience_inner_window_destroy(void *vhandle)
{
  try
  {
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(vhandle);
    if ((*handle)->window != nullptr)
    {
      DestroyWindow((*handle)->window);
    }
    delete handle;
  }
  catch (std::exception const &ex)
  {
    _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
  }
  catch (...)
  {
    _RPT0(_CRT_ERROR, "an unknown exception occured\n");
  }
}

BOOL WINAPI DllMain(_In_ HINSTANCE hinstDLL, _In_ DWORD fdwReason, _In_ LPVOID lpvReserved)
{
  hInstanceEXE = GetModuleHandleW(NULL);
  hInstanceDLL = hinstDLL;
  return true;
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_CREATE:
  {
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(((CREATESTRUCT *)lParam)->lpCreateParams);
    if (handle)
    {
      (*handle)->window = window;
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) new std::shared_ptr<WindowHandle>(*handle));
    }
    else
    {
      _RPT0(_CRT_ERROR, "handle is zero\n");
    }
  }
  break;

  case WM_COMMAND:
  {
    switch (LOWORD(wParam))
    {
    case IDM_EXIT:
      DestroyWindow(window);
      break;
    default:
      return DefWindowProcW(window, message, wParam, lParam);
    }
  }
  break;

  case WM_SIZE:
  {
    try
    {
      auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
      if (handle)
      {
        UpdateWebViewPosition(*handle);
      }
      else
      {
        _RPT0(_CRT_ERROR, "handle is zero\n");
      }
    }
    catch (hresult_error const &ex)
    {
      _RPTW1(_CRT_ERROR, L"a COM exception occured: %Ts\n", ex.message().c_str());
    }
    catch (std::exception const &ex)
    {
      _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
    }
    catch (...)
    {
      _RPT0(_CRT_ERROR, "an unknown exception occured\n");
    }
  }
  break;

  case WM_TIMER:
  {
    switch (wParam)
    {
    case 0x1:
    {
      try
      {
        auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (handle && (*handle)->webview)
        {
          auto doctitle = (*handle)->webview.DocumentTitle();
          SetWindowTextW(window, doctitle.c_str());
        }
        else
        {
          _RPT0(_CRT_ERROR, "handle is zero\n");
        }
      }
      catch (hresult_error const &ex)
      {
        _RPTW1(_CRT_ERROR, L"a COM exception occured: %Ts\n", ex.message().c_str());
      }
      catch (std::exception const &ex)
      {
        _RPT1(_CRT_ERROR, "an exception occured %Ts\n", ex.what());
      }
      catch (...)
      {
        _RPT0(_CRT_ERROR, "an unknown exception occured\n");
      }
    }
    break;
    }
  }
  break;

  case WM_DESTROY:
  {
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (handle)
    {
      (*handle)->window = nullptr;
      delete handle;
    }
    else
    {
      _RPT0(_CRT_ERROR, "handle is zero\n");
    }
    PostQuitMessage(0);
  }
  break;

  default:
    return DefWindowProcW(window, message, wParam, lParam);
  }

  return 0;
}

Rect GetWebViewTargetPosition(const std::shared_ptr<WindowHandle> &handle)
{
  RECT rect;
  if (!GetClientRect(handle->window, &rect))
  {
    return winrt::Windows::Foundation::Rect(0, 0, 0, 0);
  }
  return winrt::Windows::Foundation::Rect(0, 0, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
}

void UpdateWebViewPosition(const std::shared_ptr<WindowHandle> &handle)
{
  if (handle->webview)
  {
    auto rect = GetWebViewTargetPosition(handle);
    handle->webview.as<IWebViewControlSite>().Bounds(rect);
  }
}
