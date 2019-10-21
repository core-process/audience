#pragma once

#include <gtk/gtk.h>
#include <memory>

struct AudienceHandleData
{
  GtkWidget *window;
  GtkWidget *webview;

  AudienceHandleData() : window(nullptr),
                         webview(nullptr)
  {
  }
};

using AudienceHandle = std::shared_ptr<AudienceHandleData>;
