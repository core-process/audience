#pragma once

#include <windows.h>
#include <memory>

#include "webview.h"

struct AudienceWindowContextData
{
  HWND window;
  IEWebView *webview;

  AudienceWindowContextData() : window(nullptr),
                                webview(nullptr)
  {
  }
};

using AudienceWindowContext = std::shared_ptr<AudienceWindowContextData>;
