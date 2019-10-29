#pragma once

#include <boost/beast/core.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <string>

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    boost::beast::string_view base,
    boost::beast::string_view path)
{
  if (base.empty())
    return std::string(path);
  std::string result(base);
#ifdef WIN32
  char constexpr path_separator = '\\';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for (auto &c : result)
    if (c == '/')
      c = path_separator;
  result = boost::replace_all_copy(result, "\\\\", "\\");
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  result = boost::replace_all_copy(result, "//", "/");
#endif
  return result;
}
