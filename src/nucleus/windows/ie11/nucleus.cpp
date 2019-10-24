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
    TRACEA(info, "window destroyed");
  }
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

  case WM_DESTROY:
  {
    // clear timer for title updates
    KillTimer(window, 0x1);

    // clean up installed context
    auto context_priv = reinterpret_cast<AudienceWindowContext *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (context_priv != nullptr)
    {
      // trigger event
      internal_on_window_destroyed(*context_priv);

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

    // quit message loop
    PostQuitMessage(0);
  }
  break;
  }

  // execute default window procedure
  return DefWindowProcW(window, message, wParam, lParam);
}
