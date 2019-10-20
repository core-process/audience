#pragma once

#include <windows.h>
#include <winrt/Windows.Web.UI.h>
#include <memory>

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
