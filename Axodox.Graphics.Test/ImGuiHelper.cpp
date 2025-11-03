#include "pch.h"
#include "ImGuiHelper.h"

using namespace winrt;

com_ptr<ID3D12DescriptorHeap>
InitImGui(const Axodox::Graphics::D3D12::GraphicsDevice &device,
          u8 framesInFlight, const std ::string &iniPath) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();

  io.IniFilename = iniPath.c_str();

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
    const std::string &iniPath) {
  descriptorHeap_ = (InitImGui(device, framesInFlight, iniPath));
}

ImGuiIO &ImGUIManager::GetIO() { return ImGui::GetIO(); }
ImGUIManager::~ImGUIManager() {
  descriptorHeap_ = nullptr;
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplUwp_Shutdown();
  ImGui::DestroyContext();
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
