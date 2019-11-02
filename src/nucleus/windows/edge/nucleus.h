#pragma once

#include <windows.h>
#include <winrt/Windows.Web.UI.Interop.h>
#include <memory>
#include <spdlog/fmt/fmt.h>

///////////////////////////////////////////////////////////////////////
// Window Context
///////////////////////////////////////////////////////////////////////

struct AudienceWindowContextData
{
  HWND window;
  winrt::Windows::Web::UI::Interop::WebViewControl webview;

  AudienceWindowContextData() : window(nullptr),
                                webview(nullptr)
  {
  }
};

using AudienceWindowContext = std::shared_ptr<AudienceWindowContextData>;

///////////////////////////////////////////////////////////////////////
// Exception Handling
///////////////////////////////////////////////////////////////////////

#define NUCLEUS_TRANSLATE_EXCEPTIONS winrt::hresult_error

FMT_BEGIN_NAMESPACE

template <>
struct formatter<winrt::hresult_error>
{
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const winrt::hresult_error &ex, FormatContext &ctx)
  {
    return format_to(ctx.out(), "{}",  winrt::to_string(ex.message()));
  }
};

FMT_END_NAMESPACE
