#pragma once

#include <windows.h>
#include <winrt/Windows.Web.UI.h>
#include <memory>

///////////////////////////////////////////////////////////////////////
// Handle
///////////////////////////////////////////////////////////////////////

struct AudienceHandleData
{
  HWND window;
  winrt::Windows::Web::UI::IWebViewControl webview;

  AudienceHandleData() : window(nullptr),
                         webview(nullptr)
  {
  }
};

using AudienceHandle = std::shared_ptr<AudienceHandleData>;

///////////////////////////////////////////////////////////////////////
// Exception Handling
///////////////////////////////////////////////////////////////////////

#define NUCLEUS_TRANSLATE_EXCEPTIONS winrt::hresult_error

inline std::string get_reason_from_exception(const winrt::hresult_error &e)
{
  return winrt::to_string(e.message());
}
