#pragma once

#include <gtk/gtk.h>
#include <memory>

struct AudienceWindowContextData
{
  GtkWidget *window;
  GtkWidget *webview;

  AudienceWindowContextData() : window(nullptr),
                                webview(nullptr)
  {
  }
};

using AudienceWindowContext = std::shared_ptr<AudienceWindowContextData>;
