#pragma once

#include <windows.h>
#include <memory>

#include "ie11_webview.h"

struct AudienceHandleData
{
  HWND window;
  IEWebView *webview;

  AudienceHandleData() : window(nullptr),
                         webview(nullptr)
  {
  }
};

using AudienceHandle = std::shared_ptr<AudienceHandleData>;
