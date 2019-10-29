#pragma once

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.h>
#include <winrt/Windows.Storage.Streams.h>
#include <string>

extern const char *_audience_frontend_library_code_begin;
extern std::size_t _audience_frontend_library_code_length;

class WebViewUriToStreamResolver : public winrt::implements<WebViewUriToStreamResolver, winrt::Windows::Web::IUriToStreamResolver>
{
private:
  std::wstring base_directory;

public:
  WebViewUriToStreamResolver(std::wstring base_directory)
      : base_directory(base_directory) {}

  winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IInputStream> UriToStreamAsync(winrt::Windows::Foundation::Uri uri) const
  {
    if (!uri)
    {
      throw std::invalid_argument("invalid uri");
    }

    // check path
    std::wstring uri_path(uri.Path().c_str());

    if (uri_path.empty() ||
        uri_path[0] != L'/' ||
        uri_path.find(L"..") != std::wstring::npos)
    {
      throw std::runtime_error("illegal request target");
    }

    // handle /audience.js
    if (uri_path == L"/audience.js")
    {
      winrt::Windows::Storage::Streams::InMemoryRandomAccessStream stream;
      winrt::Windows::Storage::Streams::DataWriter dataWriter{stream};
      dataWriter.WriteBytes(winrt::array_view((const uint8_t *)_audience_frontend_library_code_begin, (const uint8_t *)_audience_frontend_library_code_begin + _audience_frontend_library_code_length));
      co_await dataWriter.StoreAsync();
      dataWriter.DetachStream();
      stream.Seek(0);
      co_return stream;
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
    try
    {
      auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(winrt::hstring(path));
      auto stream = co_await file.OpenReadAsync();
      co_return stream;
    }
    catch (const winrt::hresult_error &e)
    {
      TRACEE(e);
      throw std::runtime_error(winrt::to_string(e.message()));
    }
  }
};
