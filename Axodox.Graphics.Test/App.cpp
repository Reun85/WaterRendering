#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "Defaults.h"
#include "WaterMath.h"
#include "Helpers.h"
#include "pix3.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"

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

  struct RuntimeSettings {
    enum class Mode : u8 {
      Full = 0,
      Displacement1,
      Displacement2,
      Displacement3,
      Gradients1,
      Gradients2,
      Gradients3,
    };
    bool firstPerson = Defaults::Cam::startFirstPerson;
    bool timeRunning = true;
    XMFLOAT4 clearColor = Defaults::App::clearColor;
    XMFLOAT4 pixelMult = XMFLOAT4(1, 1, 1, 1);
    Mode mode = Mode::Full;
    XMUINT4 swizzleorder = XMUINT4(0, 1, 2, 3);
    float distanceThreshold = Defaults::QuadTree::distanceThreshold;
  };

  static void SetUpWindowInput(const CoreWindow &window,
                               RuntimeSettings &settings, bool &quit,
                               Camera &cam) {
    bool &timerunning = settings.timeRunning;
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

  static ID3D12DescriptorHeap *
  InitImGui(const Axodox::Graphics::D3D12::GraphicsDevice &device,
            u8 framesInFlight) {
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

  struct NeedToDo {
    std::optional<RasterizerFlags> changeFlag;
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

    SetUpWindowInput(window, settings, quit, cam);

    CoreDispatcher dispatcher = window.Dispatcher();

    GraphicsDevice device{};
    CommandQueue directQueue{device};
    CommandQueue computeQueue{device};
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsShaderResource};

    PipelineStateProvider pipelineStateProvider{device};

    // Graphics pipeline
    RootSignature<SimpleGraphicsRootDescription> simpleRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"VertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"PixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

    RasterizerFlags rasterizerFlags = RasterizerFlags::CullNone;

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

    SimulationStage simulationStage;

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
    RootSignature<SimulationStage::FoamDecayDescription>
        foamDecayRootDescription{device};
    ComputeShader foamDecay{app_folder() / L"foamDecay.cso"};
    ComputePipelineStateDefinition foamDecayRootStateDefinition{
        .RootSignature = &foamDecayRootDescription,
        .ComputeShader = &foamDecay,
    };
    auto foamDecayPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(foamDecayRootStateDefinition)
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
    SimulationData currData{.N = N,
                            .M = N,
                            .windDirection =
                                Defaults::Simulation::WindDirection,
                            .WindForce = Defaults::Simulation::WindForce,
                            .gravity = Defaults::Simulation::gravity,
                            .Amplitude = Defaults::Simulation::Amplitude,
                            .patchSize = Defaults::Simulation::patchSize1,
                            .Depth = Defaults::Simulation::Depth};
    auto tildeh0_1 = CalculateTildeh0<f32>(rd, currData);
    auto frequencies_1 = CalculateFrequencies<f32>(currData);
    currData.patchSize = Defaults::Simulation::patchSize2;
    auto tildeh0_2 = CalculateTildeh0<f32>(rd, currData);
    auto frequencies_2 = CalculateFrequencies<f32>(currData);
    currData.patchSize = Defaults::Simulation::patchSize3;
    auto tildeh0_3 = CalculateTildeh0<f32>(rd, currData);
    auto frequencies_3 = CalculateFrequencies<f32>(currData);

    SimulationStage::ConstantGpuSources simulationConstantSources{
        .Highest =
            SimulationStage::ConstantGpuSources::LODData{
                .Tildeh0 = ImmutableTextureFromData<std::complex<f32>>(
                    immutableAllocationContext, Format::R32G32_Float, N, N, 0u,
                    tildeh0_1),
                .Frequencies = ImmutableTextureFromData<f32>(
                    immutableAllocationContext, Format::R32_Float, N, N, 0u,
                    frequencies_1)},

        .Medium =
            SimulationStage::ConstantGpuSources::LODData{
                .Tildeh0 = ImmutableTextureFromData<std::complex<f32>>(
                    immutableAllocationContext, Format::R32G32_Float, N, N, 0u,
                    tildeh0_2),
                .Frequencies = ImmutableTextureFromData<f32>(
                    immutableAllocationContext, Format::R32_Float, N, N, 0u,
                    frequencies_2)},

        .Lowest = SimulationStage::ConstantGpuSources::LODData{
            .Tildeh0 = ImmutableTextureFromData<std::complex<f32>>(
                immutableAllocationContext, Format::R32G32_Float, N, N, 0u,
                tildeh0_3),
            .Frequencies = ImmutableTextureFromData<f32>(
                immutableAllocationContext, Format::R32_Float, N, N, 0u,
                frequencies_3)}};

    SimulationStage::GpuSources simulationSources{
        .Foam = MutableTexture(
            immutableAllocationContext,
            TextureDefinition::TextureDefinition(
                Format::R32G32_Float, N, N, 0, TextureFlags::UnorderedAccess))};

    ImmutableMesh planeMesh{immutableAllocationContext, CreateQuadPatch()};

    //  Acquire memory
    groupedResourceAllocator.Build();

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

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

    ID3D12DescriptorHeap *ImGuiDescriptorHeap =
        InitImGui(device, frameResources.size());
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
        if (settings.timeRunning) {
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

      // Compute shader stage
      {
        auto &simResource = calculatingSimResource;
        auto &computeAllocator = simResource.Allocator;

        computeAllocator.Reset();
        computeAllocator.BeginList();
        commonDescriptorHeap.Set(computeAllocator);
        SimulationStage::TimeData timeConstants{.deltaTime = deltaTime,
                                                .timeSinceLaunch = gameTime};
        GpuVirtualAddress timeDataBuffer =
            simResource.DynamicBuffer.AddBuffer(timeConstants);
        // Spektrums
        {
          spektrumPipelineState.Apply(computeAllocator);
          auto mask = spektrumRootDescription.Set(computeAllocator,
                                                  RootSignatureUsage::Compute);
          // Inputs
          mask.timeDataBuffer = timeDataBuffer;

          mask.Tildeh0 = simulationConstantSources.Highest.Tildeh0;
          mask.Frequencies = simulationConstantSources.Highest.Frequencies;

          // Outputs
          mask.Tildeh =
              *simResource.Highest.tildeh.UnorderedAccess(computeAllocator);
          mask.TildeD =
              *simResource.Highest.tildeD.UnorderedAccess(computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = N;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);
        }
        //  FFT
        {
          struct FFTData {
            MutableTextureWithState &Input;
            MutableTextureWithState &Output;
          };
          static constexpr const u32 FFTCOUNT = 2;
          std::array<FFTData, FFTCOUNT> fftStage1 = {
              FFTData{simResource.Highest.tildeh,
                      simResource.Highest.tildehBuffer},
              FFTData{simResource.Highest.tildehBuffer,
                      simResource.Highest.FFTTildeh}};
          std::array<FFTData, FFTCOUNT> fftStage2 = {
              FFTData{simResource.Highest.tildeD,
                      simResource.Highest.tildeDBuffer},
              FFTData{simResource.Highest.tildeDBuffer,
                      simResource.Highest.FFTTildeD}};

          const auto doFFTStage =
              [&computeAllocator, &N, &FFTRootDescription](
                  const std::array<FFTData, FFTCOUNT> &stage) {
                for (const auto &data : stage) {
                  data.Input.ShaderResource(computeAllocator);
                }
                for (const auto &data : stage) {
                  auto mask = FFTRootDescription.Set(
                      computeAllocator, RootSignatureUsage::Compute);

                  mask.Input = *data.Input.ShaderResource(computeAllocator);
                  mask.Output = *data.Output.UnorderedAccess(computeAllocator);

                  const auto xGroupSize = 1;
                  const auto yGroupSize = 1;
                  const auto sizeX = N;
                  const auto sizeY = 1;
                  computeAllocator.Dispatch(
                      (sizeX + xGroupSize - 1) / xGroupSize,
                      (sizeY + yGroupSize - 1) / yGroupSize, 1);
                }
              };

          FFTPipelineState.Apply(computeAllocator);
          doFFTStage(fftStage1);
          doFFTStage(fftStage2);
        }

        // Calculate final displacements
        {
          displacementPipelineState.Apply(computeAllocator);
          auto mask = displacementRootDescription.Set(
              computeAllocator, RootSignatureUsage::Compute);

          mask.Height =
              *simResource.Highest.FFTTildeh.ShaderResource(computeAllocator);
          mask.Choppy =
              *simResource.Highest.FFTTildeD.ShaderResource(computeAllocator);
          mask.Output = *simResource.Highest.DisplacementMap.UnorderedAccess(
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

          mask.Displacement =
              *simResource.Highest.DisplacementMap.ShaderResource(
                  computeAllocator);
          mask.Output =
              *simResource.Highest.gradients.UnorderedAccess(computeAllocator);

          const auto xGroupSize = 16;
          const auto yGroupSize = 16;
          const auto sizeX = N;
          const auto sizeY = N;
          computeAllocator.Dispatch((sizeX + xGroupSize - 1) / xGroupSize,
                                    (sizeY + yGroupSize - 1) / yGroupSize, 1);
        }
        // Foam Calculations
        {
          foamDecayPipelineState.Apply(computeAllocator);
          auto mask = foamDecayRootDescription.Set(computeAllocator,
                                                   RootSignatureUsage::Compute);

          computeAllocator.AddUAVBarrier(
              simResource.Highest.gradients.getInnerUnsafe());

          mask.Gradients =
              *simResource.Highest.gradients.UnorderedAccess(computeAllocator);

          mask.Foam = *simulationSources.Foam.UnorderedAccess();
          mask.timeBuffer = timeDataBuffer;

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
      // Draw Ocean
      uint qtNodes = 0;
      uint drawnNodes = 0;
      std::chrono::nanoseconds QuadTreeBuildTime(0);

      std::chrono::nanoseconds NavigatingTheQuadTree(0);

      {
        SimpleGraphicsRootDescription::VertexConstants vertexConstants{};
        SimpleGraphicsRootDescription::HullConstants hullConstants{};
        SimpleGraphicsRootDescription::DomainConstants domainConstants{};
        SimpleGraphicsRootDescription::PixelConstants pixelConstants{};
        SimpleGraphicsRootDescription::cameraConstants cameraConstants{};
        SimpleGraphicsRootDescription::ModelConstants modelConstants{};

        auto modelMatrix = XMMatrixIdentity();

        XMStoreFloat3(&cameraConstants.cameraPos, cam.GetEye());
        XMStoreFloat4x4(&cameraConstants.vMatrix,
                        XMMatrixTranspose(cam.GetViewMatrix()));
        XMStoreFloat4x4(&cameraConstants.pMatrix,
                        XMMatrixTranspose(cam.GetProj()));
        XMStoreFloat4x4(&cameraConstants.vpMatrix,
                        XMMatrixTranspose(cam.GetViewProj()));

        XMStoreFloat4x4(&modelConstants.mMatrix,
                        XMMatrixTranspose(modelMatrix));

        std::optional<GpuVirtualAddress> usedTexture;
        // Debug stuff
        {
          pixelConstants.swizzleorder = settings.swizzleorder;

          domainConstants.useDisplacement = 1;
          pixelConstants.useTexture = 0;

          switch (settings.mode) {
          case RuntimeSettings::Mode::Full:
            pixelConstants.useTexture = 1;
            domainConstants.useDisplacement = 0;
            break;

          case RuntimeSettings::Mode::Displacement1:
            usedTexture =
                *drawingSimResource.Highest.DisplacementMap.ShaderResource(
                    allocator);
            break;
          case RuntimeSettings::Mode::Gradients1:
            usedTexture =
                *drawingSimResource.Highest.gradients.ShaderResource(allocator);
            break;
          }

          pixelConstants.mult = settings.pixelMult;
        }

        // Upload absolute contants
        GpuVirtualAddress cameraConstantBuffer =
            frameResource.DynamicBuffer.AddBuffer(cameraConstants);
        GpuVirtualAddress domainConstantBuffer =
            frameResource.DynamicBuffer.AddBuffer(domainConstants);
        GpuVirtualAddress pixelConstantBuffer =
            frameResource.DynamicBuffer.AddBuffer(pixelConstants);

        GpuVirtualAddress modelBuffer =
            frameResource.DynamicBuffer.AddBuffer(modelConstants);

        // Pre translate resources
        GpuVirtualAddress displacementMap =
            *drawingSimResource.Highest.DisplacementMap.ShaderResource(
                allocator);

        GpuVirtualAddress gradients =
            *drawingSimResource.Highest.gradients.ShaderResource(allocator);

        float2 fullSizeXZ = {Defaults::App::oceanSize,
                             Defaults::App::oceanSize};

        float3 center = {0, 0, 0};
        QuadTree qt;
        XMFLOAT3 camUsedPos;
        XMVECTOR camEye = cam.GetEye();
        XMStoreFloat3(&camUsedPos, camEye);

        auto start = std::chrono::high_resolution_clock::now();

        qt.Build(center, fullSizeXZ,
                 float3(camUsedPos.x, camUsedPos.y, camUsedPos.z),
                 cam.GetFrustum(), modelMatrix, settings.distanceThreshold);

        QuadTreeBuildTime +=
            std::chrono::duration_cast<decltype(QuadTreeBuildTime)>(
                std::chrono::high_resolution_clock::now() - start);

        qtNodes += qt.GetSize();

        // The best choice is to upload planeBottomLeft and planeTopRight and
        // kinda of UV coordinate that can go outside [0,1] and the fract is
        // the actual UV value.

        u16 instanceCount = 0;

        allocator.SetRenderTargets({renderTargetView},
                                   frameResource.DepthBuffer.DepthStencil());
        graphicsPipelineState.Apply(allocator);
        start = std::chrono::high_resolution_clock::now();
        for (auto it = qt.begin(); it != qt.end(); ++it) {
          NavigatingTheQuadTree +=
              std::chrono::duration_cast<decltype(NavigatingTheQuadTree)>(
                  (std::chrono::high_resolution_clock::now() - start));
          drawnNodes++;

          {
            vertexConstants.instanceData[instanceCount].scaling = {it->size.x,
                                                                   it->size.y};
            vertexConstants.instanceData[instanceCount].offset = {it->center.x,
                                                                  it->center.y};
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
                return x;
            };
            hullConstants.instanceData[instanceCount].TesselationFactor = {
                l(res.zneg), l(res.xneg), l(res.zpos), l(res.xpos)};
          }

          instanceCount++;
          if (instanceCount == Defaults::App::maxInstances) {
            auto mask = simpleRootSignature.Set(allocator,
                                                RootSignatureUsage::Graphics);

            if (usedTexture)
              mask.texture = *usedTexture;

            mask.heightMapForDomain = displacementMap;
            mask.gradientsForPixel = gradients;

            mask.hullBuffer =
                frameResource.DynamicBuffer.AddBuffer(hullConstants);
            mask.vertexBuffer =
                frameResource.DynamicBuffer.AddBuffer(vertexConstants);
            mask.domainBuffer = domainConstantBuffer;
            mask.pixelBuffer = pixelConstantBuffer;
            mask.cameraBuffer = cameraConstantBuffer;
            mask.modelBuffer = modelBuffer;

            planeMesh.Draw(allocator, instanceCount);
            instanceCount = 0;
          }

          start = std::chrono::high_resolution_clock::now();
        }
        if (instanceCount != 0) {
          auto mask =
              simpleRootSignature.Set(allocator, RootSignatureUsage::Graphics);

          if (usedTexture)
            mask.texture = *usedTexture;

          mask.heightMapForDomain = displacementMap;
          mask.gradientsForPixel = gradients;

          mask.hullBuffer =
              frameResource.DynamicBuffer.AddBuffer(hullConstants);
          mask.vertexBuffer =
              frameResource.DynamicBuffer.AddBuffer(vertexConstants);
          mask.domainBuffer = domainConstantBuffer;
          mask.pixelBuffer = pixelConstantBuffer;
          mask.cameraBuffer = cameraConstantBuffer;
          mask.modelBuffer = modelBuffer;

          planeMesh.Draw(allocator, instanceCount);
          instanceCount = 0;
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
          ImGui::InputFloat("Distance Threshold", &settings.distanceThreshold);
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
          static uint itemsIndex =
              rasterizerFlags == RasterizerFlags::CullNone
                  ? 1
                  : (rasterizerFlags == RasterizerFlags::CullClockwise ? 0 : 2);

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
    // Wait until everything is done

    for (auto &frameResource : frameResources) {
      if (frameResource.Marker) {
        frameResource.Fence.Await(frameResource.Marker);
      }
    }
    for (auto &drawingSimResource : simulationResources) {
      if (drawingSimResource.FrameDoneMarker) {
        drawingSimResource.Fence.Await(drawingSimResource.FrameDoneMarker);
      }
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