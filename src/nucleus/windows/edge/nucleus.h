#pragma once

#include <windows.h>
#include <winrt/Windows.Web.UI.h>
#include <memory>

///////////////////////////////////////////////////////////////////////
// Window Context
///////////////////////////////////////////////////////////////////////

struct AudienceWindowContextData
{
  HWND window;
  winrt::Windows::Web::UI::IWebViewControl webview;

  AudienceWindowContextData() : window(nullptr),
                                webview(nullptr)
  {
  }
};

using AudienceWindowContext = std::shared_ptr<AudienceWindowContextData>;

///////////////////////////////////////////////////////////////////////
// Exception Handling
///////////////////////////////////////////////////////////////////////

#define NUCLEUS_TRANSLATE_EXCEPTIONS winrt::hresult_error

inline std::string get_reason_from_exception(const winrt::hresult_error &e)
{
  return winrt::to_string(e.message());
}
