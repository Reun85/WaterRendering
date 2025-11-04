#pragma once
#include "pch.h"
#include "Helpers.h"

namespace Reun {
std::string Reun::Utf16ToUtf8(const std::string_view &wstr) {
  return std::string(wstr);
}
std::wstring Reun::Utf8ToUtf16(const std::wstring_view &wstr) {
  return std::wstring(wstr);
}
std::string Reun::Utf16ToUtf8(const std::wstring_view &wstr) {
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
std::wstring Reun::Utf8ToUtf16(const std::string_view &str) {
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

float3 XMVECTORToFloat3(const DirectX::XMVECTOR &x) {
  float3 result;
  DirectX::XMStoreFloat3(&result, x);
  return result;
}

std::filesystem::path GetCacheFolder() {
  auto localFolder = winrt::Windows::Storage::ApplicationData::Current()
                         .LocalCacheFolder()
                         .Path();
  std::wstring Wstr = localFolder.c_str();
  return std::filesystem::path(Wstr);
}

std::filesystem::path GetLocalFolder() {
  auto localFolder =
      winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
  std::wstring Wpath = localFolder.c_str();
  auto path = Utf16ToUtf8(Wpath);
  return path;
  // return std::filesystem::path(path);
}

void RuntimeResults::DrawImGui(bool exclusiveWindow) const {
  bool cont = true;
  if (exclusiveWindow)
    cont = ImGui::Begin("Results");
  if (cont) {
    ImGui::Text("QuadTree Nodes = %d", qtNodes);

    ImGui::Text("QuadTree buildtime %.3f ms/frame",
                GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                std::chrono::nanoseconds>(
                    QuadTreeBuildTime));
    ImGui::Text("Navigating QuadTree %.3f ms/frame",
                GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                std::chrono::nanoseconds>(
                    NavigatingTheQuadTree));
    ImGui::Text("Drawn Nodes: %d", drawnNodes);
    ImGui::Text(
        "CPU time %.3f ms/frame",
        GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                        std::chrono::nanoseconds>(CPUTime));
  }
  if (exclusiveWindow)
    ImGui::End();
}

float frac(float x) { return x - std::floor(x); }

NeedToDo NeedToDo::WithFirstLoopUpdateSettings() {
  NeedToDo ret;

  ret.patchHighestChanged = true;
  ret.patchMediumChanged = true;
  ret.patchLowestChanged = true;
  return ret;
}

void Reun::set_flag(u32 &flag, u32 flagIndex, bool flagValue) {
  if (flagValue) {
    flag |= 1 << flagIndex;
  } else {
    flag &= ~(1 << flagIndex);
  }
}
} // namespace Reun
