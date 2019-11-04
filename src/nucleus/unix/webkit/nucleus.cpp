#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <spdlog/spdlog.h>

#include "../../../common/scope_guard.h"
#include "../../../common/utf.h"
#include "../../shared/interface.h"
#include "nucleus.h"

#define WIDGET_HANDLE_KEY "audience_window_handle"

void window_resize_callback(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data);
gboolean window_close_callback(GtkWidget *widget, GdkEvent *event, gpointer user_data);
void window_destroy_callback(GtkWidget *widget, gpointer arg);
void webview_title_update_callback(GtkWidget *widget, gpointer arg);

static gboolean window_close_callback_default_return = TRUE;

bool nucleus_impl_init(AudienceNucleusProtocolNegotiation &negotiation, const NucleusImplAppDetails &details)
{
  // negotiate protocol
  negotiation.nucleus_handles_webapp_type_url = true;

  // init gtk
  if (gtk_init_check(0, NULL) == FALSE)
  {
    return false;
  }

  // load icons
  // NOTE: GDK/X11 (and maybe other implementations) too stops packing icons silently,
  //       once a certain limit is hit. For that reason it seems best to order icons
  //       by size ascending.
  std::vector<GdkPixbuf *> icons{};
  icons.reserve(details.icon_set.size());

  for (auto &icon_path : details.icon_set)
  {
    SPDLOG_INFO("loading icon {}", utf16_to_utf8(icon_path));
    GError *gerror = nullptr;
    auto icon = gdk_pixbuf_new_from_file(utf16_to_utf8(icon_path).c_str(), &gerror);
    if (icon == nullptr)
    {
      SPDLOG_ERROR("could not load icon: {}", gerror->message);
      g_error_free(gerror);
    }
    else
    {
      SPDLOG_DEBUG("icon width = {}", gdk_pixbuf_get_width(icon));
      icons.push_back(icon);
    }
  }

  std::sort(icons.begin(), icons.end(),
            [](const GdkPixbuf *a, const GdkPixbuf *b) -> bool {
              return gdk_pixbuf_get_width(a) < gdk_pixbuf_get_width(b);
            });

  GList *icon_list = nullptr;
  for (auto icon : icons)
  {
    icon_list = g_list_append(icon_list, icon);
  }

  if (icon_list != nullptr)
  {
    SPDLOG_DEBUG("setting default icon list");
    gtk_window_set_default_icon_list(icon_list);
  }

  return true;
}

AudienceScreenList nucleus_impl_screen_list()
{
  AudienceScreenList result{};

  // find focused monitor
  auto display = gdk_display_get_default();
  auto screen = gdk_display_get_default_screen(display);
  GdkMonitor *focused_monitor = nullptr;
  {
    GdkWindow *focused_window = nullptr;
    auto window_list = gdk_screen_get_window_stack(screen);
    for (auto i = window_list; i != nullptr; i = i->next)
    {
      auto window_state = gdk_window_get_state(GDK_WINDOW(i->data));
      if ((window_state & GDK_WINDOW_STATE_FOCUSED) != 0)
      {
        focused_window = GDK_WINDOW(i->data);
        break;
      }
    }
    if (focused_window != nullptr)
    {
      focused_monitor = gdk_display_get_monitor_at_window(display, focused_window);
    }
    for (auto i = window_list; i != nullptr; i = i->next)
    {
      g_object_unref(G_OBJECT(i->data));
    }
    g_list_free(window_list);
  }

  // iterate monitors
  for (int monitor_count = gdk_display_get_n_monitors(display), i = 0; i < monitor_count && result.count < AUDIENCE_SCREEN_LIST_ENTRIES; ++i)
  {
    auto monitor = gdk_display_get_monitor(display, i);
    if (monitor == nullptr)
    {
      SPDLOG_WARN("invalid monitor no {}", i);
      continue;
    }

    if (gdk_monitor_is_primary(monitor))
    {
      result.primary = result.count;
    }

    if (monitor == focused_monitor)
    {
      result.focused = result.count;
    }

    GdkRectangle frame{}, workspace{};

    gdk_monitor_get_geometry(monitor, &frame);
    gdk_monitor_get_workarea(monitor, &workspace);

    result.screens[result.count].frame = {{(double)frame.x, (double)frame.y},
                                          {(double)frame.width, (double)frame.height}};

    result.screens[result.count].workspace = {{(double)workspace.x, (double)workspace.y},
                                              {(double)workspace.width, (double)workspace.height}};

    result.count += 1;
  }

  return result;
}

AudienceWindowContext nucleus_impl_window_create(const NucleusImplWindowDetails &details)
{
  scope_guard scope_fail(scope_guard::execution::exception);

  // create context instance
  auto context = std::make_shared<AudienceWindowContextData>();

  // convert input parameter
  auto title = utf16_to_utf8(details.loading_title);
  auto url = utf16_to_utf8(details.webapp_location);

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
  g_signal_connect(G_OBJECT(context->window), "size-allocate", G_CALLBACK(NUCLEUS_SAFE_FN(window_resize_callback)), nullptr);
  g_signal_connect(G_OBJECT(context->window), "delete-event", G_CALLBACK(NUCLEUS_SAFE_FN(window_close_callback, &window_close_callback_default_return)), nullptr);
  g_signal_connect(G_OBJECT(context->window), "destroy", G_CALLBACK(NUCLEUS_SAFE_FN(window_destroy_callback)), nullptr);
  g_signal_connect(G_OBJECT(context->webview), "notify::title", G_CALLBACK(NUCLEUS_SAFE_FN(webview_title_update_callback)), nullptr);

  // debugging features
  if (details.dev_mode)
  {
    WebKitSettings *settings =
        webkit_web_view_get_settings(WEBKIT_WEB_VIEW(context->webview));
    webkit_settings_set_enable_write_console_messages_to_stdout(settings, true);
    webkit_settings_set_enable_developer_extras(settings, true);
  }

  // set window styles
  if (details.styles.not_decorated)
  {
    gtk_window_set_decorated(GTK_WINDOW(context->window), FALSE);
  }

  if (details.styles.not_resizable)
  {
    gtk_window_set_resizable(GTK_WINDOW(context->window), FALSE);
  }

  if (details.styles.always_on_top)
  {
    gtk_window_set_keep_above(GTK_WINDOW(context->window), TRUE);
  }

  // position window
  if (details.position.size.width > 0 && details.position.size.height > 0)
  {
    nucleus_impl_window_update_position(context, details.position);
  }

  // show window and trigger url load
  webkit_web_view_load_uri(WEBKIT_WEB_VIEW(context->webview), url.c_str());
  gtk_widget_show_all(GTK_WIDGET(context->window));

  SPDLOG_INFO("window created");
  return context;
}

NucleusImplWindowStatus
nucleus_impl_window_status(AudienceWindowContext context)
{
  NucleusImplWindowStatus result{};

  // has focus?
  result.has_focus = !!gtk_window_has_toplevel_focus(GTK_WINDOW(context->window));

  // retrieve frame
  gint wx = 0, wy = 0, ww = 0, wh = 0;
  gtk_window_get_position(GTK_WINDOW(context->window), &wx, &wy);
  gtk_window_get_size(GTK_WINDOW(context->window), &ww, &wh);

  result.frame = {{(double)wx, (double)wy}, {(double)ww, (double)wh}};

  // retrieve workspace
  GtkAllocation walloc{};
  gtk_widget_get_allocation(GTK_WIDGET(context->webview), &walloc);

  result.workspace = {(double)walloc.width, (double)walloc.height};

  return result;
}

void nucleus_impl_window_update_position(AudienceWindowContext context,
                                         AudienceRect position)
{
  SPDLOG_DEBUG("window_update_position: origin={},{} size={}x{}", position.origin.x, position.origin.y, position.size.width, position.size.height);

  // write positioning info to context
  context->last_positioning = std::chrono::steady_clock::now();
  context->last_positioning_data = position;

  // try move
  gtk_window_move(GTK_WINDOW(context->window), (gint)std::ceil(position.origin.x), (gint)std::ceil(position.origin.y));

  // resize
  if (gtk_window_get_resizable(GTK_WINDOW(context->window)))
  {
    gtk_window_resize(GTK_WINDOW(context->window), (gint)std::floor(position.size.width), (gint)std::floor(position.size.height));
  }
  else
  {
    gtk_widget_set_size_request(GTK_WIDGET(context->window), (gint)std::floor(position.size.width), (gint)std::floor(position.size.height));
  }
}

void nucleus_impl_window_post_message(AudienceWindowContext context, const std::wstring &message) {}

void nucleus_impl_window_destroy(AudienceWindowContext context)
{
  SPDLOG_TRACE("delaying call to gtk_widget_destroy()");
  gdk_threads_add_idle_full(
      G_PRIORITY_HIGH_IDLE,
      [](void *context_void) {
        AudienceWindowContext *context = reinterpret_cast<AudienceWindowContext *>(context_void);
        if ((*context)->window != nullptr)
        {
          SPDLOG_INFO("calling gtk_widget_destroy()");
          gtk_widget_destroy(GTK_WIDGET((*context)->window));
        }
        delete context;
        return FALSE;
      },
      new AudienceWindowContext(context),
      nullptr);
}

void nucleus_impl_quit()
{
  SPDLOG_TRACE("delaying call to gtk_main_quit()");
  gdk_threads_add_timeout(
      100,
      [](void *context) -> int {
        GList *window_list = gtk_window_list_toplevels();
        size_t window_count = 0;
        for (auto i = window_list; i != nullptr; i = i->next)
        {
          if (gtk_widget_is_visible(GTK_WIDGET(i->data)))
          {
            window_count += 1;
          }
          else
          {
            SPDLOG_DEBUG("hidden window detected");
          }
        }
        g_list_free(window_list);
        SPDLOG_INFO("top level windows: {}", window_count);
        if (window_count == 0)
        {
          SPDLOG_INFO("calling gtk_main_quit()");
          gtk_main_quit();
          return FALSE;
        }
        return TRUE;
      },
      nullptr);
}

void nucleus_impl_main()
{
  gtk_main();

  // trigger final event
  emit_app_quit();

  // lets quit now
  SPDLOG_INFO("calling exit()");
  exit(0);
}

void nucleus_impl_dispatch_sync(void (*task)(void *context), void *context)
{
  // sync utilities
  bool ready = false;
  std::condition_variable condition;
  std::mutex mutex;

  // prepare wrapper
  auto wrapper_lambda = [&]() {
    // execute task
    task(context);
    // set ready signal
    std::unique_lock<std::mutex> ready_lock(mutex);
    ready = true;
    ready_lock.unlock();
    condition.notify_one();
  };

  auto wrapper = [](void *context) {
    (*static_cast<decltype(wrapper_lambda) *>(context))();
    return FALSE;
  };

  // execute wrapper
  SPDLOG_INFO("dispatching task on main queue (sync)");
  gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE, wrapper, &wrapper_lambda, nullptr);

  // wait for ready signal
  std::unique_lock<std::mutex> wait_lock(mutex);
  condition.wait(wait_lock, [&] { return ready; });
}

void nucleus_impl_dispatch_async(void (*task)(void *context), void *context)
{
  struct wrapped_context_t
  {
    void (*task)(void *context);
    void *context;
  };

  SPDLOG_INFO("dispatching task on main queue (async)");
  gdk_threads_add_idle_full(
      G_PRIORITY_HIGH_IDLE,
      [](void *wrapped_context_void) {
        auto wrapped_context = static_cast<wrapped_context_t *>(wrapped_context_void);
        wrapped_context->task(wrapped_context->context);
        return FALSE;
      },
      new wrapped_context_t{task, context},
      [](void *wrapped_context_void) {
        auto wrapped_context = static_cast<wrapped_context_t *>(wrapped_context_void);
        delete wrapped_context;
      });
}

void window_resize_callback(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data)
{
  auto context_priv = reinterpret_cast<AudienceWindowContext *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr)
  {
    std::chrono::duration<double> time_delta = std::chrono::steady_clock::now() - (*context_priv)->last_positioning;
    if (time_delta.count() <= 1)
    {
      SPDLOG_DEBUG("delayed window move in resize event");
      gtk_window_move(
          GTK_WINDOW((*context_priv)->window),
          (gint)std::ceil((*context_priv)->last_positioning_data.origin.x),
          (gint)std::ceil((*context_priv)->last_positioning_data.origin.y));
    }
  }
}

gboolean window_close_callback(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  // trigger event
  auto context_priv = reinterpret_cast<AudienceWindowContext *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr)
  {
    emit_window_close_intent(*context_priv);
  }
  else
  {
    gtk_widget_destroy(widget);
  }

  return TRUE;
}

void window_destroy_callback(GtkWidget *widget, gpointer arg)
{
  auto context_priv = reinterpret_cast<AudienceWindowContext *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr)
  {
    // trigger event
    emit_window_close(*context_priv, util_is_only_window(*context_priv));

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
    SPDLOG_INFO("window closed and private context released");
  }
}

void webview_title_update_callback(GtkWidget *widget, gpointer arg)
{
  auto context_priv = reinterpret_cast<AudienceWindowContext *>(g_object_get_data(G_OBJECT(widget), WIDGET_HANDLE_KEY));
  if (context_priv != nullptr && (*context_priv)->window != nullptr && (*context_priv)->webview != nullptr)
  {
    auto title = webkit_web_view_get_title(WEBKIT_WEB_VIEW((*context_priv)->webview));
    if (title != nullptr)
    {
      gtk_window_set_title(GTK_WINDOW((*context_priv)->window), title);
    }
  }
}
