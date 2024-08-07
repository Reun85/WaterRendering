#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "ConstantGPUBuffer.h"
#include "Defaults.h"
#include "WaterMath.h"
#include "Helpers.h"
#include "pix3.h"

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

  struct SimpleGraphicsRootDescription : public RootSignatureMask {
    struct DomainConstants {
      XMFLOAT4X4 ViewProjTransform;
      int useDisplacement;
    };

    struct VertexConstants {
      XMFLOAT4X4 WorldTransform;
      XMFLOAT4X4 ViewTransform;
      // This cannot be center and size, otherwise we could not rotate the thing
      // But it does require that the plane perpendicular to the y axis
      // If not, rewrite this to use float3s.
      XMFLOAT2 PlaneBottomLeft;
      XMFLOAT2 PlaneTopRight;
    };
    struct HullConstants {
      // zneg,xneg, zpos, xpos
      XMFLOAT4 TesselationFactor;
    };

    struct PixelConstants {
      XMFLOAT4 cameraPos;
      XMFLOAT4 mult;
      XMUINT4 swizzleorder;
      int useTexture;
    };

    RootDescriptor<RootDescriptorType::ConstantBuffer> VertexBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> HullBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> DomainBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> PixelBuffer;
    RootDescriptorTable<1> Texture;
    StaticSampler TextureSampler;
    RootDescriptorTable<1> HeightMapForDomain;
    StaticSampler HeightmapSamplerForDomain;
    RootDescriptorTable<1> GradientsForDomain;

    explicit SimpleGraphicsRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          VertexBuffer(this, {0}, ShaderVisibility::Vertex),
          HullBuffer(this, {0}, ShaderVisibility::Hull),
          DomainBuffer(this, {0}, ShaderVisibility::Domain),
          PixelBuffer(this, {0}, ShaderVisibility::Pixel),
          Texture(this, {DescriptorRangeType::ShaderResource},
                  ShaderVisibility::Pixel),
          TextureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                         ShaderVisibility::Pixel),
          HeightMapForDomain(this, {DescriptorRangeType::ShaderResource, {0}},
                             ShaderVisibility::Domain),
          HeightmapSamplerForDomain(this, {0}, Filter::Linear,
                                    TextureAddressMode::Clamp,
                                    ShaderVisibility::Domain),
          GradientsForDomain(this, {DescriptorRangeType::ShaderResource, {1}},
                             ShaderVisibility::Domain) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct SimulationStage {
    struct SpektrumRootDescription : public RootSignatureMask {
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
    struct GradientDescription : public RootSignatureMask {
      // In
      RootDescriptorTable<1> Displacement;
      // Out
      RootDescriptorTable<1> Output;

      explicit GradientDescription(const RootSignatureContext &context)
          : RootSignatureMask(context),
            Displacement(this, {DescriptorRangeType::ShaderResource, 0}),
            Output(this, {DescriptorRangeType::UnorderedAccess, 0}) {
        Flags = RootSignatureFlags::None;
      }
    };

    struct SimulationResources {
      CommandAllocator Allocator;
      CommandFence Fence;
      CommandFenceMarker FrameDoneMarker;
      DynamicBufferManager DynamicBuffer;

      MutableTextureWithState tildeh;
      MutableTextureWithState tildeD;
#define useDifferentFFTOutputBuffers true
#if useDifferentFFTOutputBuffers
      MutableTextureWithState FFTTildeh;
      MutableTextureWithState FFTTildeD;
#else
      MutableTextureWithState &FFTTildeh = tildeh;
      MutableTextureWithState &FFTTildeD = tildeD;
#endif
      MutableTextureWithState tildehBuffer;
      MutableTextureWithState tildeDBuffer;

      MutableTextureWithState finalDisplacementMap;
      MutableTextureWithState gradients;

      explicit SimulationResources(const ResourceAllocationContext &context,
                                   const u32 N, const u32 M)
          : Allocator(*context.Device),

            Fence(*context.Device), DynamicBuffer(*context.Device),
            tildeh(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
            tildeD(context, TextureDefinition::TextureDefinition(
                                Format::R32G32_Float, N, M, 0,
                                TextureFlags::UnorderedAccess)),
#if useDifferentFFTOutputBuffers
            FFTTildeh(context, TextureDefinition::TextureDefinition(
                                   Format::R32G32_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)),
            FFTTildeD(context, TextureDefinition::TextureDefinition(
                                   Format::R32G32_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess)),
#endif
            tildehBuffer(context, TextureDefinition::TextureDefinition(
                                      Format::R32G32_Float, N, M, 0,
                                      TextureFlags::UnorderedAccess)),
            tildeDBuffer(context, TextureDefinition::TextureDefinition(
                                      Format::R32G32_Float, N, M, 0,
                                      TextureFlags::UnorderedAccess)),
            finalDisplacementMap(context,
                                 TextureDefinition::TextureDefinition(
                                     Format::R32G32B32A32_Float, N, M, 0,
                                     TextureFlags::UnorderedAccess)),

            gradients(context, TextureDefinition::TextureDefinition(
                                   Format::R16G16B16A16_Float, N, M, 0,
                                   TextureFlags::UnorderedAccess))

      {
      }
    };
  };

  struct RuntimeSettings {
    enum class Mode : u8 {
      Full = 0,
      Tildeh0,
      Frequencies,
      Tildeh,
      TildeD,
      FFTTildeh,
      FFTTildeD,
      Displacement,
      Gradients,
      TildehBuffer,
      TildeDBuffer
    };
    bool firstPerson = Defaults::Cam::startFirstPerson;
    bool timeRunning = true;
    XMFLOAT4 clearColor = Defaults::App::clearColor;
    XMFLOAT4 pixelMult = XMFLOAT4(1, 1, 1, 1);
    Mode mode = Mode::Tildeh0;
    XMUINT4 swizzleorder = XMUINT4(0, 1, 2, 3);
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
    RuntimeSettings settings;

    cam.SetFirstPerson(settings.firstPerson);
    // Events

    bool &timerunning = settings.timeRunning;
    {
      window.KeyDown([&cam, &quit, &timerunning](CoreWindow const &,
                                                 KeyEventArgs const &args) {
        if (ImGui::GetIO().WantCaptureKeyboard)
          return;
        switch (args.VirtualKey()) {
        case Windows::System::VirtualKey::Escape:
          quit = true;
          break;
        case Windows::System::VirtualKey::Space:
          timerunning = !timerunning;
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
    RootSignature<SimpleGraphicsRootDescription> simpleRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"SimpleVertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"PixelShader.cso"};
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
        .InputLayout = VertexPosition::Layout,
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

    RootSignature<SimulationStage::GradientDescription> gradientRootDescription{
        device};
    ComputeShader gradient{app_folder() / L"gradient.cso"};
    ComputePipelineStateDefinition gradientRootStateDefinition{
        .RootSignature = &gradientRootDescription,
        .ComputeShader = &gradient,
    };
    auto gradientPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(gradientRootStateDefinition)
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
    std::random_device rd;

    ImmutableTexture computeTildeh0 =
        ImmutableTextureFromData<std::complex<f32>>(
            immutableAllocationContext, Format::R32G32_Float, N, N, 0u,
            CalculateTildeh0FromDefaults<f32>(rd, N, N));

    ImmutableTexture computeFrequencies = ImmutableTextureFromData<f32>(
        immutableAllocationContext, Format::R32_Float, N, N, 0u,
        CalculateFrequenciesFromDefaults<f32>(N, N));

    ImmutableMesh planeMesh{immutableAllocationContext, CreateQuadPatch()};

    ImmutableTexture gradientTexture{immutableAllocationContext,
                                     app_folder() / L"gradient.jpg"};

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    //  Acquire memory
    groupedResourceAllocator.Build();

    array<FrameResources, 2> frameResources{
        FrameResources(mutableAllocationContext),
        FrameResources(mutableAllocationContext)};

    array<SimulationStage::SimulationResources, 2> simulationResources{
        SimulationStage::SimulationResources(mutableAllocationContext, N, N),
        SimulationStage::SimulationResources(mutableAllocationContext, N, N)};

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
    float gameTime = 0;
    // Frame counter
    auto frameCounter = 0u;
    std::chrono::steady_clock::time_point frameStart =
        std::chrono::high_resolution_clock::now();
    while (!quit) {
      // Process user input
      dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

      // Get frame frameResource
      frameCounter++;
      // Current frames resources
      auto &frameResource = frameResources[frameCounter & 0x1u];
      // Simulation resources for drawing.
      auto &drawingSimResource = simulationResources[frameCounter & 0x1u];
      // Simulation resources for calculating.
      auto &calculatingSimResource =
          simulationResources[(frameCounter + 1u) & 0x1u];
      auto renderTargetView = swapChain.RenderTargetView();

      // Wait until buffers can be used
      if (frameResource.Marker)
        frameResource.Fence.Await(frameResource.Marker);
      if (drawingSimResource.FrameDoneMarker)
        drawingSimResource.Fence.Await(drawingSimResource.FrameDoneMarker);

      // Get DeltaTime
      float deltaTime;
      {
        auto newFrameStart = std::chrono::high_resolution_clock::now();
        deltaTime = GetDurationInFloatWithPrecision<std::chrono::seconds,
                                                    std::chrono::milliseconds>(
            newFrameStart - frameStart);
        frameStart = newFrameStart;
        if (timerunning) {
          gameTime += deltaTime;
        }
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
      // Frame Begin
      {
        committedResourceAllocator.Build();
        depthStencilDescriptorHeap.Build();
        commonDescriptorHeap.Build();
      }

      // Compute shader phase
      {
        auto &simResource = calculatingSimResource;
        auto &computeAllocator = simResource.Allocator;

        computeAllocator.Reset();
        computeAllocator.BeginList();
        commonDescriptorHeap.Set(computeAllocator);
        // Spektrums
        {
          SimulationStage::SpektrumRootDescription::InputConstants constant{};
          constant.time = gameTime;

          spektrumPipelineState.Apply(computeAllocator);
          auto mask = spektrumRootDescription.Set(computeAllocator,
                                                  RootSignatureUsage::Compute);
          // Inputs
          mask.InputBuffer = simResource.DynamicBuffer.AddBuffer(constant);
          mask.Tildeh0 = computeTildeh0;
          mask.Frequencies = computeFrequencies;

          // Outputs
          mask.Tildeh = *simResource.tildeh.UnorderedAccess(computeAllocator);
          mask.TildeD = *simResource.tildeD.UnorderedAccess(computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = N;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);
        }
        //  FFT
        {
          // TILDE h
          {
            // Stage1
            {
              FFTPipelineState.Apply(computeAllocator);
              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input = *simResource.tildeh.ShaderResource(computeAllocator);
              mask.Output =
                  *simResource.tildehBuffer.UnorderedAccess(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = N;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);
            }

            // TILDE h
            // Stage2
            {
              FFTPipelineState.Apply(computeAllocator);
              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input =
                  *simResource.tildehBuffer.ShaderResource(computeAllocator);
              mask.Output =
                  *simResource.FFTTildeh.UnorderedAccess(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = N;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);
            }
          }

          // TILDE D
          {
            // Stage1
            {
              FFTPipelineState.Apply(computeAllocator);
              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input = *simResource.tildeD.ShaderResource(computeAllocator);
              mask.Output =
                  *simResource.tildeDBuffer.UnorderedAccess(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = N;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);
            }

            // TILDE D
            // Stage2
            {
              FFTPipelineState.Apply(computeAllocator);
              auto mask = FFTRootDescription.Set(computeAllocator,
                                                 RootSignatureUsage::Compute);

              mask.Input =
                  *simResource.tildeDBuffer.ShaderResource(computeAllocator);
              mask.Output =
                  *simResource.FFTTildeD.UnorderedAccess(computeAllocator);

              const auto xGroupSize = 1;
              const auto yGroupSize = 1;
              const auto sizeX = N;
              const auto sizeY = 1;
              computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                        (sizeY + yGroupSize - 1) / yGroupSize,
                                        1);
            }
          }
        }

        // Calculate final displacements
        {
          displacementPipelineState.Apply(computeAllocator);
          auto mask = displacementRootDescription.Set(
              computeAllocator, RootSignatureUsage::Compute);

          mask.Height = *simResource.FFTTildeh.ShaderResource(computeAllocator);
          mask.Choppy = *simResource.FFTTildeD.ShaderResource(computeAllocator);
          mask.Output = *simResource.finalDisplacementMap.UnorderedAccess(
              computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = N;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);
        }

        // Calculate gradients
        {
          gradientPipelineState.Apply(computeAllocator);
          auto mask = gradientRootDescription.Set(computeAllocator,
                                                  RootSignatureUsage::Compute);

          mask.Displacement = *simResource.finalDisplacementMap.ShaderResource(
              computeAllocator);
          mask.Output =
              *simResource.gradients.UnorderedAccess(computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = N;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);
        }
        // Upload queue
        {
          PIXScopedEvent(computeQueue.get(), 0, "asdadwadsdawddawdsdzxc");
          auto commandList = computeAllocator.EndList();
          computeAllocator.BeginList();
          simResource.DynamicBuffer.UploadResources(computeAllocator);
          resourceUploader.UploadResourcesAsync(computeAllocator);
          auto initCommandList = computeAllocator.EndList();

          computeQueue.Execute(initCommandList);
          computeQueue.Execute(commandList);

          simResource.FrameDoneMarker =
              simResource.Fence.EnqueueSignal(computeQueue);
        }
      }

      // Graphics Stage

      auto &allocator = frameResource.Allocator;
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
        renderTargetView->Clear(allocator, settings.clearColor);
        frameResource.DepthBuffer.DepthStencil()->Clear(allocator);
      }
      // Draw objects
      uint qtNodes;
      uint drawnNodes = 0;
      std::chrono::nanoseconds QuadTreeBuildTime(0);

      std::chrono::nanoseconds NavigatingTheQuadTree(0);

      {
        SimpleGraphicsRootDescription::VertexConstants vertexConstants{};
        SimpleGraphicsRootDescription::HullConstants hullConstants{};
        SimpleGraphicsRootDescription::DomainConstants domainConstants{};
        SimpleGraphicsRootDescription::PixelConstants pixelConstants{};
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

        const i32 displaycount = 1;
        for (i32 idc = -(displaycount - 1); idc <= displaycount / 2; ++idc) {
          for (i32 jdc = -(displaycount - 1); jdc <= displaycount / 2; ++jdc) {
            auto worldBasic =
                XMMatrixTranslation(idc * Defaults::App::planeSize, 0,
                                    jdc * Defaults::App::planeSize);

            XMVECTOR planeBottomLeft =
                XMVectorSet(-fullSizeXZ.x / 2, 0, -fullSizeXZ.y / 2, 1);
            XMVECTOR planeTopRight =
                XMVectorSet(fullSizeXZ.x / 2, 0, fullSizeXZ.y / 2, 1);
            planeBottomLeft =
                XMVector3TransformCoord(planeBottomLeft, worldBasic);
            planeTopRight = XMVector3TransformCoord(planeTopRight, worldBasic);
            // Reorder to float2
            planeBottomLeft = XMVectorSwizzle(planeBottomLeft, 0, 2, 1, 3);
            planeTopRight = XMVectorSwizzle(planeTopRight, 0, 2, 1, 3);

            start = std::chrono::high_resolution_clock::now();
            for (auto it = qt.begin(); it != qt.end(); ++it) {
              drawnNodes++;
              NavigatingTheQuadTree +=
                  std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                      (std::chrono::high_resolution_clock::now() - start));

              {
                auto world =
                    XMMatrixScaling(it->size.x, 1, it->size.y) *
                    XMMatrixTranslation(it->center.x, center.y, it->center.y) *
                    worldBasic;
                auto ViewProjection = cam.GetViewProj();
                // auto worldIT = XMMatrixInverse(nullptr, world);

                XMStoreFloat4x4(&vertexConstants.WorldTransform,
                                XMMatrixTranspose(world));
                XMStoreFloat4x4(&vertexConstants.ViewTransform,
                                XMMatrixTranspose(ViewProjection));

                XMStoreFloat4x4(&domainConstants.ViewProjTransform,
                                XMMatrixTranspose(ViewProjection));

                // Why no 2x2?
                XMStoreFloat2(&vertexConstants.PlaneBottomLeft,
                              planeBottomLeft);
                XMStoreFloat2(&vertexConstants.PlaneTopRight, planeTopRight);

                XMStoreFloat4(&pixelConstants.cameraPos, cam.GetEye());
                pixelConstants.mult = settings.pixelMult;
              }
              {
                start = std::chrono::high_resolution_clock::now();

                auto res = it.GetSmallerNeighbor();

                NavigatingTheQuadTree +=
                    std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                        (std::chrono::high_resolution_clock::now() - start));
                static const constexpr auto l = [](const float x) -> float {
                  if (x == 0)
                    return 1;
                  else
                    return 1 / x;
                };
                hullConstants.TesselationFactor = {l(res.zneg), l(res.xneg),
                                                   l(res.zpos), l(res.xpos)};
              }

              auto mask = simpleRootSignature.Set(allocator,
                                                  RootSignatureUsage::Graphics);
              mask.HeightMapForDomain =
                  *drawingSimResource.finalDisplacementMap.ShaderResource(
                      allocator);

              pixelConstants.swizzleorder = settings.swizzleorder;

              domainConstants.useDisplacement = 1;
              pixelConstants.useTexture = 0;
              switch (settings.mode) {
              case RuntimeSettings::Mode::Full:
                pixelConstants.useTexture = 1;
                domainConstants.useDisplacement = 0;
                break;

              case RuntimeSettings::Mode::Tildeh0:
                mask.Texture = computeTildeh0;
                break;
              case RuntimeSettings::Mode::Frequencies:
                mask.Texture = computeFrequencies;
                break;
              case RuntimeSettings::Mode::Tildeh:
                mask.Texture =
                    *drawingSimResource.tildeh.ShaderResource(allocator);
                break;

              case RuntimeSettings::Mode::TildeD:
                mask.Texture =
                    *drawingSimResource.tildeD.ShaderResource(allocator);
                break;
              case RuntimeSettings::Mode::FFTTildeh:
                mask.Texture =
                    *drawingSimResource.FFTTildeh.ShaderResource(allocator);
                break;
              case RuntimeSettings::Mode::FFTTildeD:
                mask.Texture =
                    *drawingSimResource.FFTTildeD.ShaderResource(allocator);
                break;
              case RuntimeSettings::Mode::Displacement:
                mask.Texture =
                    *drawingSimResource.finalDisplacementMap.ShaderResource(
                        allocator);
                break;
              case RuntimeSettings::Mode::Gradients:
                mask.Texture =
                    *drawingSimResource.gradients.ShaderResource(allocator);
                break;
              case RuntimeSettings::Mode::TildehBuffer:
                mask.Texture =
                    *drawingSimResource.tildehBuffer.ShaderResource(allocator);
                break;
              case RuntimeSettings::Mode::TildeDBuffer:
                mask.Texture =
                    *drawingSimResource.tildeDBuffer.ShaderResource(allocator);
                break;
              }

              mask.HullBuffer =
                  frameResource.DynamicBuffer.AddBuffer(hullConstants);
              mask.VertexBuffer =
                  frameResource.DynamicBuffer.AddBuffer(vertexConstants);
              mask.DomainBuffer =
                  frameResource.DynamicBuffer.AddBuffer(domainConstants);
              mask.PixelBuffer =
                  frameResource.DynamicBuffer.AddBuffer(pixelConstants);
              mask.GradientsForDomain =
                  *drawingSimResource.gradients.ShaderResource(allocator);

              allocator.SetRenderTargets(
                  {renderTargetView}, frameResource.DepthBuffer.DepthStencil());
              graphicsPipelineState.Apply(allocator);
              planeMesh.Draw(allocator);

              start = std::chrono::high_resolution_clock::now();
            }
          }
        }
      }

      auto CPURenderEnd = std::chrono::high_resolution_clock::now();
      // ImGUI
      {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplUwp_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Application")) {
          ImGui::Text("Press ESC to quit");
          ImGui::Text("Press Space to stop time");
          ImGui::ColorEdit3("clear color", (float *)&settings.clearColor);

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
          ImGui::Text("Drawn Nodes: %d", drawnNodes);
          ImGui::Text("CPU time %.3f ms/frame",
                      GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                      std::chrono::nanoseconds>(
                          (CPURenderEnd - frameStart)));
          ImGui::Text(
              "Since start %.3f s",
              GetDurationInFloatWithPrecision<std::chrono::seconds,
                                              std::chrono::milliseconds>(
                  getTimeSinceStart()));
          static const std::array<std::string, 11> modeitems = {
              "Full",      "Tildeh0",      "Frequencies", "Tildeh",
              "TildeD",    "FFTTildeh",    "FFTTildeD",   "Displacement",
              "Gradients", "TildehBuffer", "TildeDBuffer"};

          if (ImGui::BeginCombo("Mode",
                                modeitems[(u32)(settings.mode)].c_str())) {
            for (uint i = 0; i < modeitems.size(); i++) {
              bool isSelected = ((u32)(settings.mode) == i);
              if (ImGui::Selectable(modeitems[i].c_str(), isSelected)) {
                settings.mode = RuntimeSettings::Mode(i);
                settings.swizzleorder = XMUINT4(0, 1, 2, 3);
              }
              if (isSelected) {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }
          if (settings.mode != RuntimeSettings::Mode::Full) {
            ImGui::InputFloat3("Pixel Mult", (float *)&settings.pixelMult);

            static const std::array<std::string, 4> swizzleitems = {"r", "g",
                                                                    "b", "a"};

            const std::array<u32 *const, 4> vals = {
                &settings.swizzleorder.x, &settings.swizzleorder.y,
                &settings.swizzleorder.z, &settings.swizzleorder.w};
            ImGui::Text("Swizzle");
            for (int i = 0; i < 4; i++) {
              const string id = "##Swizzle" + swizzleitems[i];
              ImGui::Text("%s", swizzleitems[i].c_str());
              ImGui::SameLine();

              if (ImGui::BeginCombo(id.c_str(),
                                    swizzleitems[*vals[i]].c_str())) {
                for (size_t j = 0; j < swizzleitems.size(); j++) {
                  bool isSelected = (*(vals[i]) == j);
                  if (ImGui::Selectable(swizzleitems[j].c_str(), isSelected)) {
                    *vals[i] = j;
                  }
                  if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                  }
                }
                ImGui::EndCombo();
              }
            }
          }

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

          if (ImGui::Checkbox("Firstperson", &settings.firstPerson))
            cam.SetFirstPerson(settings.firstPerson);

          ImGui::Checkbox("Time running", &settings.timeRunning);

          XMVECTOR camEye = cam.GetEye();
          if (ImGui::InputFloat3("Cam eye ", (float *)&camEye, "%.3f"))
            cam.SetView(camEye, cam.GetAt(), cam.GetWorldUp());

          float camSpeed = cam.GetBaseSpeed();
          if (ImGui::SliderFloat("Cam speed", &camSpeed, 0, 10))
            cam.SetBaseSpeed(camSpeed);

          if (!cam.GetFirstPerson()) {
            XMVECTOR camLookAt = cam.GetAt();
            if (ImGui::InputFloat3("Cam Look At ", (float *)&camLookAt, "%.3f"))
              cam.SetView(cam.GetEye(), camLookAt, cam.GetWorldUp());

            float distance = cam.GetDistance();
            if (ImGui::SliderFloat("Cam distance", &distance, 0, 100))
              cam.SetDistanceFromAt(distance);
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