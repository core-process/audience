#include <windows.h>
#include <roapi.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.UI.Interop.h>

#include <memory>

#include "edge_handle.h"

#include "../../common/trace.h"
#include "../interface.h"
#include "common.h"
#include "resource.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::UI;
using namespace winrt::Windows::Web::UI::Interop;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

Rect GetWebViewTargetPosition(const AudienceHandle &handle);
void UpdateWebViewPosition(const AudienceHandle &handle);

bool internal_init()
{
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

AudienceHandle *internal_window_create(const std::wstring &title, const std::wstring &url)
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
  AudienceHandle handle = std::make_shared<AudienceHandleData>();

  HWND window = CreateWindowW(wndcls.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstanceEXE, &handle);
  if (!window)
  {
    return nullptr;
  }

  // create browser widget
  try
  {
    WebViewControlProcess()
        .CreateWebViewControlAsync((std::uint64_t)window, GetWebViewTargetPosition(handle))
        .Completed(
            [handle, url](auto &&sender, AsyncStatus const status) {
              try
              {
                if (status == AsyncStatus::Completed)
                {
                  // update handle
                  handle->webview = sender.GetResults();

                  // update position of web view
                  UpdateWebViewPosition(handle);

                  // navigate to initial URL
                  handle->webview.Navigate(Uri(hstring(url)));

                  TRACEA(info, "web widget created successfully");
                }
                else
                {
                  TRACEA(error, "web widget could not be created");
                }
              }
              catch (const hresult_error &ex)
              {
                TRACEW(error, L"a COM exception occured: " << ex.message().c_str());
              }
              catch (const std::exception &ex)
              {
                TRACEA(error, "an exception occured: " << ex.what());
              }
              catch (...)
              {
                TRACEA(error, "an unknown exception occured");
              }
            });
  }
  catch (const hresult_error &ex)
  {
    TRACEW(error, L"a COM exception occured: " << ex.message().c_str());

    // clean up and abort
    DestroyWindow(window);
    return nullptr;
  }

  // show window
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  TRACEA(info, "window created successfully");

  // return handle
  return new AudienceHandle(handle);
}

void internal_window_destroy(AudienceHandle *handle)
{
  // destroy window
  if ((*handle)->window != nullptr)
  {
    DestroyWindow((*handle)->window);
    TRACEA(info, "window destroyed");
  }

  // delete handle
  delete handle;
  TRACEA(info, "handle deleted");
}

void internal_loop()
{
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_NCCREATE:
  {
    // install handle as user data
    auto handle = reinterpret_cast<AudienceHandle *>(((CREATESTRUCT *)lParam)->lpCreateParams);
    if (handle != nullptr)
    {
      (*handle)->window = window;
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) new AudienceHandle(*handle));
      TRACEA(info, "handle installed in GWLP_USERDATA");
    }
    else
    {
      TRACEA(error, "handle invalid");
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
    try
    {
      auto handle = reinterpret_cast<AudienceHandle *>(GetWindowLongPtrW(window, GWLP_USERDATA));
      if (handle != nullptr && (*handle)->window != nullptr && (*handle)->webview)
      {
        UpdateWebViewPosition(*handle);
      }
      else
      {
        TRACEA(warning, "handle invalid");
      }
    }
    catch (const hresult_error &ex)
    {
      TRACEW(error, L"a COM exception occured: " << ex.message().c_str());
    }
    catch (const std::exception &ex)
    {
      TRACEA(error, "an exception occured: " << ex.what());
    }
    catch (...)
    {
      TRACEA(error, "an unknown exception occured");
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
      try
      {
        auto handle = reinterpret_cast<AudienceHandle *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (handle != nullptr && (*handle)->window != nullptr && (*handle)->webview)
        {
          auto doctitle = (*handle)->webview.DocumentTitle();
          SetWindowTextW(window, doctitle.c_str());
        }
        else
        {
          TRACEA(error, "handle invalid");
        }
      }
      catch (const hresult_error &ex)
      {
        TRACEW(error, L"a COM exception occured: " << ex.message().c_str());
      }
      catch (const std::exception &ex)
      {
        TRACEA(error, "an exception occured: " << ex.what());
      }
      catch (...)
      {
        TRACEA(error, "an unknown exception occured");
      }
    }
    break;
    }
  }
  break;

  case WM_DESTROY:
  {
    // clear timer for title updates
    KillTimer(window, 0x1);

    // clean up installed handle
    auto handle = reinterpret_cast<AudienceHandle *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (handle != nullptr)
    {
      // reset referenced window and widget
      (*handle)->webview = nullptr;
      (*handle)->window = nullptr;

      // remove handle from window user data
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) nullptr);
      delete handle;

      TRACEA(info, "handle removed from GWLP_USERDATA");
    }
    else
    {
      TRACEA(error, "handle invalid");
    }

    // quit message loop
    PostQuitMessage(0);
  }
  break;
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
}

Rect GetWebViewTargetPosition(const AudienceHandle &handle)
{
  RECT rect;
  if (!GetClientRect(handle->window, &rect))
  {
    TRACEA(error, "could not retrieve window client rect");
    return winrt::Windows::Foundation::Rect(0, 0, 0, 0);
  }
  return winrt::Windows::Foundation::Rect(0, 0, (float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
}

void UpdateWebViewPosition(const AudienceHandle &handle)
{
  auto rect = GetWebViewTargetPosition(handle);
  handle->webview.as<IWebViewControlSite>().Bounds(rect);
}
