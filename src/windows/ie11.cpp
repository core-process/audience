#include <memory>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"
#include "ie11_webview.h"
#include "common.h"
#include "resource.h"
#include "../trace.h"

struct WindowHandle
{
  HWND window;
  IEWebView *webview;

  WindowHandle() : window(nullptr),
                   webview(nullptr)
  {
  }
};

bool fix_ie_compat_mode();
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool audience_inner_init()
{
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
  return true;
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
    wndcls.lpszClassName = L"audience_ie11";

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
    handle->webview = new IEWebView();
    handle->webview->AddRef();

    if (!handle->webview->Create(window))
    {
      // delete webview
      handle->webview->Release();
      handle->webview = nullptr;

      // destroy window
      DestroyWindow(window);

      return nullptr;
    }

    // navigate to url
    handle->webview->Navigate(url);

    // show window
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    TRACEA(info, "window created successfully");

    // return handle
    return new std::shared_ptr<WindowHandle>(handle);
  }
  catch (const std::exception &ex)
  {
    TRACEA(error, "an exception occured: " << ex.what());
  }
  catch (...)
  {
    TRACEA(error, "an unknown exception occured");
  }

  return nullptr;
}

void audience_inner_window_destroy(void *vhandle)
{
  try
  {
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(vhandle);

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
  catch (const std::exception &ex)
  {
    TRACEA(error, "an exception occured: " << ex.what());
  }
  catch (...)
  {
    TRACEA(error, "an unknown exception occured");
  }
}

#define KEY_FEATURE_BROWSER_EMULATION \
  L"Software\\Microsoft\\Internet "   \
  L"Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION"

bool fix_ie_compat_mode()
{
  HKEY hk = nullptr;
  DWORD ie_version = 11000;
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
    // install handle as user data
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(((CREATESTRUCT *)lParam)->lpCreateParams);
    if (handle != nullptr)
    {
      (*handle)->window = window;
      SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR) new std::shared_ptr<WindowHandle>(*handle));
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
      auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
      if (handle != nullptr && (*handle)->window != nullptr && (*handle)->webview != nullptr)
      {
        (*handle)->webview->UpdateWebViewPosition();
      }
      else
      {
        TRACEA(warning, "handle invalid");
      }
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
        auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (handle != nullptr && (*handle)->window != nullptr && (*handle)->webview != nullptr)
        {
          auto doctitle = (*handle)->webview->GetDocumentTitle();
          SetWindowTextW(window, doctitle.c_str());
        }
        else
        {
          TRACEA(error, "handle invalid");
        }
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
    auto handle = reinterpret_cast<std::shared_ptr<WindowHandle> *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (handle != nullptr)
    {
      // reset referenced window and widget
      if ((*handle)->webview != nullptr)
      {
        (*handle)->webview->Release();
        (*handle)->webview = nullptr;
      }
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
