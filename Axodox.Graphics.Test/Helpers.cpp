#pragma once
#include "pch.h"

std::string Utf16ToUtf8(const std::wstring_view &wstr) {
  if (wstr.empty())
    return {};
  // Calculat the size
  int size_needed = WideCharToMultiByte(
      CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
  if (size_needed == 0)
    return {}; // handle error as needed
  std::string str(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &str[0],
                      size_needed, nullptr, nullptr);
  return str;
}
std::wstring Utf8ToUtf16(const std::string_view &str) {
  if (str.empty())
    return {};
  // Calculat the size
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
  if (size_needed == 0)
    return {}; // handle error as needed
  std::wstring wstr(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &wstr[0],
                      size_needed);
  return wstr;
}
