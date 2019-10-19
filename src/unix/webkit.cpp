#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <string>
#include <locale>
#include <codecvt>

#define AUDIENCE_COMPILING_INNER
#include "../inner.h"
#include "../trace.h"

bool audience_inner_init()
{
  if (gtk_init_check(0, NULL) == FALSE) {
    return false;
  }

  return true;
}

void *audience_inner_window_create(const wchar_t *const title, const wchar_t *const url)
{
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  auto titlea = converter.to_bytes(title);
  auto urla = converter.to_bytes(url);

  GdkRectangle workarea = {0};
  gdk_monitor_get_workarea(
    gdk_display_get_primary_monitor(gdk_display_get_default()),
    &workarea);

  auto window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), titlea.c_str());

  gtk_window_set_default_size(GTK_WINDOW(window), workarea.width / 2, workarea.height / 2);
  gtk_window_set_resizable(GTK_WINDOW(window), true);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

  auto webview = webkit_web_view_new();

  gtk_container_add(GTK_CONTAINER(window), webview);
  gtk_widget_show_all(window);

  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(webview), urla.c_str());

  TRACEA(info, "window created");

  return window;
}

void audience_inner_window_destroy(void *window)
{
  gtk_window_close(GTK_WINDOW(window));
}

void audience_inner_loop()
{
  gtk_main();
  // gtk_main_iteration_do();
}
