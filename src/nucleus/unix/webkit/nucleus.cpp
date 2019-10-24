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

bool internal_init(AudienceNucleusProtocolNegotiation *negotiation)
{
  // negotiate protocol
  negotiation->nucleus_handles_webapp_type_url = true;

  // init gtk
  if (gtk_init_check(0, NULL) == FALSE)
  {
    return false;
  }

  return true;
}

AudienceWindowContext internal_window_create(const InternalWindowDetails &details)
{
  scope_guard scope_fail(scope_guard::execution::exception);

  // create context instance
  auto context = std::make_shared<AudienceWindowContextData>();

  // convert input parameter
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  auto title = converter.to_bytes(details.loading_title);
  auto url = converter.to_bytes(details.webapp_location);

  // retrieve screen dimensions
  GdkRectangle workarea = {0};
  gdk_monitor_get_workarea(
      gdk_display_get_primary_monitor(gdk_display_get_default()),
      &workarea);

  // create window
  context->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  if (context->window == nullptr)
  {
    throw std::runtime_error("could not create window");
  }

  scope_fail += [context]() {
    if (context->window != nullptr)
    {
      gtk_widget_destroy(GTK_WIDGET(context->window));
      context->window = nullptr;
    }
  };

  gtk_window_set_title(GTK_WINDOW(context->window), title.c_str());
  gtk_window_set_default_size(GTK_WINDOW(context->window), workarea.width / 2, workarea.height / 2);
  gtk_window_set_resizable(GTK_WINDOW(context->window), true);
  gtk_window_set_position(GTK_WINDOW(context->window), GTK_WIN_POS_CENTER);

  // create webview
  context->webview = webkit_web_view_new();

  if (context->webview == nullptr)
  {
    throw std::runtime_error("could not create webview");
  }

  gtk_container_add(GTK_CONTAINER(context->window), GTK_WIDGET(context->webview));

  // create context instance for window and widget
  auto context_priv = new AudienceWindowContext(context);
  g_object_set_data(G_OBJECT(context->window), WIDGET_HANDLE_KEY, context_priv);
  g_object_set_data(G_OBJECT(context->webview), WIDGET_HANDLE_KEY, context_priv);

  // listen to destroy signal and title changed event
  g_signal_connect(G_OBJECT(context->window), "destroy", G_CALLBACK(NUCLEUS_SAFE_FN(widget_destroy_callback)), nullptr);
  g_signal_connect(G_OBJECT(context->webview), "destroy", G_CALLBACK(NUCLEUS_SAFE_FN(widget_destroy_callback)), nullptr);
  g_signal_connect(G_OBJECT(context->webview), "notify::title", G_CALLBACK(NUCLEUS_SAFE_FN(webview_title_update_callback)), nullptr);

  // TODO debugging features
  // WebKitSettings *settings =
  //     webkit_web_view_get_settings(WEBKIT_WEB_VIEW(context->webview));
  // webkit_settings_set_enable_write_console_messages_to_stdout(settings, true);
  // webkit_settings_set_enable_developer_extras(settings, true);

  // show window and trigger url load
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(context->webview), url.c_str());
  gtk_widget_show_all(GTK_WIDGET(context->window));

  TRACEA(info, "window created");
  return context;
}

void internal_window_destroy(AudienceWindowContext context)
{
  if (context->window != nullptr)
  {
    gtk_window_close(GTK_WINDOW(context->window));
    TRACEA(info, "window close triggered");
  }
}

void internal_loop()
{
  gtk_main();
}

void widget_destroy_callback(GtkWidget *widget, gpointer arg)
{
  auto context_priv = reinterpret_cast<AudienceWindowContext>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr)
  {
    // trigger event
    internal_on_window_destroyed(*context_priv);

    // remove context pointer from widgets
    if ((*context_priv)->window != nullptr)
    {
      g_object_set_data(G_OBJECT((*context_priv)->window), WIDGET_HANDLE_KEY, nullptr);
      (*context_priv)->window = nullptr;
    }

    if ((*context_priv)->webview)
    {
      g_object_set_data(G_OBJECT((*context_priv)->webview), WIDGET_HANDLE_KEY, nullptr);
      (*context_priv)->webview = nullptr;
    }

    // discard private context
    delete context_priv;
    TRACEA(info, "window closed and private context released");
  }

  // trigger quit signal
  gtk_main_quit();
}

void webview_title_update_callback(GtkWidget *widget, gpointer arg)
{
  auto context_priv = reinterpret_cast<AudienceWindowContext>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview != nullptr)
  {
    auto title = webkit_web_view_get_title(WEBKIT_WEB_VIEW((*context_priv)->webview));
    if (title != nullptr)
    {
      gtk_window_set_title(GTK_WINDOW((*context_priv)->window), title);
    }
  }
}
