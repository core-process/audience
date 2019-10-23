#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.h>
#include <winrt/Windows.Storage.Streams.h>
#include <string>

class WebViewUriToStreamResolver : public winrt::implements<WebViewUriToStreamResolver, winrt::Windows::Web::IUriToStreamResolver>
{
private:
  std::wstring base_directory;

public:
  WebViewUriToStreamResolver(std::wstring base_directory)
      : base_directory(base_directory) {}

  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IInputStream> UriToStreamAsync(winrt::Windows::Foundation::Uri uri) const
  {
    // check path
    std::wstring uri_path(uri.Path().c_str());

    if (uri_path.empty() ||
        uri_path[0] != L'/' ||
        uri_path.find(L"..") != std::wstring::npos)
    {
      throw std::runtime_error("illegal request target");
    }

    // assemble path
    auto path = base_directory + L"\\" + uri_path.substr(1);
    for (auto &c : path)
    {
      if (c == '/')
      {
        c = '\\';
      }
    }

    // retrieve file and stream
    auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(winrt::hstring(path));
    auto stream = co_await file.OpenReadAsync();
    co_return stream;
  }
};
