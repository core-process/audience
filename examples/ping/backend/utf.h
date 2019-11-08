#pragma once

#include <string>
#include <locale>
#include <codecvt>

static std::string utf16_to_utf8(const std::wstring &data)
{
  static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(data);
}

static std::wstring utf8_to_utf16(const std::string &data)
{
  static thread_local std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(data);
}
