#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <chrono>

struct AudienceWindowContextData
{
  GtkWidget *window;
  GtkWidget *webview;

  std::chrono::time_point<std::chrono::steady_clock> last_positioning; // programatically, not by user...
  AudienceRect last_positioning_data;

  AudienceWindowContextData() : window(nullptr),
                                webview(nullptr),
                                last_positioning{},
                                last_positioning_data{}
  {
  }
};

using AudienceWindowContext = std::shared_ptr<AudienceWindowContextData>;
