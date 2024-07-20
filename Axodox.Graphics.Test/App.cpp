#include <pch.h>
#include "Camera.h"
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Popups.h>
#include "QuadTree.h"
#include <fstream>
#include <string.h>
using namespace std;
using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX;
using namespace DirectX::PackedVector;

template <typename T>
concept IsRatio = std::is_same_v<T, std::ratio<T::num, T::den>>;
template <typename T>
concept HasRatioPeriod =
    requires { typename T::period; } && IsRatio<typename T::period>;

template <typename TimeRep, typename PrecisionRep, typename T, typename Q>
  requires IsRatio<TimeRep> && IsRatio<PrecisionRep> && IsRatio<Q>
constexpr float
GetDurationInFloatWithPrecision(const std::chrono::duration<T, Q> &inp) {

  using TimeRepReciprocal = std::ratio<TimeRep::den, TimeRep::num>;
  using Result = std::ratio_multiply<TimeRepReciprocal, PrecisionRep>;

  const T count =
      std::chrono::duration_cast<std::chrono::duration<T, PrecisionRep>>(inp)
          .count();
  return count * Result::num / static_cast<float>(Result::den);
}
template <typename TimeRepTimeFrame, typename PrecisionTimeFrame, typename T,
          typename Q>
  requires HasRatioPeriod<TimeRepTimeFrame> &&
           HasRatioPeriod<PrecisionTimeFrame> && IsRatio<Q>
constexpr float
GetDurationInFloatWithPrecision(const std::chrono::duration<T, Q> &inp) {

  return GetDurationInFloatWithPrecision<typename TimeRepTimeFrame::period,
                                         typename PrecisionTimeFrame::period, T,
                                         Q>(inp);
}

MeshDescription CreateQuadPatch(float size) {

  size = size / 2;

  MeshDescription result;

  result.Vertices = {
      VertexPositionTexture{XMFLOAT3{-size, size, 0.f}, XMUSHORTN2{0.f, 0.f}},
      VertexPositionTexture{XMFLOAT3{-size, -size, 0.f}, XMUSHORTN2{0.f, 1.f}},
      VertexPositionTexture{XMFLOAT3{size, size, 0.f}, XMUSHORTN2{1.f, 0.f}},
      VertexPositionTexture{XMFLOAT3{size, -size, 0.f}, XMUSHORTN2{1.f, 1.f}}};

  // No indices
  // result.Indices = {0, 1, 2, 1, 3, 2};
  //  D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST = 36
  //  PrimitiveTopology::PatchList4 = 36
  // result.Topology = PrimitiveTopology::PatchList4;

  result.Topology = static_cast<PrimitiveTopology>(
      D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

  return result;
}

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {

  struct Defaults {
    static const constexpr float planeSize = 100.0f;
    static const constexpr float3 camStartPos = float3(-1, 300, 0);
    static const constexpr XMFLOAT4 clearColor = {0, 1, 0, 0};
    static const constexpr bool startFirstPerson = true;

    static const constexpr uint32_t heightMapDimensions = 1024;
    static const constexpr uint32_t computeShaderGroupsX = 16;
    static const constexpr uint32_t computeShaderGroupsY = 16;
  };

  IFrameworkView CreateView() { return *this; }
  void Initialize(CoreApplicationView const &) {}

  void Load(hstring const &) {}

  void Uninitialize() {}

  struct FrameResources {
    CommandAllocator Allocator;
    CommandFence Fence;
    CommandFenceMarker Marker;
    DynamicBufferManager DynamicBuffer;

    MutableTexture DepthBuffer;
    descriptor_ptr<ShaderResourceView> ScreenResourceView;

    FrameResources(const ResourceAllocationContext &context)
        : Allocator(*context.Device), Fence(*context.Device), Marker(),
          DynamicBuffer(*context.Device), DepthBuffer(context) {}
  };

  struct SimpleRootDescription : public RootSignatureMask {

    struct DomainConstants {

      // There is no 2x2?
      XMFLOAT4X4 TextureTransform;
      XMFLOAT4X4 WorldIT;
    };

    struct VertexConstants {

      XMFLOAT4X4 WorldViewProjection;
    };
    struct HullConstants {
      // zneg,xneg, zpos, xpos
      XMFLOAT4 TesselationFactor;
    };

    RootDescriptor<RootDescriptorType::ConstantBuffer> VertexBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> HullBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> DomainBuffer;
    RootDescriptorTable<1> Texture;
    StaticSampler TextureSampler;
    RootDescriptorTable<1> Heightmap;
    StaticSampler HeightmapSampler;
    RootDescriptorTable<1> HeightMapForDomain;
    StaticSampler HeightmapSamplerForDomain;

    SimpleRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          VertexBuffer(this, {0}, ShaderVisibility::Vertex),
          HullBuffer(this, {0}, ShaderVisibility::Hull),
          DomainBuffer(this, {0}, ShaderVisibility::Domain),
          Texture(this, {DescriptorRangeType::ShaderResource},
                  ShaderVisibility::Pixel),
          TextureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                         ShaderVisibility::Pixel),
          HeightMapForDomain(this, {DescriptorRangeType::ShaderResource},
                             ShaderVisibility::Domain),
          HeightmapSamplerForDomain(this, {0}, Filter::Linear,
                                    TextureAddressMode::Clamp,
                                    ShaderVisibility::Domain),
          Heightmap(this, {DescriptorRangeType::ShaderResource},
                    ShaderVisibility::Hull),
          HeightmapSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                           ShaderVisibility::Hull) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct ComputeShaderRootDescription : public RootSignatureMask {
    struct ComputeShaderConstants {
      float time;
    };
    RootDescriptor<RootDescriptorType::ConstantBuffer> ConstantBuffer;
    RootDescriptorTable<1> OutputTexture;

    ComputeShaderRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          OutputTexture(this, {DescriptorRangeType::UnorderedAccess}),
          ConstantBuffer(this, {0}) {
      Flags = RootSignatureFlags::None;
    }
  };

  D3D12_INPUT_ELEMENT_DESC InputElement(const char *semanticName,
                                        uint32_t semanticIndex,
                                        DXGI_FORMAT format) {
    return {.SemanticName = semanticName,
            .SemanticIndex = semanticIndex,
            .Format = format,
            .InputSlot = 0,
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0};
  }
  void Run() {

    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    Camera cam;
    cam.SetView(XMVectorSet(Defaults::camStartPos.x, Defaults::camStartPos.y,
                            Defaults::camStartPos.z, 0),
                XMVectorSet(0.0f, 0.0f, 0.0f, 0),
                XMVectorSet(0.0f, 1.0f, 0.0f, 0));
    bool quit = false;
    bool firstperson = Defaults::startFirstPerson;
    cam.SetFirstPerson(firstperson);
    // Events
    {
      // If I redo the whole system, this could be handled when the callback is
      // made.
      window.KeyDown([&cam, &quit, &firstperson](CoreWindow const &,
                                                 KeyEventArgs const &args) {
        if (ImGui::GetIO().WantCaptureKeyboard)
          return;
        switch (args.VirtualKey()) {
        case Windows::System::VirtualKey::Escape:
          quit = true;
          break;
        case Windows::System::VirtualKey::Space:
          firstperson = !firstperson;
          cam.SetFirstPerson(firstperson);
          break;

        default:
          cam.KeyboardDown(args);
          break;
        }
      });
      window.KeyUp([&cam](CoreWindow const &, KeyEventArgs const &args) {
        if (ImGui::GetIO().WantCaptureKeyboard)
          return;
        cam.KeyboardUp(args);
      });
      window.PointerMoved(
          [&cam](CoreWindow const &, PointerEventArgs const &args) {
            if (ImGui::GetIO().WantCaptureMouse)
              return;
            cam.MouseMove(args);
          });
      window.PointerWheelChanged(
          [&cam](CoreWindow const &, PointerEventArgs const &args) {
            if (ImGui::GetIO().WantCaptureMouse)
              return;
            cam.MouseWheel(args);
          });
    }

    CoreDispatcher dispatcher = window.Dispatcher();

    GraphicsDevice device{};
    CommandQueue directQueue{device};
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsShaderResource};

    PipelineStateProvider pipelineStateProvider{device};

    // Graphics pipeline
    RootSignature<SimpleRootDescription> simpleRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"SimpleVertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"SimplePixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

    RasterizerFlags rasterizerFlags = RasterizerFlags::CullClockwise;

    GraphicsPipelineStateDefinition simplePipelineStateDefinition{
        .RootSignature = &simpleRootSignature,
        .VertexShader = &simpleVertexShader,
        .DomainShader = &domainShader,
        .HullShader = &hullShader,
        .PixelShader = &simplePixelShader,
        .RasterizerState = rasterizerFlags,
        .DepthStencilState = DepthStencilMode::WriteDepth,
        .InputLayout = VertexPositionTexture::Layout,
        .TopologyType = PrimitiveTopologyType::Patch,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
        .DepthStencilFormat = Format::D32_Float};
    Axodox::Graphics::D3D12::PipelineState simplePipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(simplePipelineStateDefinition)
            .get();

    // Compute pipeline

    ComputeShader computeShader{app_folder() / L"computeShader.cso"};
    RootSignature<ComputeShaderRootDescription> computeShaderRootSignature{
        device};
    ComputePipelineStateDefinition computePipelineStateDefinition{
        .RootSignature = &computeShaderRootSignature,
        .ComputeShader = &computeShader};
    auto computePipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(computePipelineStateDefinition)
            .get();

    // Group together allocations
    GroupedResourceAllocator groupedResourceAllocator{device};
    ResourceUploader resourceUploader{device};
    CommonDescriptorHeap commonDescriptorHeap{device, 2};
    DepthStencilDescriptorHeap depthStencilDescriptorHeap{device};
    ResourceAllocationContext immutableAllocationContext{
        .Device = &device,
        .ResourceAllocator = &groupedResourceAllocator,
        .ResourceUploader = &resourceUploader,
        .CommonDescriptorHeap = &commonDescriptorHeap,
        .DepthStencilDescriptorHeap = &depthStencilDescriptorHeap};

    ImmutableMesh planeMesh{immutableAllocationContext,
                            CreateQuadPatch(Defaults::planeSize)};
    // ImmutableTexture texture{immutableAllocationContext,
    //                          app_folder() / L"image.jpeg"};
    // ImmutableTexture texture{immutableAllocationContext,
    //                         app_folder() / L"iceland_heightmap.png"};
    ImmutableTexture heightmap{immutableAllocationContext,
                               app_folder() / L"iceland_heightmap.png"};

    ImmutableTexture texture{immutableAllocationContext,
                             app_folder() / L"gradient.jpg"};

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    // Used by compute and domain shader
    MutableTexture heightMap{mutableAllocationContext,
                             TextureDefinition(Format::R32_Float,
                                               Defaults::heightMapDimensions,
                                               Defaults::heightMapDimensions, 0,
                                               TextureFlags::UnorderedAccess)};

    //  Acquire memory
    groupedResourceAllocator.Build();

    array<FrameResources, 2> frames{mutableAllocationContext,
                                    mutableAllocationContext};

    swapChain.Resizing(no_revoke, [&](SwapChain *) {
      for (auto &frame : frames)
        frame.ScreenResourceView.reset();
      commonDescriptorHeap.Clean();
    });

    ID3D12DescriptorHeap *ImGuiDescriptorHeap{};
    // Init ImGUI
    {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGuiIO &io = ImGui::GetIO();
      (void)io;
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
      ImGui::StyleColorsDark();

      // Setup Platform/Renderer bindings
      ImGui_ImplUwp_InitForCurrentView();

      D3D12_DESCRIPTOR_HEAP_DESC ImGuiDescriptorHeapDesc = {};
      ImGuiDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      ImGuiDescriptorHeapDesc.NumDescriptors = 2;
      ImGuiDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

      check_hresult(device.get()->CreateDescriptorHeap(
          &ImGuiDescriptorHeapDesc, IID_PPV_ARGS(&ImGuiDescriptorHeap)));
      ImGui_ImplDX12_Init(
          device.get(), static_cast<int>(frames.size()),
          DXGI_FORMAT_B8G8R8A8_UNORM, ImGuiDescriptorHeap,
          ImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
          ImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
    ImGuiIO &io = ImGui::GetIO();

    // Time counter
    using SinceTimeStartTimeFrame = std::chrono::nanoseconds;
    auto loopStartTime = std::chrono::high_resolution_clock::now();
    auto getTimeSinceStart = [&loopStartTime]() {
      return std::chrono::duration_cast<SinceTimeStartTimeFrame>(
          std::chrono::high_resolution_clock::now() - loopStartTime);
    };
    // Frame counter
    auto i = 0u;
    XMFLOAT4 clearColor = Defaults::clearColor;
    while (!quit) {
      // Process user input
      dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

      // Get frame resources
      auto &resources = frames[i++ & 0x1u];
      auto renderTargetView = swapChain.RenderTargetView();

      // Wait until buffers can be reused
      if (resources.Marker)
        resources.Fence.Await(resources.Marker);

      // Ensure depth buffer matches frame size
      if (!resources.DepthBuffer || !TextureDefinition::AreSizeCompatible(
                                        *resources.DepthBuffer.Definition(),
                                        renderTargetView->Definition())) {
        auto depthDefinition =
            renderTargetView->Definition().MakeSizeCompatible(
                Format::D32_Float, TextureFlags::DepthStencil);
        resources.DepthBuffer.Allocate(depthDefinition);
      }

      // Ensure screen shader resource view
      if (!resources.ScreenResourceView ||
          resources.ScreenResourceView->Resource() !=
              renderTargetView->Resource()) {
        Texture screenTexture{renderTargetView->Resource()};
        resources.ScreenResourceView =
            commonDescriptorHeap.CreateShaderResourceView(&screenTexture);
      }

      // Update constants
      auto resolution = swapChain.Resolution();
      cam.SetAspect(float(resolution.x) / float(resolution.y));
      cam.Update(0.01f);

      // Begin frame command list
      auto &allocator = resources.Allocator;
      {
        allocator.Reset();
        allocator.BeginList();
        allocator.TransitionResource(*renderTargetView, ResourceStates::Present,
                                     ResourceStates::RenderTarget);

        committedResourceAllocator.Build();
        depthStencilDescriptorHeap.Build();

        commonDescriptorHeap.Build();
        commonDescriptorHeap.Set(allocator);

        // Clears frame with background color
        renderTargetView->Clear(allocator, clearColor);
        resources.DepthBuffer.DepthStencil()->Clear(allocator);
      }

      {

        ComputeShaderRootDescription::ComputeShaderConstants constant{};
        constant.time =
            GetDurationInFloatWithPrecision<std::chrono::seconds,
                                            std::chrono::milliseconds>(
                getTimeSinceStart());
        auto mask = computeShaderRootSignature.Set(allocator,
                                                   RootSignatureUsage::Compute);
        mask.ConstantBuffer = resources.DynamicBuffer.AddBuffer(constant);
        mask.OutputTexture = *heightMap.UnorderedAccess();

        computePipelineState.Apply(allocator);

        const auto xSize = Defaults::computeShaderGroupsX;
        const auto ySize = Defaults::computeShaderGroupsY;
        const auto size = Defaults::heightMapDimensions;
        allocator.Dispatch((size + xSize - 1) / xSize,
                           (size + ySize - 1) / ySize, 1);
      }

      // Draw objects
      uint qtNodes;
      std::chrono::nanoseconds QuadTreeBuildTime;

      std::chrono::nanoseconds NavigatingTheQuadTree(0);

      {
        SimpleRootDescription::VertexConstants vertexConstants{};
        SimpleRootDescription::HullConstants hullConstants{};
        SimpleRootDescription::DomainConstants domainConstants{};
        float3 center = {0, 0, 0};
        float2 fullSizeXZ = {Defaults::planeSize, Defaults::planeSize};
        QuadTree qt;
        XMFLOAT3 cam_eye;

        XMStoreFloat3(&cam_eye, cam.GetEye());
        auto start = std::chrono::high_resolution_clock::now();

        qt.Build(center, fullSizeXZ, float3(cam_eye.x, cam_eye.y, cam_eye.z));

        auto end = std::chrono::high_resolution_clock::now();
        QuadTreeBuildTime =
            std::chrono::duration_cast<decltype(QuadTreeBuildTime)>(end -
                                                                    start);

        qtNodes = qt.GetSize();

        auto worldBasic = XMMatrixRotationX(XMConvertToRadians(-90.0));
        start = std::chrono::high_resolution_clock::now();
        for (auto it = qt.begin(); it != qt.end(); ++it) {
          NavigatingTheQuadTree +=
              std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                  (std::chrono::high_resolution_clock::now() - start));

          {
            auto t1 = it->size;
            auto t2 = it->center;
            auto t3 = float3(t2.x, center.y, t2.y);
            auto world =
                worldBasic *
                XMMatrixScaling(it->size.x / fullSizeXZ.x, 1,
                                it->size.y / fullSizeXZ.y) *
                XMMatrixTranslation(it->center.x, center.y, it->center.y);
            auto worldViewProjection =
                XMMatrixTranspose(world * cam.GetViewProj());
            auto worldIT = XMMatrixInverse(nullptr, world);

            XMStoreFloat4x4(&vertexConstants.WorldViewProjection,
                            worldViewProjection);
            XMStoreFloat4x4(&domainConstants.WorldIT, worldIT);

            float2 uvbottomleft =
                float2(it->center.x - center.x, it->center.y - center.y) /
                    fullSizeXZ +
                float2(0.5, 0.5) - (it->size / fullSizeXZ) * 0.5;
            auto texturetransf =
                XMMatrixScaling(it->size.x / fullSizeXZ.x, 1,
                                it->size.y / fullSizeXZ.y) *
                XMMatrixTranslation(uvbottomleft.x, 0, uvbottomleft.y);
            // Why no 2x2?
            XMStoreFloat4x4(&domainConstants.TextureTransform, texturetransf);
          }
          {
            start = std::chrono::high_resolution_clock::now();

            auto res = it.GetSmallerNeighbor();

            NavigatingTheQuadTree +=
                std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                    (std::chrono::high_resolution_clock::now() - start));
            static const constexpr auto l = [](float x) -> float {
              if (x == 0)
                return 1;
              else
                return 1 / x;
            };
            hullConstants.TesselationFactor = {l(res.zneg), l(res.xneg),
                                               l(res.zpos), l(res.xpos)};
          }

          auto mask =
              simpleRootSignature.Set(allocator, RootSignatureUsage::Graphics);
          mask.HullBuffer = resources.DynamicBuffer.AddBuffer(hullConstants);
          mask.VertexBuffer =
              resources.DynamicBuffer.AddBuffer(vertexConstants);
          mask.DomainBuffer =
              resources.DynamicBuffer.AddBuffer(domainConstants);
          mask.Texture = texture;
          // mask.Texture = *heightMap.ShaderResource();
          //  mask.Heightmap = heightmap;
          //  mask.HeightMapForDomain = heightmap;
          mask.Heightmap = *heightMap.ShaderResource();
          mask.HeightMapForDomain = *heightMap.ShaderResource();

          allocator.SetRenderTargets({renderTargetView},
                                     resources.DepthBuffer.DepthStencil());
          simplePipelineState.Apply(allocator);
          planeMesh.Draw(allocator);

          start = std::chrono::high_resolution_clock::now();
        }
      }

      // ImGUI
      {

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplUwp_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Application")) {
          ImGui::ColorEdit3("clear color", (float *)&clearColor);

          ImGui::Text("frameID = %d", i);
          ImGui::Text("QuadTree Nodes = %d", qtNodes);

          ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                      1000.0f / io.Framerate, io.Framerate);
          ImGui::Text("QuadTree buildtime %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          QuadTreeBuildTime));
          ImGui::Text("Navigating %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          NavigatingTheQuadTree));
          ImGui::Text("Since start %.3f ms",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          getTimeSinceStart()));

          static const char *items[] = {"CullClockwise", "CullNone",
                                        "WireFrame"};
          static uint itemsIndex = 0;

          if (ImGui::BeginCombo("Render State", items[itemsIndex])) {
            for (uint i = 0; i < IM_ARRAYSIZE(items); i++) {
              bool isSelected = (itemsIndex == i);
              if (ImGui::Selectable(items[i], isSelected)) {
                itemsIndex = i;
                switch (itemsIndex) {
                case 0:
                  rasterizerFlags = RasterizerFlags::CullClockwise;
                  break;
                case 1:
                  rasterizerFlags = RasterizerFlags::CullNone;
                  break;
                case 2:
                  rasterizerFlags = RasterizerFlags::Wireframe;
                  break;
                }
                simplePipelineStateDefinition.RasterizerState.Flags =
                    rasterizerFlags;
                simplePipelineState =
                    pipelineStateProvider
                        .CreatePipelineStateAsync(simplePipelineStateDefinition)
                        .get();
              }
              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          if (cam.GetFirstPerson()) {
            XMVECTOR camEye = cam.GetEye();
            if (ImGui::InputFloat3("Cam eye ", (float *)&camEye, "%.3f")) {
              cam.SetView(camEye, cam.GetAt(), cam.GetWorldUp());
            }
          } else {

            XMVECTOR camLookAt = cam.GetAt();
            if (ImGui::InputFloat3("Cam Look At ", (float *)&camLookAt,
                                   "%.3f")) {
              cam.SetView(cam.GetEye(), camLookAt, cam.GetWorldUp());
            }
          }
        }
        ImGui::End();

        ImGui::Render();

        allocator->SetDescriptorHeaps(1, &ImGuiDescriptorHeap);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
                                      allocator.operator->());
      }

      // End frame command list
      {
        allocator.TransitionResource(*renderTargetView,
                                     ResourceStates::RenderTarget,
                                     ResourceStates::Present);
        auto drawCommandList = allocator.EndList();

        allocator.BeginList();
        resources.DynamicBuffer.UploadResources(allocator);
        resourceUploader.UploadResourcesAsync(allocator);
        auto initCommandList = allocator.EndList();

        directQueue.Execute(initCommandList);
        directQueue.Execute(drawCommandList);
        resources.Marker = resources.Fence.EnqueueSignal(directQueue);
      }

      // Present frame
      swapChain.Present();
    }
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplUwp_Shutdown();
    ImGui::DestroyContext();
  }

  void SetWindow(CoreWindow const & /*window*/) {}
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

  CoreApplication::Run(make<App>());
}