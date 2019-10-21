#include <memory>
#include <stdexcept>

#include "../../../common/trace.h"
#include "../../../common/scope_guard.h"
#include "../../shared/interface.h"
#include "../shared/init.h"
#include "../shared/resource.h"
#include "webview.h"
#include "nucleus.h"

bool fix_ie_compat_mode();
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

bool internal_init()
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

AudienceHandle *internal_window_create(const std::wstring &title, const std::wstring &url)
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
    return nullptr;
  }

  // create window
  AudienceHandle handle = std::make_shared<AudienceHandleData>();

  HWND window = CreateWindowW(wndcls.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstanceEXE, &handle);
  if (!window)
  {
    return nullptr;
  }

  scope_fail += [window]() { DestroyWindow(window); };

  // create browser widget
  handle->webview = new IEWebView();
  handle->webview->AddRef();

  scope_fail += [handle]() {
    if (handle->webview != nullptr)
    {
      handle->webview->Release();
      handle->webview = nullptr;
    }
  };

  if (!handle->webview->Create(window))
  {
    throw std::runtime_error("creation of web view failed");
  }

  // navigate to url
  handle->webview->Navigate(url.c_str());

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
    auto handle = reinterpret_cast<AudienceHandle *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (handle != nullptr && (*handle)->window != nullptr && (*handle)->webview != nullptr)
    {
      (*handle)->webview->UpdateWebViewPosition();
    }
    else
    {
      TRACEA(warning, "handle invalid");
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
      auto handle = reinterpret_cast<AudienceHandle *>(GetWindowLongPtrW(window, GWLP_USERDATA));
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
