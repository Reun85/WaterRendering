#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "ConstantGPUBuffer.h"
#include "Defaults.h"
#include "WaterMath.h"

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
  using Result = std::ratio_divide<PrecisionRep, TimeRep>;
  using Precision = std::chrono::duration<T, PrecisionRep>;
  const T count = std::chrono::duration_cast<Precision>(inp).count();
  return static_cast<float>(count) * static_cast<float>(Result::num) /
         static_cast<float>(Result::den);
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

template <typename DataType>
ImmutableTexture
ImmutableTextureFromData(const ResourceAllocationContext &context,
                         const Format &f, const u32 width, const u32 height,
                         const u16 arraySize, std::span<const DataType> data) {
  auto text = TextureData(f, width, height, arraySize);
  std::span<u8> span = text.AsRawSpan();
  assert(span.size_bytes() == data.size_bytes());

  std::memcpy(span.data(), data.data(), data.size_bytes());
  return ImmutableTexture{context, text};
};
MeshDescription CreateQuadPatch(float size) {
  size = size / 2;

  MeshDescription result;

  result.Vertices = {
      VertexPositionTexture{XMFLOAT3{-size, size, 0.f}, XMUSHORTN2{0.f, 0.f}},
      VertexPositionTexture{XMFLOAT3{-size, -size, 0.f}, XMUSHORTN2{0.f, 1.f}},
      VertexPositionTexture{XMFLOAT3{size, size, 0.f}, XMUSHORTN2{1.f, 0.f}},
      VertexPositionTexture{XMFLOAT3{size, -size, 0.f}, XMUSHORTN2{1.f, 1.f}}};

  result.Topology = static_cast<PrimitiveTopology>(
      D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);

  return result;
}

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
  IFrameworkView CreateView() const { return *this; }
  void Initialize(CoreApplicationView const &) const {
    // ?
  }

  void Load(hstring const &) const {
    // ?
  }

  void Uninitialize() const {
    // ?
  }

  struct FrameResources {
    CommandAllocator Allocator;
    CommandFence Fence;
    CommandFenceMarker Marker;
    DynamicBufferManager DynamicBuffer;

    MutableTexture DepthBuffer;
    descriptor_ptr<ShaderResourceView> ScreenResourceView;

    explicit FrameResources(const ResourceAllocationContext &context)
        : Allocator(*context.Device), Fence(*context.Device),
          DynamicBuffer(*context.Device), DepthBuffer(context) {}
  };

  struct SimpleGraphicRootDescription : public RootSignatureMask {
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

    explicit SimpleGraphicRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          VertexBuffer(this, {0}, ShaderVisibility::Vertex),
          HullBuffer(this, {0}, ShaderVisibility::Hull),
          DomainBuffer(this, {0}, ShaderVisibility::Domain),
          Texture(this, {DescriptorRangeType::ShaderResource},
                  ShaderVisibility::Pixel),
          TextureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                         ShaderVisibility::Pixel),
          Heightmap(this, {DescriptorRangeType::ShaderResource},
                    ShaderVisibility::Hull),
          HeightmapSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                           ShaderVisibility::Hull),
          HeightMapForDomain(this, {DescriptorRangeType::ShaderResource},
                             ShaderVisibility::Domain),
          HeightmapSamplerForDomain(this, {0}, Filter::Linear,
                                    TextureAddressMode::Clamp,
                                    ShaderVisibility::Domain) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct SimulationStage {
    struct SpektrumRootDescription : public RootSignatureMask {
      // How does it know, where its binded?
      // In
      RootDescriptorTable<1> Tildeh0;
      RootDescriptorTable<1> Frequencies;
      struct InputConstants {
        float time;
      };
      RootDescriptor<RootDescriptorType::ConstantBuffer> InputBuffer;
      // Out
      RootDescriptorTable<1> Tildeh;
      RootDescriptorTable<1> TildeD;

      explicit SpektrumRootDescription(const RootSignatureContext &context)
          : RootSignatureMask(context),
            Tildeh0(this, {DescriptorRangeType::ShaderResource, 0}),
            Frequencies(this, {DescriptorRangeType::ShaderResource, 1}),
            InputBuffer(this, {0}),
            Tildeh(this, {DescriptorRangeType::UnorderedAccess, 0}),
            TildeD(this, {DescriptorRangeType::UnorderedAccess, 1}) {
        Flags = RootSignatureFlags::None;
      }
    };
    struct FFTDescription : public RootSignatureMask {
      // In
      RootDescriptorTable<1> Input;
      // Out
      RootDescriptorTable<1> Output;

      explicit FFTDescription(const RootSignatureContext &context)
          : RootSignatureMask(context),
            Input(this, {DescriptorRangeType::ShaderResource, 0}),
            Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
        Flags = RootSignatureFlags::None;
      }
    };
    struct DisplacementDescription : public RootSignatureMask {
      // In
      RootDescriptorTable<1> Height;
      RootDescriptorTable<1> Choppy;
      // Out
      RootDescriptorTable<1> Output;

      explicit DisplacementDescription(const RootSignatureContext &context)
          : RootSignatureMask(context),
            Height(this, {DescriptorRangeType::ShaderResource, 0}),
            Choppy(this, {DescriptorRangeType::ShaderResource, 1}),
            Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
        Flags = RootSignatureFlags::None;
      }
    };
  };

  struct SimulationResources {

    CommandAllocator Allocator;
    CommandFence Fence;
    CommandFenceMarker FrameDoneMarker;
    CommandFenceMarker SpektrumsMarker;
    CommandFenceMarker FFThMarker1;
    CommandFenceMarker FFTDMarker1;
    CommandFenceMarker FFTDMarker2;
    CommandFenceMarker FFThMarker2;
    DynamicBufferManager DynamicBuffer;

    MutableTexture computeTildeh;
    MutableTexture computeTildeD;
    MutableTexture tildehBuffer;
    MutableTexture tildeDBuffer;

    // Just reuse a buffer
    /// <summary>
    ///
    /// </summary>
    MutableTexture finalDisplacementMap;

    explicit SimulationResources(const ResourceAllocationContext &context,
                                 const u32 N, const u32 M)
        : Allocator(*context.Device),

          Fence(*context.Device), DynamicBuffer(*context.Device),
          computeTildeh(context, TextureDefinition::TextureDefinition(
                                     Format::R32G32_Float, N, M, 0,
                                     TextureFlags::UnorderedAccess)),
          computeTildeD(context, TextureDefinition::TextureDefinition(
                                     Format::R32G32_Float, N, M, 0,
                                     TextureFlags::UnorderedAccess)),
          tildehBuffer(context, TextureDefinition::TextureDefinition(
                                    Format::R32G32_Float, N, M, 0,
                                    TextureFlags::UnorderedAccess)),
          tildeDBuffer(context, TextureDefinition::TextureDefinition(
                                    Format::R32G32_Float, N, M, 0,
                                    TextureFlags::UnorderedAccess)),
          finalDisplacementMap(context, TextureDefinition::TextureDefinition(
                                            Format::R8G8B8A8_SNorm, N, M, 0,
                                            TextureFlags::UnorderedAccess))

    {}
  };

  void Run() const {
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    Camera cam;
    cam.SetView(
        XMVectorSet(Defaults::Cam::camStartPos.x, Defaults::Cam::camStartPos.y,
                    Defaults::Cam::camStartPos.z, 0),
        XMVectorSet(0.0f, 0.0f, 0.0f, 0), XMVectorSet(0.0f, 1.0f, 0.0f, 0));
    bool quit = false;
    bool firstperson = Defaults::Cam::startFirstPerson;
    cam.SetFirstPerson(firstperson);
    // Events
    {
      // If I redo the whole system, this could be handled when the callback
      // is made.
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
    CommandQueue computeQueue{device};
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsShaderResource};

    PipelineStateProvider pipelineStateProvider{device};

    // Graphics pipeline
    RootSignature<SimpleGraphicRootDescription> simpleRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"SimpleVertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"SimplePixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

    RasterizerFlags rasterizerFlags = RasterizerFlags::CullClockwise;

    GraphicsPipelineStateDefinition graphicsPipelineStateDefinition{
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
    Axodox::Graphics::D3D12::PipelineState graphicsPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(graphicsPipelineStateDefinition)
            .get();

    // Compute pipeline

    ComputeShader spektrum{app_folder() / L"Spektrums.cso"};

    RootSignature<SimulationStage::SpektrumRootDescription>
        spektrumRootDescription{device};
    ComputePipelineStateDefinition spektrumRootStateDefinition{
        .RootSignature = &spektrumRootDescription,
        .ComputeShader = &spektrum,
    };
    auto spektrumPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(spektrumRootStateDefinition)
            .get();

    ComputeShader FFT{app_folder() / L"FFT.cso"};
    RootSignature<SimulationStage::FFTDescription> FFTRootDescription{device};
    ComputePipelineStateDefinition FFTStateDefinition{
        .RootSignature = &FFTRootDescription,
        .ComputeShader = &FFT,
    };
    auto FFTPipelineState =
        pipelineStateProvider.CreatePipelineStateAsync(FFTStateDefinition)
            .get();

    RootSignature<SimulationStage::DisplacementDescription>
        displacementRootDescription{device};
    ComputeShader displacement{app_folder() / L"displacement.cso"};
    ComputePipelineStateDefinition displacementRootStateDefinition{
        .RootSignature = &displacementRootDescription,
        .ComputeShader = &displacement,
    };
    auto displacementPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(displacementRootStateDefinition)
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

    u32 N = Defaults::Simulation::N;
    u32 M = Defaults::Simulation::M;
    std::random_device rd;

    auto x = CalculateTildeh0FromDefaults<f32>(rd, N, M);
    ImmutableTexture computeTildeh0 =
        ImmutableTextureFromData<std::complex<f32>>(
            immutableAllocationContext, Format::R32G32_Float, N, M, 0u,
            CalculateTildeh0FromDefaults<f32>(rd, N, M));

    ImmutableTexture computeFrequencies = ImmutableTextureFromData<f32>(
        immutableAllocationContext, Format::R32_Float, N, M, 0u,
        CalculateFrequenciesFromDefaults<f32>(N, M));

    ImmutableMesh planeMesh{immutableAllocationContext,
                            CreateQuadPatch(Defaults::App::planeSize)};

    ImmutableTexture displayTexture{immutableAllocationContext,
                                    app_folder() / L"gradient.jpg"};

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    //  Acquire memory
    groupedResourceAllocator.Build();

    array<FrameResources, 2> frameResources{
        FrameResources(mutableAllocationContext),
        FrameResources(mutableAllocationContext)};

    array<SimulationResources, 2> simulationResources{
        SimulationResources(mutableAllocationContext, N, M),
        SimulationResources(mutableAllocationContext, N, M)};

    swapChain.Resizing(
        no_revoke, [&frameResources, &commonDescriptorHeap](SwapChain const *) {
          for (auto &frame : frameResources)
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
          device.get(), static_cast<int>(frameResources.size()),
          DXGI_FORMAT_B8G8R8A8_UNORM, ImGuiDescriptorHeap,
          ImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
          ImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
    ImGuiIO const &io = ImGui::GetIO();

    // Time counter
    using SinceTimeStartTimeFrame = std::chrono::nanoseconds;
    auto loopStartTime = std::chrono::high_resolution_clock::now();
    auto getTimeSinceStart = [&loopStartTime]() {
      return std::chrono::duration_cast<SinceTimeStartTimeFrame>(
          std::chrono::high_resolution_clock::now() - loopStartTime);
    };
    // Frame counter
    auto frameCounter = 0u;
    std::chrono::steady_clock::time_point frameStart =
        std::chrono::high_resolution_clock::now();
    XMFLOAT4 clearColor = Defaults::App::clearColor;
    while (!quit) {
      // Process user input
      dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

      // Get frame frameResource
      frameCounter++;
      // Current frames resources
      auto &frameResource = frameResources[frameCounter & 0x1u];
      // Simulation resources for drawing.
      auto &currSimResource = simulationResources[frameCounter & 0x1u];
      // Simulation resources for calculating.
      auto &nextSimResource = simulationResources[(frameCounter + 1u) & 0x1u];
      auto renderTargetView = swapChain.RenderTargetView();

      // Wait until buffers can be reused
      if (frameResource.Marker)
        frameResource.Fence.Await(frameResource.Marker);
      // if (currSimResource.FrameDoneMarker)
      //   currSimResource.Fence.Await(currSimResource.FrameDoneMarker);

      // Get DeltaTime
      float deltaTime;
      {
        auto newFrameStart = std::chrono::high_resolution_clock::now();
        deltaTime = GetDurationInFloatWithPrecision<std::chrono::seconds,
                                                    std::chrono::milliseconds>(
            newFrameStart - frameStart);
        frameStart = newFrameStart;
      }

      // Ensure depth buffer matches frame size
      if (!frameResource.DepthBuffer ||
          !TextureDefinition::AreSizeCompatible(
              *frameResource.DepthBuffer.Definition(),
              renderTargetView->Definition())) {
        auto depthDefinition =
            renderTargetView->Definition().MakeSizeCompatible(
                Format::D32_Float, TextureFlags::DepthStencil);
        frameResource.DepthBuffer.Allocate(depthDefinition);
      }

      // Ensure screen shader resource view
      if (!frameResource.ScreenResourceView ||
          frameResource.ScreenResourceView->Resource() !=
              renderTargetView->Resource()) {
        Texture screenTexture{renderTargetView->Resource()};
        frameResource.ScreenResourceView =
            commonDescriptorHeap.CreateShaderResourceView(&screenTexture);
      }

      // Update constants
      auto resolution = swapChain.Resolution();
      cam.SetAspect(float(resolution.x) / float(resolution.y));
      cam.Update(deltaTime);

      // Begin frame command list
      auto &allocator = frameResource.Allocator;
      {
        allocator.Reset();
        allocator.BeginList();
        allocator.TransitionResource(*renderTargetView, ResourceStates::Present,
                                     ResourceStates::RenderTarget);
      }
      {
        committedResourceAllocator.Build();
        depthStencilDescriptorHeap.Build();

        commonDescriptorHeap.Build();
      }

      // Compute shader phase
      {
        auto &simResource = nextSimResource;
        auto &computeAllocator = simResource.Allocator;

        computeAllocator.Reset();
        using CommandsList = Axodox::Collections::object_pool_handle<
            Axodox::Graphics::D3D12::CommandList>;
        CommandsList spektrumCommandList;
        CommandsList FFTTildeh0Stage1CommandList;
        CommandsList FFTTildeDStage1CommandList;
        CommandsList FFTTildeh0Stage2CommandList;
        CommandsList FFTTildeDStage2CommandList;
        CommandsList displacementCommandList;
        // Spektrums
        {
          computeAllocator.BeginList();
          commonDescriptorHeap.Set(computeAllocator);
          SimulationStage::SpektrumRootDescription::InputConstants constant{};
          constant.time =
              GetDurationInFloatWithPrecision<std::chrono::seconds,
                                              std::chrono::milliseconds>(
                  getTimeSinceStart());
          auto mask = spektrumRootDescription.Set(computeAllocator,
                                                  RootSignatureUsage::Compute);
          mask.InputBuffer = simResource.DynamicBuffer.AddBuffer(constant);
          mask.Tildeh0 = computeTildeh0;
          mask.Frequencies = computeFrequencies;

          mask.Tildeh = *simResource.computeTildeh.UnorderedAccess();
          mask.TildeD = *simResource.computeTildeD.UnorderedAccess();

          spektrumPipelineState.Apply(computeAllocator);
          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = M;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);

          simResource.SpektrumsMarker = simResource.Fence.CreateMarker();
          computeAllocator.AddSignaler(simResource.SpektrumsMarker);

          spektrumCommandList = computeAllocator.EndList();
        }
        //  FFT
        {

          struct FFTData {
            MutableTexture &Input;
            MutableTexture &Buffer;
            MutableTexture &Output;
            CommandFenceMarker &Stage1Marker;
            CommandFenceMarker &Stage2Marker;
            CommandsList &Stage1CommandList;
            CommandsList &Stage2CommandList;
          };
          static const std::array<FFTData, 2> Data = {
              {FFTData{.Input = simResource.computeTildeh,

                       .Buffer = simResource.tildehBuffer,
                       .Output = simResource.computeTildeh,
                       .Stage1Marker = simResource.FFThMarker1,
                       .Stage2Marker = simResource.FFThMarker2,
                       .Stage1CommandList = FFTTildeh0Stage1CommandList,
                       .Stage2CommandList = FFTTildeh0Stage2CommandList},
               FFTData{.Input = simResource.computeTildeD,
                       .Buffer = simResource.tildeDBuffer,
                       .Output = simResource.computeTildeD,
                       .Stage1Marker = simResource.FFTDMarker1,
                       .Stage2Marker = simResource.FFTDMarker2,
                       .Stage1CommandList = FFTTildeDStage1CommandList,
                       .Stage2CommandList = FFTTildeDStage2CommandList}}};

          for (const auto &val : Data) {

            // Stage1
            {
              computeAllocator.BeginList();
              commonDescriptorHeap.Set(computeAllocator);
              computeAllocator.AddAwaiter(simResource.SpektrumsMarker);
              ;
              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input = *val.Input.ShaderResource();
              mask.Output = *val.Buffer.UnorderedAccess();

              FFTPipelineState.Apply(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = N;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);
              val.Stage1Marker = simResource.Fence.CreateMarker();
              computeAllocator.AddSignaler(val.Stage1Marker);

              val.Stage1CommandList = computeAllocator.EndList();
            }

            // Stage2
            {
              computeAllocator.BeginList();
              commonDescriptorHeap.Set(computeAllocator);

              computeAllocator.AddAwaiter(val.Stage1Marker);

              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input = *val.Buffer.ShaderResource();
              mask.Output = *val.Output.UnorderedAccess();

              FFTPipelineState.Apply(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = M;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);

              computeAllocator.AddSignaler(val.Stage2Marker);

              val.Stage2CommandList = computeAllocator.EndList();
            }
          }
        }
        // Calculate final displacements

        {

          computeAllocator.BeginList();
          commonDescriptorHeap.Set(computeAllocator);

          std::array<CommandFenceMarker, 2> waitFor = {simResource.FFThMarker2,
                                                       simResource.FFTDMarker2};
          for (const auto &marker : waitFor) {
            computeAllocator.AddAwaiter(marker);
          }

          auto mask = displacementRootDescription.Set(
              computeAllocator, RootSignatureUsage::Compute);

          mask.Height = *simResource.computeTildeh.ShaderResource();
          mask.Choppy = *simResource.computeTildeD.ShaderResource();
          mask.Output = *simResource.finalDisplacementMap.UnorderedAccess();

          displacementPipelineState.Apply(computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = M;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);

          displacementCommandList = computeAllocator.EndList();
        }

        // Upload queue
        {

          computeAllocator.BeginList();
          simResource.DynamicBuffer.UploadResources(computeAllocator);
          resourceUploader.UploadResourcesAsync(computeAllocator);
          auto initCommandList = computeAllocator.EndList();

          computeQueue.Execute(initCommandList);
          computeQueue.Execute(spektrumCommandList);
          computeQueue.Execute(FFTTildeh0Stage1CommandList);
          computeQueue.Execute(FFTTildeDStage1CommandList);
          computeQueue.Execute(FFTTildeh0Stage2CommandList);
          computeQueue.Execute(FFTTildeDStage2CommandList);
          computeQueue.Execute(displacementCommandList);

          simResource.FrameDoneMarker =
              simResource.Fence.EnqueueSignal(computeQueue);
        }
      }

      // Graphics Stage

      {

        commonDescriptorHeap.Set(allocator);
        // Clears frame with background color
        renderTargetView->Clear(allocator, clearColor);
        frameResource.DepthBuffer.DepthStencil()->Clear(allocator);
      }
      // Draw objects
      uint qtNodes;
      std::chrono::nanoseconds QuadTreeBuildTime(0);

      std::chrono::nanoseconds NavigatingTheQuadTree(0);

      {
        SimpleGraphicRootDescription::VertexConstants vertexConstants{};
        SimpleGraphicRootDescription::HullConstants hullConstants{};
        SimpleGraphicRootDescription::DomainConstants domainConstants{};
        float3 center = {0, 0, 0};
        float2 fullSizeXZ = {Defaults::App::planeSize,
                             Defaults::App::planeSize};
        QuadTree qt;
        XMFLOAT3 cam_eye;

        XMStoreFloat3(&cam_eye, cam.GetEye());
        auto start = std::chrono::high_resolution_clock::now();

        qt.Build(center, fullSizeXZ, float3(cam_eye.x, cam_eye.y, cam_eye.z));

        QuadTreeBuildTime =
            std::chrono::duration_cast<decltype(QuadTreeBuildTime)>(
                std::chrono::high_resolution_clock::now() - start);

        qtNodes = qt.GetSize();

        auto worldBasic = XMMatrixRotationX(XMConvertToRadians(-90.0));
        start = std::chrono::high_resolution_clock::now();
        for (auto it = qt.begin(); it != qt.end(); ++it) {
          NavigatingTheQuadTree +=
              std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                  (std::chrono::high_resolution_clock::now() - start));

          {
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
          mask.HullBuffer =
              frameResource.DynamicBuffer.AddBuffer(hullConstants);
          mask.VertexBuffer =
              frameResource.DynamicBuffer.AddBuffer(vertexConstants);
          mask.DomainBuffer =
              frameResource.DynamicBuffer.AddBuffer(domainConstants);
          mask.Texture = displayTexture;

          // mask.Texture = computeTildeh0;
          // mask.Texture = *currSimResource.computeTildeh.ShaderResource();
          // mask.Texture = *currSimResource.computeTildeh.ShaderResource();
          // mask.Texture =
          // *currSimResource.finalDisplacementMap.ShaderResource();
          mask.HeightMapForDomain =
              *currSimResource.finalDisplacementMap.ShaderResource();

          allocator.SetRenderTargets({renderTargetView},
                                     frameResource.DepthBuffer.DepthStencil());
          graphicsPipelineState.Apply(allocator);
          planeMesh.Draw(allocator);

          start = std::chrono::high_resolution_clock::now();
        }
      }

      auto CPURenderEnd = std::chrono::high_resolution_clock::now();
      // ImGUI
      {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplUwp_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Application")) {
          ImGui::ColorEdit3("clear color", (float *)&clearColor);

          ImGui::Text("frameID = %d", frameCounter);
          ImGui::Text("QuadTree Nodes = %d", qtNodes);

          ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                      1000.0f / io.Framerate, io.Framerate);
          ImGui::Text("QuadTree buildtime %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          QuadTreeBuildTime));
          ImGui::Text("Navigating QuadTree %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          NavigatingTheQuadTree));
          ImGui::Text("CPU time %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          (CPURenderEnd - frameStart)));
          ImGui::Text(
              "Since start %.3f s",
              GetDurationInFloatWithPrecision<std::chrono::seconds,
                                              std::chrono::milliseconds>(
                  getTimeSinceStart()));

          static const std::array<std::string, 3> items = {
              "CullClockwise", "CullNone", "WireFrame"};
          static uint itemsIndex = 0;

          if (ImGui::BeginCombo("Render State", items[itemsIndex].c_str())) {
            for (uint i = 0; i < items.size(); i++) {
              bool isSelected = (itemsIndex == i);
              if (ImGui::Selectable(items[i].c_str(), isSelected)) {
                itemsIndex = i;
                switch (itemsIndex) {
                case 1:
                  rasterizerFlags = RasterizerFlags::CullNone;
                  break;
                case 2:
                  rasterizerFlags = RasterizerFlags::Wireframe;
                  break;
                default:
                  rasterizerFlags = RasterizerFlags::CullClockwise;
                  break;
                }
                graphicsPipelineStateDefinition.RasterizerState.Flags =
                    rasterizerFlags;
                graphicsPipelineState = pipelineStateProvider
                                            .CreatePipelineStateAsync(
                                                graphicsPipelineStateDefinition)
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
        frameResource.DynamicBuffer.UploadResources(allocator);
        resourceUploader.UploadResourcesAsync(allocator);
        auto initCommandList = allocator.EndList();

        directQueue.Execute(initCommandList);
        directQueue.Execute(drawCommandList);
        frameResource.Marker = frameResource.Fence.EnqueueSignal(directQueue);
      }

      // Present frame
      swapChain.Present();
    }
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplUwp_Shutdown();
    ImGui::DestroyContext();
  }

  void SetWindow(CoreWindow const & /*window*/) const {
    // ?
  }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoreApplication::Run(make<App>());
}