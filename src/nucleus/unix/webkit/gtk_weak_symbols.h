#pragma once

// NOTE: This is required for backward compatibility.
//       Include this header before including gtk.h!

#pragma weak gdk_display_get_monitor
#pragma weak gdk_display_get_monitor_at_window
#pragma weak gdk_display_get_n_monitors
#pragma weak gdk_display_get_primary_monitor
#pragma weak gdk_monitor_get_geometry
#pragma weak gdk_monitor_get_workarea
#pragma weak gdk_monitor_is_primary
