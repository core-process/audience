#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <string>
#include <locale>
#include <codecvt>
#include <memory>

#include "../../../common/trace.h"
#include "../../../common/scope_guard.h"
#include "../../shared/interface.h"
#include "nucleus.h"

#define WIDGET_HANDLE_KEY "audience_window_handle"

void widget_destroy_callback(GtkWidget *widget, gpointer arg);
void webview_title_update_callback(GtkWidget *widget, gpointer arg);

bool internal_init()
{
  if (gtk_init_check(0, NULL) == FALSE)
  {
    return false;
  }

  return true;
}

AudienceHandle *internal_window_create(const std::wstring &title, const std::wstring &url)
{
  scope_guard scope_fail(scope_guard::execution::exception);

  // create handle instance
  auto handle = std::make_shared<AudienceHandleData>();

  // convert input parameter
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  auto titlea = converter.to_bytes(title);
  auto urla = converter.to_bytes(url);

  // retrieve screen dimensions
  GdkRectangle workarea = {0};
  gdk_monitor_get_workarea(
      gdk_display_get_primary_monitor(gdk_display_get_default()),
      &workarea);

  // create window
  handle->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  if (handle->window == nullptr)
  {
    throw std::runtime_error("could not create window");
  }

  scope_fail += [handle]() {
    if (handle->window != nullptr)
    {
      gtk_widget_destroy(GTK_WIDGET(handle->window));
      handle->window = nullptr;
    }
  };

  gtk_window_set_title(GTK_WINDOW(handle->window), titlea.c_str());
  gtk_window_set_default_size(GTK_WINDOW(handle->window), workarea.width / 2, workarea.height / 2);
  gtk_window_set_resizable(GTK_WINDOW(handle->window), true);
  gtk_window_set_position(GTK_WINDOW(handle->window), GTK_WIN_POS_CENTER);

  // create webview
  handle->webview = webkit_web_view_new();

  if (handle->webview == nullptr)
  {
    throw std::runtime_error("could not create webview");
  }

  gtk_container_add(GTK_CONTAINER(handle->window), GTK_WIDGET(handle->webview));

  // create handle instance for window and widget
  auto handle_priv = new AudienceHandle(handle);
  g_object_set_data(G_OBJECT(handle->window), WIDGET_HANDLE_KEY, handle_priv);
  g_object_set_data(G_OBJECT(handle->webview), WIDGET_HANDLE_KEY, handle_priv);

  // listen to destroy signal and title changed event
  g_signal_connect(G_OBJECT(handle->window), "destroy", G_CALLBACK(NUCLEUS_SAFE_FN(widget_destroy_callback)), nullptr);
  g_signal_connect(G_OBJECT(handle->webview), "destroy", G_CALLBACK(NUCLEUS_SAFE_FN(widget_destroy_callback)), nullptr);
  g_signal_connect(G_OBJECT(handle->webview), "notify::title", G_CALLBACK(NUCLEUS_SAFE_FN(webview_title_update_callback)), nullptr);

  // show window and trigger url load
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(handle->webview), urla.c_str());
  gtk_widget_show_all(GTK_WIDGET(handle->window));

  TRACEA(info, "window created");

  return new AudienceHandle(handle);
}

void internal_window_destroy(AudienceHandle *handle)
{
  if ((*handle)->window != nullptr)
  {
    gtk_window_close(GTK_WINDOW((*handle)->window));
  }
  delete handle;
  TRACEA(info, "window public handle released");
}

void internal_loop()
{
  gtk_main();
}

void widget_destroy_callback(GtkWidget *widget, gpointer arg)
{
  auto handle_priv = reinterpret_cast<AudienceHandle *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (handle_priv != nullptr)
  {
    if ((*handle_priv)->window != nullptr)
    {
      g_object_set_data(G_OBJECT((*handle_priv)->window), WIDGET_HANDLE_KEY, nullptr);
      (*handle_priv)->window = nullptr;
    }
    if ((*handle_priv)->webview)
    {
      g_object_set_data(G_OBJECT((*handle_priv)->webview), WIDGET_HANDLE_KEY, nullptr);
      (*handle_priv)->webview = nullptr;
    }
    delete handle_priv;
    TRACEA(info, "window closed and private handle released");
  }
  gtk_main_quit();
}

void webview_title_update_callback(GtkWidget *widget, gpointer arg)
{
  auto handle_priv = reinterpret_cast<AudienceHandle *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (handle_priv != nullptr)
  {
    if ((*handle_priv)->window != nullptr && (*handle_priv)->webview != nullptr)
    {
      auto title = webkit_web_view_get_title(WEBKIT_WEB_VIEW((*handle_priv)->webview));
      if (title != nullptr)
      {
        gtk_window_set_title(GTK_WINDOW((*handle_priv)->window), title);
      }
    }
  }
}
