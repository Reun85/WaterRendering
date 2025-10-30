#include "pch.h"
#include <string.h>
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"

using namespace Axodox::Infrastructure;

inline static ID3D12DescriptorHeap *
InitImGui(const Axodox::Graphics::D3D12::GraphicsDevice &device,
          u8 framesInFlight, const string &iniPath) {
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

  ID3D12DescriptorHeap *ImGuiDescriptorHeap{};
  check_hresult(device.get()->CreateDescriptorHeap(
      &ImGuiDescriptorHeapDesc, IID_PPV_ARGS(&ImGuiDescriptorHeap)));
  ImGui_ImplDX12_Init(
      device.get(), static_cast<int>(framesInFlight),
      DXGI_FORMAT_B8G8R8A8_UNORM, ImGuiDescriptorHeap,
      ImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
      ImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
  return ImGuiDescriptorHeap;
}

struct ImGUIManager {

  ImGUIManager(const Axodox::Graphics::D3D12::GraphicsDevice &device,
               u8 framesInFlight, const string &iniPath)

  {

    descriptorHeap_ = (InitImGui(device, framesInFlight, iniPath));
  }

  ImGuiIO &GetIO() { return ImGui::GetIO(); }
  ~ImGUIManager() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplUwp_Shutdown();
    ImGui::DestroyContext();
    if (descriptorHeap_) {
      descriptorHeap_->Release();
      descriptorHeap_ = nullptr;
    }
  }
  void Render(CommandAllocator &allocator) const {

    ImGui::Render();

    allocator->SetDescriptorHeaps(1, &descriptorHeap_);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), allocator.operator->());
  }
  ID3D12DescriptorHeap *descriptorHeap_ = nullptr;
  // ImGuiIO &io;
};
