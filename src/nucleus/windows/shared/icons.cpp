#undef NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <algorithm>

#include "../../../common/trace.h"
#include "../../../common/scope_guard.h"
#include "icons.h"

void load_icon_handles(const NucleusImplAppDetails &details, HICON &small_icon_handle, HICON &large_icon_handle)
{
  scope_guard scope_always(scope_guard::execution::always);

  // reset out vars
  small_icon_handle = nullptr;
  large_icon_handle = nullptr;

  // start gdi+
  Gdiplus::GdiplusStartupInput gpsi{};
  ULONG_PTR gpt{};
  Gdiplus::GdiplusStartup(&gpt, &gpsi, nullptr);

  scope_always += [gpt]() { Gdiplus::GdiplusShutdown(gpt); };

  // load icons
  std::vector<Gdiplus::Bitmap *> icons;
  icons.reserve(details.icon_set.size());

  for (auto &icon_path : details.icon_set)
  {
    TRACEW(info, "loading icon " << icon_path);
    Gdiplus::Bitmap *icon = Gdiplus::Bitmap::FromFile(icon_path.c_str());
    if (icon->GetLastStatus() != Gdiplus::Ok)
    {
      TRACEA(error, "could not load icon");
      delete icon;
    }
    else
    {
      TRACEA(debug, "icon width = " << icon->GetWidth());
      icons.push_back(icon);
    }
  }

  if (icons.size() == 0)
  {
    return;
  }

  scope_always += [icons]() {
    for (auto icon : icons)
    {
      delete icon;
    }
  };

  // sort by size descending
  std::sort(icons.begin(), icons.end(),
            [](Gdiplus::Bitmap *a, Gdiplus::Bitmap *b) -> bool {
              return a->GetWidth() > b->GetWidth();
            });

  // pick icons
  Gdiplus::Bitmap *small_icon = icons.front();
  Gdiplus::Bitmap *large_icon = icons.front();

  for (auto icon : icons)
  {
    if (icon->GetWidth() >= GetSystemMetrics(SM_CXSMICON) && icon->GetHeight() >= GetSystemMetrics(SM_CYSMICON))
    {
      small_icon = icon;
    }
    if (icon->GetWidth() >= GetSystemMetrics(SM_CXICON) && icon->GetHeight() >= GetSystemMetrics(SM_CYICON))
    {
      large_icon = icon;
    }
  }

  // translate to handles
  if (small_icon->GetHICON(&small_icon_handle) != Gdiplus::Ok)
  {
    TRACEA(error, "could not acquire small icon handle");
  }
  if (large_icon->GetHICON(&large_icon_handle) != Gdiplus::Ok)
  {
    TRACEA(error, "could not acquire large icon handle");
  }
}
