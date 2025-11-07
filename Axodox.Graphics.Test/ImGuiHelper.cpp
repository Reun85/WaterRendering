#include "pch.h"
#include "ImGuiHelper.h"
#include "../ImGUI/imgui_internal.h"

namespace Reun::Menu {
using namespace winrt;
void SetupImGuiStyle() {
  using namespace Reun::Menu::Bess::Config;
  setBessDarkColors(); // 4/5
}

void InitImGui(const std::filesystem::path &iniPath) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  typedef std::basic_string<std::filesystem::path::value_type> system_string;
  system_string value = iniPath;
  static std::string save = Utf16ToUtf8(value);
  io.IniFilename = (char *)save.c_str();
}
com_ptr<ID3D12DescriptorHeap> InitImGuiPlatformDependent(
    const Axodox::Graphics::D3D12::GraphicsDevice &device, u8 framesInFlight) {

  // Setup Platform/Renderer bindings
  ImGui_ImplUwp_InitForCurrentView();

  D3D12_DESCRIPTOR_HEAP_DESC ImGuiDescriptorHeapDesc = {};
  ImGuiDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  ImGuiDescriptorHeapDesc.NumDescriptors = 2;
  ImGuiDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  com_ptr<ID3D12DescriptorHeap> ImGuiDescriptorHeap{};
  check_hresult(device.get()->CreateDescriptorHeap(
      &ImGuiDescriptorHeapDesc, IID_PPV_ARGS(&ImGuiDescriptorHeap)));
  static const char *debugName = "ImGui Descriptor Heap";
  ImGuiDescriptorHeap->SetPrivateData(WKPDID_D3DDebugObjectName,
                                      UINT(strlen(debugName)), debugName);
  ImGui_ImplDX12_Init(
      device.get(), static_cast<int>(framesInFlight),
      DXGI_FORMAT_B8G8R8A8_UNORM, ImGuiDescriptorHeap.operator->(),
      ImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
      ImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
  return ImGuiDescriptorHeap;
}

ImGUIManager::ImGUIManager(
    const Axodox::Graphics::D3D12::GraphicsDevice &device, u8 framesInFlight,
    const std::filesystem::path &iniPath, const std::string iniName)
    : persistence(), iniName(iniName) {

  // Have to set persistence before initializing!
  InitImGui(iniPath);
  SetupPersistence();
  ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
  descriptorHeap_ = (InitImGuiPlatformDependent(device, framesInFlight));
  SetupImGuiStyle();
}

ImGuiIO &ImGUIManager::GetIO() { return ImGui::GetIO(); }
ImGUIManager::~ImGUIManager() {
  descriptorHeap_ = nullptr;
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplUwp_Shutdown();
  ImGui::DestroyContext();
}

void ImGUIManager::SetupPersistence() {

  ImGuiSettingsHandler handler;
  handler.TypeName = iniName.c_str();
  handler.TypeHash = ImHashStr(iniName.c_str());
  handler.UserData = &persistence;

  handler.ReadOpenFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler,
                          const char *name) -> void * {
    // what data to read to
    return handler->UserData;
  };

  handler.ReadLineFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler,
                          void *user_data, const char *line) {
    std::unordered_map<std::string, std::string> &settings =
        *static_cast<std::unordered_map<std::string, std::string> *>(user_data);

    const char *delimiter = strchr(line, '=');

    // We have an '=' otherwise just throw it out!
    if (delimiter) {
      std::ptrdiff_t map_key_len = delimiter - line;

      // Until delim, everything is key
      std::string map_key(line, map_key_len);

      // After that it is key.
      std::string map_value(delimiter + 1);

      settings[map_key] = map_value;
    }
  };
  handler.WriteAllFn = [](ImGuiContext *ctx, ImGuiSettingsHandler *handler,
                          ImGuiTextBuffer *buf) {
    std::unordered_map<std::string, std::string> &settings =
        *static_cast<std::unordered_map<std::string, std::string> *>(
            handler->UserData);

    buf->appendf("[%s][]\n", handler->TypeName);

    for (auto &[key, value] : settings) {
      buf->appendf("%s=%s\n", key.c_str(), value.c_str());
    }
  };

  ImGui::AddSettingsHandler(&handler);
}

ID3D12DescriptorHeap *ImGUIManager::GetHeap() { return &*descriptorHeap_; }
void ImGUIManager::Pre(CommandAllocator &allocator) const {
  ID3D12DescriptorHeap *t = &*descriptorHeap_;
  allocator->SetDescriptorHeaps(1, &t);
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplUwp_NewFrame();
  ImGui::NewFrame();
}
void ImGUIManager::Render(CommandAllocator &allocator) const {

  ImGui::Render();

  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), allocator.operator->());
}
} // namespace Reun::Menu

namespace Reun::Menu::Bess::Config {

/*
Taken from: https://github.com/shivang51/bess/tree/main
MIT license
*/

ImVec4 BlendColors(const ImVec4 &base, const ImVec4 &accent,
                   float blendFactor) {
  return ImVec4(base.x * (1.0f - blendFactor) + accent.x * blendFactor,
                base.y * (1.0f - blendFactor) + accent.y * blendFactor,
                base.z * (1.0f - blendFactor) + accent.z * blendFactor, base.w);
}

void setBessDarkColors() {
  ImGuiStyle &style = ImGui::GetStyle();
  ImVec4 *colors = style.Colors;

  // Primary background
  colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);  // #131318
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f); // #131318

  colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

  // Headers
  colors[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);

  // Buttons
  colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.32f, 0.40f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.38f, 0.50f, 1.00f);

  // Frame BG
  colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.50f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.38f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);

  // Title
  colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

  // Highlights
  colors[ImGuiCol_CheckMark] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.70f, 1.00f, 0.50f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.80f, 1.00f, 0.75f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.70f, 0.90f, 1.00f, 1.00f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);

  // Style tweaks
  style.WindowRounding = 5.0f;
  style.FrameRounding = 5.0f;
  style.GrabRounding = 5.0f;
  style.TabRounding = 5.0f;
  style.PopupRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.WindowPadding = ImVec2(10, 10);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 6);
  style.PopupBorderSize = 0.f;
}

} // namespace Reun::Menu::Bess::Config