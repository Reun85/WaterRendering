#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <string.h>
#include "Defaults.h"
#include "Simulation.h"
#include "Helpers.h"
#include "pix3.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"
#include "SkyboxPipeline.hpp"
#include "ShadowVolume.h"

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
using namespace Axodox::Threading;
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

  struct TimeData {
    float deltaTime;
    float timeSinceLaunch;
  };
  struct RuntimeSettings {
    bool timeRunning = true;
    XMFLOAT4 clearColor = DefaultsValues::App::clearColor;
    bool quit = false;
    void DrawImGui([[maybe_unused]] NeedToDo &out,
                   bool exclusiveWindow = false) {
      bool cont = true;
      if (exclusiveWindow)
        cont = ImGui::Begin("Runtime Settings");
      if (cont) {
        ImGui::ColorEdit3("clear color", (float *)&clearColor);
        ImGui::Checkbox("Time running", &timeRunning);
      }
      if (exclusiveWindow)
        ImGui::End();
    }
  };

  struct DebugValues {
    enum class Mode : u8 {
      Full = 0,
      DisplacementHighest,
      DisplacementMedium,
      DisplacementLowest,
      GradientsHighest,
      GradientsMedium,
      GradientsLowest,
    };

    XMFLOAT4 pixelMult = XMFLOAT4(1, 1, 1, 1);
    XMUINT4 swizzleorder = XMUINT4(0, 1, 2, 3);
    XMFLOAT4 blendDistances = XMFLOAT4(150.f, 300, 1000, 5000);
    XMFLOAT3 foamColor = XMFLOAT3(1, 1, 1);

    std::array<bool, 31 - 0 + 1> DebugBits{false, false, true,
                                           true,  true,  true};

    bool enableSSR = false;
    bool lockQuadTree = false;
    const static constexpr std::initializer_list<
        std::pair<u8, std::optional<const char *>>>
        DebugBitsDesc = {{2, "Use Foam"},
                         {3, "Use channel Highest"},
                         {4, "Use channel Medium"},
                         {5, "Use channel Lowest"},
                         {8, "Show albedo"},
                         {9, "Show normal"},
                         {10, "Show neg normal"},
                         {11, "Show worldPos"},
                         {12, "Show materialValues"},
                         {13, "Show depth"},
                         {24, "Normal overflow"},
                         {25, "Display Foam"},

                         {20, "F"},
                         {21, "D"},
                         {22, "G"},

                         {26, std::nullopt},
                         {27, std::nullopt},
                         {28, std::nullopt},
                         {29, std::nullopt},
                         {30, std::nullopt},
                         {31, std::nullopt}};

    Mode mode = Mode::Full;

    std::array<bool, 3> getChannels() {
      // I am sorry, I am lazy
      return {DebugBits[3], DebugBits[4], DebugBits[5]};
    }
    RasterizerFlags rasterizerFlags = RasterizerFlags::CullClockwise;
    void DrawImGui(NeedToDo &out, bool exclusiveWindow = true) {
      bool cont = true;
      if (exclusiveWindow)
        cont = ImGui::Begin("Debug Values");
      if (cont) {
        // useTexture
        useTextureImGuiDraw();
        // Culling
        CullingImGuiDraw(out);

        ImGui::InputFloat4("Blend Distances", (float *)&blendDistances);
        ImGui::Checkbox("Enable SSR", &enableSSR);
        ImGui::Checkbox("Lock QuadTree", &lockQuadTree);
        for (auto &[id, name] : DebugBitsDesc) {
          if (name)
            ImGui::Checkbox(*name, &DebugBits[id]);
          else
            ImGui::Checkbox(std::format("Debug Bit {}", id).c_str(),
                            &DebugBits[id]);
        }
      }
      if (exclusiveWindow)
        ImGui::End();
    }

  private:
    void CullingImGuiDraw(NeedToDo &out) {
      {
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
                using enum Axodox::Graphics::D3D12::RasterizerFlags;
              case 1:
                rasterizerFlags = CullNone;
                break;
              case 2:
                rasterizerFlags = Wireframe;
                break;
              default:
                rasterizerFlags = CullClockwise;
                break;
              }
              out.changeFlag = rasterizerFlags;
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
      }
    }

  private:
    void useTextureImGuiDraw() {
      {
        static const std::array<std::string, 7> modeitems = {
            "Full",
            "DisplacementHighest",
            "DisplacementMedium",
            "DisplacementLowest",
            "GradientsHighest",
            "GradientsMedium",
            "GradientsLowest"};
        if (ImGui::BeginCombo("Mode", modeitems[(u32)(mode)].c_str())) {
          for (uint i = 0; i < modeitems.size(); i++) {
            bool isSelected = ((u32)(mode) == i);
            if (ImGui::Selectable(modeitems[i].c_str(), isSelected)) {
              mode = Mode(i);
              swizzleorder = XMUINT4(0, 1, 2, 3);
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }
        if (mode != Mode::Full) {
          ImGui::InputFloat4("Pixel Mult", (float *)&pixelMult);

          static const std::array<std::string, 4> swizzleitems = {"r", "g", "b",
                                                                  "a"};

          const std::array<u32 *const, 4> vals = {
              &swizzleorder.x, &swizzleorder.y, &swizzleorder.z,
              &swizzleorder.w};
          ImGui::Text("Swizzle");
          for (int i = 0; i < 4; i++) {
            const string id = "##Swizzle" + swizzleitems[i];
            ImGui::Text("%s", swizzleitems[i].c_str());
            ImGui::SameLine();

            if (ImGui::BeginCombo(id.c_str(), swizzleitems[*vals[i]].c_str())) {
              for (size_t j = 0; j < swizzleitems.size(); j++) {
                bool isSelected = (*(vals[i]) == j);
                if (ImGui::Selectable(swizzleitems[j].c_str(), isSelected)) {
                  *vals[i] = (u32)j;
                }
                if (isSelected) {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
          }
        }
      }
    }
  };

  struct DebugGPUBufferStuff {
    XMFLOAT4 pixelMult;
    XMFLOAT4 blendDistances;
    XMUINT4 swizzleOrder;
    XMFLOAT3 patchSizes;
    // 0: use displacement
    // 1: use normal
    // 2: use foam
    // 3: use channel1
    // 4: use channel2
    // 5: use channel3
    // 6: display texture instead of shader
    // 7: transform texture values from [-1,1] to [0,1]
    uint flags = 0; // Its here because padding
    XMFLOAT4 foamInfo;
    float EnvMapMult;
  };
  static void set_flag(uint &flag, uint flagIndex, bool flagValue = true) {
    if (flagValue) {
      flag |= 1 << flagIndex;
    } else {
      flag &= ~(1 << flagIndex);
    }
  }
  static DebugGPUBufferStuff From(const DebugValues &deb,
                                  const SimulationData &simData) {
    DebugGPUBufferStuff res;
    res.pixelMult = XMFLOAT4(deb.pixelMult.x, deb.pixelMult.y, deb.pixelMult.z,
                             deb.pixelMult.w);
    res.swizzleOrder = deb.swizzleorder;
    res.blendDistances = deb.blendDistances;
    res.patchSizes =
        XMFLOAT3(simData.Highest.patchSize, simData.Medium.patchSize,
                 simData.Lowest.patchSize);

    for (int i = 0; i < deb.DebugBits.size(); i++) {
      set_flag(res.flags, i, deb.DebugBits[i]);
    }

    set_flag(res.flags, 6, true);
    set_flag(res.flags, 0, false);

    switch (deb.mode) {
    case DebugValues::Mode::Full:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 6, false);
      break;
    case DebugValues::Mode::GradientsHighest:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 7, true);
      set_flag(res.flags, 6, false);
      set_flag(res.flags, 3, true);
      set_flag(res.flags, 4, false);
      set_flag(res.flags, 5, false);
      break;
    case DebugValues::Mode::GradientsMedium:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 7, true);
      set_flag(res.flags, 6, false);
      set_flag(res.flags, 3, false);
      set_flag(res.flags, 4, true);
      set_flag(res.flags, 5, false);
      break;
    case DebugValues::Mode::GradientsLowest:
      set_flag(res.flags, 0, true);
      set_flag(res.flags, 7, true);
      set_flag(res.flags, 6, false);
      set_flag(res.flags, 3, false);
      set_flag(res.flags, 4, false);
      set_flag(res.flags, 5, true);
      break;
    default:
      break;
    }
    return res;
  }

  static void SetUpWindowInput(const CoreWindow &window,
                               RuntimeSettings &settings, Camera &cam) {
    bool &timerunning = settings.timeRunning;
    bool &quit = settings.quit;
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

  static void DrawImGuiForPSResources(
      WaterGraphicRootDescription::WaterPixelShaderData &waterData,
      PixelLighting &sunData, DeferredShading::DeferredShaderBuffers &defData,
      bool exclusiveWindow = true) {
    bool cont = true;
    if (exclusiveWindow) {
      cont = ImGui::Begin("PS Data");
    }
    if (cont) {
      ImGui::ColorEdit3("Surface Color", &waterData.AlbedoColor.x);
      ImGui::SliderFloat("Roughness", &waterData.Roughness, 0.0f, 1.0f);

      ImGui::ColorEdit3("Tip Color", &defData._TipColor.x);
      ImGui::SliderFloat("Normal Depth Attenuation",
                         &waterData.NormalDepthAttenuation, 0, 2);
      ImGui::SliderFloat("Foam Roughness Modifier",
                         &waterData.foamRoughnessModifier, 0.0f, 10.0f);
      ImGui::SliderFloat("Foam Depth Falloff", &waterData.foamDepthFalloff,
                         0.0f, 10.0f);
      ImGui::SliderFloat("Height Modifier", &waterData._HeightModifier, 0.0f,
                         10.0f);
      ImGui::SliderFloat("Fresnel", &waterData._Fresnel, 0.0f, 1.0f);
      ImGui::SliderFloat("Wave Peak Scatter Strength",
                         &waterData._WavePeakScatterStrength, 0.0f, 10.0f);
      ImGui::SliderFloat("Scatter Shadow Strength",
                         &waterData._ScatterShadowStrength, 0.0f, 10.0f);

      ImGui::Separator();
      ImGui::Text("Sun Data");
      ImGui::SliderFloat3("Light Pos", (float *)&sunData.lights[0].lightPos, -1,
                          1);

      ImGui::ColorEdit3("Light Color", (float *)&sunData.lights[0].lightColor);
      ImGui::SliderFloat("Light Intensity", &sunData.lights[0].lightColor.w, 0,
                         10);

      ImGui::ColorEdit3("Ambient Color", &sunData.lights[0].AmbientColor.x);
      ImGui::SliderFloat("Ambient Mult", &sunData.lights[0].AmbientColor.w,
                         0.0f, 10.0f);
      ImGui::Separator();
      ImGui::Text("DeferredShaderBuffer Data");
      ImGui::SliderFloat("Env Map", (float *)&defData.EnvMapMult, 0, 2);
    }
    if (exclusiveWindow)
      ImGui::End();
  }
  static void DrawImGuiForPSResources(
      WaterGraphicRootDescription::PixelShaderPBRData &waterData,
      PixelLighting &sunData, bool exclusiveWindow = true) {
    bool cont = true;
    if (exclusiveWindow) {
      cont = ImGui::Begin("PS Data");
    }
    if (cont) {
      ImGui::ColorEdit3("Surface Color", (float *)&waterData.SurfaceColor);
      ImGui::SliderFloat("Roughness", &waterData.Roughness, 0, 1);
      ImGui::SliderFloat("Subsurface Scattering",
                         &waterData.SubsurfaceScattering, 0, 1);
      ImGui::SliderFloat("Sheen", &waterData.Sheen, 0, 1);
      ImGui::SliderFloat("Sheen Tint", &waterData.SheenTint, 0, 1);
      ImGui::SliderFloat("Anisotropic", &waterData.Anisotropic, 0, 1);
      ImGui::SliderFloat("Specular Strength", &waterData.SpecularStrength, 0,
                         1);
      ImGui::SliderFloat("Metallic", &waterData.Metallic, 0, 1);
      ImGui::SliderFloat("Specular Tint", &waterData.SpecularTint, 0, 1);
      ImGui::SliderFloat("Clearcoat", &waterData.Clearcoat, 0, 1);
      ImGui::SliderFloat("Clearcoat Gloss", &waterData.ClearcoatGloss, 0, 1);

      ImGui::Separator();
      ImGui::Text("Sun Data");
      ImGui::SliderFloat3("Light Pos", (float *)&sunData.lights[0].lightPos, -1,
                          1);

      ImGui::ColorEdit3("Light Color", (float *)&sunData.lights[0].lightColor);
      ImGui::SliderFloat("Light Intensity", &sunData.lights[0].lightColor.w, 0,
                         10);
    }
    if (exclusiveWindow)
      ImGui::End();
  }

  struct RuntimeCPUBuffers {
    std::vector<WaterGraphicRootDescription::OceanData> oceanData;
  };

  void Run() const {
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    Camera cam;
    cam.SetView(XMVectorSet(DefaultsValues::Cam::camStartPos.x,
                            DefaultsValues::Cam::camStartPos.y,
                            DefaultsValues::Cam::camStartPos.z, 0),
                XMVectorSet(0.0f, 0.0f, 0.0f, 0),
                XMVectorSet(0.0f, 1.0f, 0.0f, 0));

    cam.SetFirstPerson(DefaultsValues::Cam::startFirstPerson);
    // Events

    RuntimeSettings settings;
    DebugValues debugValues;

    SetUpWindowInput(window, settings, cam);

    CoreDispatcher dispatcher = window.Dispatcher();

    GraphicsDevice device{};
    CommandQueue directQueue{device};
    CommandQueue computeQueue{device, /* CommandKind::Compute*/};
    // CommandQueue &computeQueue = directQueue;
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsTearingAllowed};
    // CoreSwapChain swapChain{directQueue, window, SwapChainFlags::Default};

    PipelineStateProvider pipelineStateProvider{device};

    // Graphics pipeline
    RootSignature<WaterGraphicRootDescription> waterRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"VertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"PixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

    auto gBufferFormats = DeferredShading::GBuffer::GetGBufferFormats();

    GraphicsPipelineStateDefinition waterPipelineStateDefinition{
        .RootSignature = &waterRootSignature,
        .VertexShader = &simpleVertexShader,
        .DomainShader = &domainShader,
        .HullShader = &hullShader,
        .PixelShader = &simplePixelShader,
        .RasterizerState = debugValues.rasterizerFlags,
        .DepthStencilState = DepthStencilMode::WriteDepth,
        .InputLayout = VertexPosition::Layout,
        .TopologyType = PrimitiveTopologyType::Patch,
        .RenderTargetFormats =
            std::initializer_list(std::to_address(gBufferFormats.begin()),
                                  std::to_address(gBufferFormats.end())),
        .DepthStencilFormat = Format::D32_Float};

    Axodox::Graphics::D3D12::PipelineState waterPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(waterPipelineStateDefinition)
            .get();

    VertexShader atmosphereVS{app_folder() / L"AtmosphereVS.cso"};
    PixelShader atmospherePS{app_folder() / L"AtmospherePS.cso"};
    RootSignature<SkyboxRootDescription> skyboxRootSignature{device};
    DepthStencilState skyboxDepthStencilState{DepthStencilMode::WriteDepth};
    skyboxDepthStencilState.Comparison = ComparisonFunction::LessOrEqual;

    GraphicsPipelineStateDefinition skyboxPipelineStateDefinition{
        .RootSignature = &skyboxRootSignature,
        .VertexShader = &atmosphereVS,
        .PixelShader = &atmospherePS,
        .RasterizerState = RasterizerFlags::CullNone,
        .DepthStencilState = skyboxDepthStencilState,
        .InputLayout = VertexPositionNormalTexture::Layout,
        .RenderTargetFormats =
            std::initializer_list(std::to_address(gBufferFormats.begin()),
                                  std::to_address(gBufferFormats.end())),

        .DepthStencilFormat = Format::D32_Float};
    Axodox::Graphics::D3D12::PipelineState skyboxPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(skyboxPipelineStateDefinition)
            .get();

    VertexShader deferredShadingVS{app_folder() / L"DeferredShadingVS.cso"};
    PixelShader deferredShadingPS{app_folder() / L"DeferredShadingPS.cso"};
    RootSignature<DeferredShading> deferredShadingRootSignature{device};

    GraphicsPipelineStateDefinition deferredShadingPipelineStateDefinition{
        .RootSignature = &deferredShadingRootSignature,
        .VertexShader = &deferredShadingVS,
        .PixelShader = &deferredShadingPS,
        .BlendState = {BlendType::Additive, BlendType::AlphaBlend},
        .RasterizerState = RasterizerFlags::CullCounterClockwise,
        .InputLayout = VertexPositionNormalTexture::Layout,
        .TopologyType = PrimitiveTopologyType::Triangle,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
    };
    Axodox::Graphics::D3D12::PipelineState deferredShadingPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(deferredShadingPipelineStateDefinition)
            .get();

    RootSignature<SSRPostProcessing> postProcessingRootSignature{device};
    ComputeShader postProcessingComputeShader{app_folder() /
                                              L"SSRPostProcessingShader.cso"};
    ComputePipelineStateDefinition postProcessingStateDefinition{
        .RootSignature = &postProcessingRootSignature,
        .ComputeShader = &postProcessingComputeShader};
    auto postProcessingPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(postProcessingStateDefinition)
            .get();

    BasicShader basicShader =
        BasicShader::WithDefaultShaders(pipelineStateProvider, device);

    SilhouetteDetector silhouetteDetector =
        SilhouetteDetector::WithDefaultShaders(pipelineStateProvider, device);

    SilhouetteClearTask silhouetteClear =
        SilhouetteClearTask::WithDefaultShaders(pipelineStateProvider, device);

    SilhouetteDetectorTester silhouetteTester =
        SilhouetteDetectorTester::WithDefaultShaders(pipelineStateProvider,
                                                     device);

    WaterGraphicRootDescription::WaterPixelShaderData waterData;
    DeferredShading::DeferredShaderBuffers defData;
    PixelLighting sunData = PixelLighting::SunData();

    ShadowMapping::Data shadowMapData(cam);

    // Compute pipeline

    auto fullSimPipeline =
        SimulationStage::FullPipeline::Create(device, pipelineStateProvider);

    // Group together allocations
    GroupedResourceAllocator groupedResourceAllocator{device};
    ResourceUploader resourceUploader{device};
    CommonDescriptorHeap commonDescriptorHeap{device, 2};
    DepthStencilDescriptorHeap depthStencilDescriptorHeap{device};
    RenderTargetDescriptorHeap renderTargetDescriptorHeap{device};
    ResourceAllocationContext immutableAllocationContext{
        .Device = &device,
        .ResourceAllocator = &groupedResourceAllocator,
        .ResourceUploader = &resourceUploader,
        .CommonDescriptorHeap = &commonDescriptorHeap,
        .RenderTargetDescriptorHeap = &renderTargetDescriptorHeap,
        .DepthStencilDescriptorHeap = &depthStencilDescriptorHeap};

    SimulationData simData = SimulationData::Default();

    ImmutableMesh planeMesh{immutableAllocationContext, CreateQuadPatch()};

    ImmutableMesh deferredShadingPlane{immutableAllocationContext,
                                       CreateBackwardsPlane(2, XMUINT2(2, 2))};
    ImmutableMesh skyboxMesh{immutableAllocationContext, CreateCube(2)};
    ImmutableMesh Box{immutableAllocationContext, CreateCube(2)};

    const CubeMapPaths paths = {.PosX = app_folder() / "Assets/skybox/px.png",
                                .NegX = app_folder() / "Assets/skybox/nx.png",
                                .PosY = app_folder() / "Assets/skybox/py.png",
                                .NegY = app_folder() / "Assets/skybox/ny.png",
                                .PosZ = app_folder() / "Assets/skybox/pz.png",
                                .NegZ = app_folder() / "Assets/skybox/nz.png"};
    CubeMapTexture skyboxTexture{immutableAllocationContext, paths};
    // CubeMapTexture skyboxTexture{immutableAllocationContext,
    //                              app_folder() / "Assets/skybox/skybox3.hdr",
    //                              2024};

    //  Acquire memory
    SilhouetteDetector::MeshSpecificBuffers silhouetteDetectorMeshBuffers(
        immutableAllocationContext, Box);
    groupedResourceAllocator.Build();

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    SimulationStage::ConstantGpuSources simulationConstantSources(
        mutableAllocationContext, simData);
    SimulationStage::MutableGpuSources simulationMutableSources(
        mutableAllocationContext, simData);

    SilhouetteDetector::Buffers silhouetteDetectorBuffers(
        mutableAllocationContext, Box.GetIndexCount() * 4);

    array<FrameResources, 2> frameResources{
        FrameResources(mutableAllocationContext),
        FrameResources(mutableAllocationContext)};

    array<SimulationStage::SimulationResources, 2> simulationResources{
        SimulationStage::SimulationResources(mutableAllocationContext,
                                             simData.N, simData.M),
        SimulationStage::SimulationResources(mutableAllocationContext,
                                             simData.N, simData.M)};

    committedResourceAllocator.Build();
    const u32 &N = simData.N;

    swapChain.Resizing(
        no_revoke,
        [&cam, &frameResources, &commonDescriptorHeap](SwapChain const *self) {
          for (auto &frame : frameResources)
            frame.ScreenResourceView.reset();
          commonDescriptorHeap.Clean();
          auto resolution = self->Resolution();
          cam.SetAspect(float(resolution.x) / float(resolution.y));
        });

    std::string ImGuiIniPath = GetLocalFolder() + "/imgui.ini";

    ID3D12DescriptorHeap *ImGuiDescriptorHeap =
        InitImGui(device, (u8)frameResources.size(), ImGuiIniPath);
    ImGuiIO const &io = ImGui::GetIO();

    // Time counter
    using SinceTimeStartTimeFrame = std::chrono::nanoseconds;
    decltype(std::chrono::high_resolution_clock::now()) loopStartTime;
    auto getTimeSinceStart = [&loopStartTime]() {
      return std::chrono::duration_cast<SinceTimeStartTimeFrame>(
          std::chrono::high_resolution_clock::now() - loopStartTime);
    };
    float gameTime = 0;
    // Frame counter
    auto frameCounter = 0u;
    std::chrono::steady_clock::time_point frameStart =
        std::chrono::high_resolution_clock::now();

    NeedToDo beforeNextFrame;
    beforeNextFrame.patchHighestChanged = true;
    beforeNextFrame.patchMediumChanged = true;
    beforeNextFrame.patchLowestChanged = true;

    RuntimeCPUBuffers cpuBuffers;

    auto resolution = swapChain.Resolution();
    cam.SetAspect(float(resolution.x) / float(resolution.y));

    loopStartTime = std::chrono::high_resolution_clock::now();
    while (!settings.quit) {
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

      struct NewData {
        std::optional<std::future<PipelineState>> pipelineState;
        std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
            highestData;
        std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
            mediumData;
        std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
            lowestData;
      };

      NewData newData;
      {
        if (beforeNextFrame.changeFlag) {
          waterPipelineStateDefinition.RasterizerState.Flags =
              *beforeNextFrame.changeFlag;
          newData.pipelineState =
              pipelineStateProvider.CreatePipelineStateAsync(
                  waterPipelineStateDefinition);
        }
        if (beforeNextFrame.patchHighestChanged) {
          newData.highestData =
              SimulationStage::ConstantGpuSources<>::LODDataSource(
                  mutableAllocationContext, simData.Highest);
        }
        if (beforeNextFrame.patchMediumChanged) {
          newData.mediumData =
              SimulationStage::ConstantGpuSources<>::LODDataSource(
                  mutableAllocationContext, simData.Medium);
        }
        if (beforeNextFrame.patchLowestChanged) {
          newData.lowestData =
              SimulationStage::ConstantGpuSources<>::LODDataSource(
                  mutableAllocationContext, simData.Lowest);
        }
      }

      // Wait until buffers can be used
      if (frameResource.Marker)
        frameResource.Fence.Await(frameResource.Marker);
      if (drawingSimResource.FrameDoneMarker)
        drawingSimResource.Fence.Await(drawingSimResource.FrameDoneMarker);
      // This is necessary for the compute queue

      if (beforeNextFrame.changeFlag && newData.pipelineState) {
        waterPipelineState = newData.pipelineState->get();
        beforeNextFrame.changeFlag = std::nullopt;
      }

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
      TimeData timeConstants{.deltaTime = settings.timeRunning ? deltaTime : 0,
                             .timeSinceLaunch = gameTime};

      frameResource.MakeCompatible(*renderTargetView, mutableAllocationContext);

      bool camChanged = cam.Update(deltaTime);

      // Frame Begin
      {
        committedResourceAllocator.Build();
        depthStencilDescriptorHeap.Build();
        renderTargetDescriptorHeap.Build();
        commonDescriptorHeap.Build();
      }

      RuntimeResults runtimeResults;

      auto oceanModelMatrix =
          XMMatrixTranslationFromVector(XMVECTOR{0, -5, 0, 1});
      auto oceanDataFuture = threadpool_execute<
          std::vector<WaterGraphicRootDescription::OceanData>
              &>([&cpuBuffers, cam, simData, &runtimeResults, camChanged,
                  oceanModelMatrix, &debugValues]()
                     -> std::vector<WaterGraphicRootDescription::OceanData> & {
        if (camChanged && !debugValues.lockQuadTree) {
          cpuBuffers.oceanData.clear();
          return WaterGraphicRootDescription::CollectOceanQuadInfoWithQuadTree(
              cpuBuffers.oceanData, cam, oceanModelMatrix,
              simData.quadTreeDistanceThreshold, &runtimeResults);
        }
        return cpuBuffers.oceanData;
      });

      // Compute shader stage
      // It has to return some value or threadpool execute fails?????
      std::future computeStage = threadpool_execute<bool>([&]() {
        auto &simResource = calculatingSimResource;
        auto &computeAllocator = simResource.Allocator;
        computeAllocator.Reset();
        computeAllocator.BeginList();
        commonDescriptorHeap.Set(computeAllocator);

        // If a change has been issued change constant buffers
        if (newData.highestData || newData.mediumData || newData.lowestData) {
          auto copyRes = [&computeAllocator](const MutableTexture &src,
                                             const MutableTexture &dst) {
            computeAllocator.TransitionResources(
                {{src, ResourceStates::Common, ResourceStates::CopySource},
                 {dst, ResourceStates::NonPixelShaderResource,
                  ResourceStates::CopyDest}});
            computeAllocator.CopyResource(src, dst);
            computeAllocator.TransitionResources(
                {{dst, ResourceStates::CopyDest,
                  ResourceStates::NonPixelShaderResource}});
          };
          auto copyLOD =
              [&copyRes](
                  const SimulationStage::ConstantGpuSources<>::LODDataSource
                      &src,
                  const SimulationStage::ConstantGpuSources<>::LODDataSource
                      &dst) {
                copyRes(src.Tildeh0, dst.Tildeh0);
                copyRes(src.Frequencies, dst.Frequencies);
              };
          if (newData.highestData) {
            copyLOD(*newData.highestData, simulationConstantSources.Highest);
            beforeNextFrame.patchHighestChanged = false;
          }
          if (newData.mediumData) {
            copyLOD(*newData.mediumData, simulationConstantSources.Medium);
            beforeNextFrame.patchMediumChanged = false;
          }
          if (newData.lowestData) {
            copyLOD(*newData.lowestData, simulationConstantSources.Lowest);
            beforeNextFrame.patchLowestChanged = false;
          }
        }

        // Since we are using this on different queues, it is uploaded twice.
        GpuVirtualAddress timeDataBuffer =
            simResource.DynamicBuffer.AddBuffer(timeConstants);

        WaterSimulationComputeShader(
            simResource, simulationConstantSources, simulationMutableSources,
            simData, fullSimPipeline, computeAllocator, timeDataBuffer, N,
            debugValues.getChannels());

        // Upload queue
        {
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
        return true;
      });

      // Graphics Stage
      {
        auto &allocator = frameResource.Allocator;
        {
          allocator.Reset();
          allocator.BeginList();
          allocator.TransitionResource(*renderTargetView,
                                       ResourceStates::Present,
                                       ResourceStates::RenderTarget);
          commonDescriptorHeap.Set(allocator);

          renderTargetView->Clear(allocator, settings.clearColor);
          frameResource.Clear(allocator);
        }

        // Global data
        GpuVirtualAddress cameraConstantBuffer;
        GpuVirtualAddress debugConstantBuffer;
        GpuVirtualAddress lightsConstantBuffer;
        GpuVirtualAddress timeDataBuffer;

        {
          CameraConstants cameraConstants{};
          DebugGPUBufferStuff debugBufferContent = From(debugValues, simData);

          XMStoreFloat3(&cameraConstants.cameraPos, cam.GetEye());
          XMStoreFloat4x4(&cameraConstants.vMatrix,
                          XMMatrixTranspose(cam.GetViewMatrix()));
          XMStoreFloat4x4(&cameraConstants.pMatrix,
                          XMMatrixTranspose(cam.GetProj()));
          XMStoreFloat4x4(&cameraConstants.vpMatrix,
                          XMMatrixTranspose(cam.GetViewProj()));
          XMStoreFloat4x4(&cameraConstants.INVvMatrix,
                          XMMatrixTranspose(cam.GetINVView()));
          XMStoreFloat4x4(&cameraConstants.INVpMatrix,
                          XMMatrixTranspose(cam.GetINVProj()));
          XMStoreFloat4x4(&cameraConstants.INVvpMatrix,
                          XMMatrixTranspose(cam.GetINVViewProj()));
          cameraConstantBuffer =
              frameResource.DynamicBuffer.AddBuffer(cameraConstants);
          debugConstantBuffer =
              frameResource.DynamicBuffer.AddBuffer(debugBufferContent);
          lightsConstantBuffer = frameResource.DynamicBuffer.AddBuffer(sunData);
          timeDataBuffer = frameResource.DynamicBuffer.AddBuffer(timeConstants);
        }

        // Draw Ocean
        {
          // Will be accessed in multiple sections
          const auto &modelMatrix = oceanModelMatrix;

          // Need to reset after drawing
          std::optional<GpuVirtualAddress> usedTextureAddress;
          std::optional<MutableTextureWithState *> usedTexture;

          // Start Draw pass
          // Debug Data
          {
            switch (debugValues.mode) {
            case DebugValues::Mode::DisplacementHighest:
              usedTexture = &drawingSimResource.HighestBuffer.displacementMap;
              break;
            case DebugValues::Mode::GradientsHighest:
              usedTexture = &drawingSimResource.HighestBuffer.gradients;
              break;
            case DebugValues::Mode::DisplacementMedium:
              usedTexture = &drawingSimResource.MediumBuffer.displacementMap;
              break;
            case DebugValues::Mode::GradientsMedium:
              usedTexture = &drawingSimResource.MediumBuffer.gradients;
              break;
            case DebugValues::Mode::DisplacementLowest:
              usedTexture = &drawingSimResource.LowestBuffer.displacementMap;
              break;
            case DebugValues::Mode::GradientsLowest:
              usedTexture = &drawingSimResource.LowestBuffer.gradients;
              break;
            default:
              break;
            }

            if (usedTexture) {
              usedTextureAddress = *(*usedTexture)->ShaderResource(allocator);
            }
          }

          // Ocean Buffers
          WaterGraphicRootDescription::ModelConstants modelConstants{};

          XMStoreFloat4x4(&modelConstants.mMatrix,
                          XMMatrixTranspose(modelMatrix));

          // Upload absolute contants

          GpuVirtualAddress modelBuffer =
              frameResource.DynamicBuffer.AddBuffer(modelConstants);

          GpuVirtualAddress waterDataBuffer =
              frameResource.DynamicBuffer.AddBuffer(waterData);

          // Pre translate resources
          GpuVirtualAddress displacementMapAddressHighest =
              *drawingSimResource.HighestBuffer.displacementMap.ShaderResource(
                  allocator);
          GpuVirtualAddress gradientsAddressHighest =
              *drawingSimResource.HighestBuffer.gradients.ShaderResource(
                  allocator);
          GpuVirtualAddress displacementMapAddressMedium =
              *drawingSimResource.MediumBuffer.displacementMap.ShaderResource(
                  allocator);
          GpuVirtualAddress gradientsAddressMedium =
              *drawingSimResource.MediumBuffer.gradients.ShaderResource(
                  allocator);
          GpuVirtualAddress displacementMapAddressLowest =
              *drawingSimResource.LowestBuffer.displacementMap.ShaderResource(
                  allocator);
          GpuVirtualAddress gradientsAddressLowest =
              *drawingSimResource.LowestBuffer.gradients.ShaderResource(
                  allocator);

          // Shadow Map pass
          {
            // Get shadow casting object silhouette
            {
              {
                silhouetteClear.Pre(allocator);
                SilhouetteClearTask::Inp inp{
                    .buffers = silhouetteDetectorBuffers,
                };
                silhouetteClear.Run(allocator, frameResource.DynamicBuffer,
                                    inp);
              }
              {
                silhouetteDetector.Pre(allocator);
                SilhouetteDetector::Inp inp{
                    .buffers = silhouetteDetectorBuffers,
                    .lights = lightsConstantBuffer,
                    .mesh = Box,
                    .meshBuffers = silhouetteDetectorMeshBuffers,
                };
                silhouetteDetector.Run(allocator, frameResource.DynamicBuffer,
                                       inp);
              }
            }
            // ...
          }

          // GBuffer Pass
          {
            auto gBufferViews = frameResource.GBuffer.GetGBufferViews();
            allocator.SetRenderTargets(
                std::initializer_list(std::to_address(gBufferViews.begin()),
                                      std::to_address(gBufferViews.end())),
                frameResource.DepthBuffer.DepthStencil());

            // Box
            // outline
            {
              allocator.TransitionResource(
                  silhouetteDetectorBuffers.EdgeCountBuffer.get()->get(),
                  ResourceStates::UnorderedAccess,
                  ResourceStates::IndirectArgument);

              silhouetteTester.Pre(allocator);
              XMMATRIX boxModel = XMMatrixTranspose(
                  XMMatrixTranslationFromVector(XMVECTOR{2, 5, 2, 0}));
              SilhouetteDetectorTester::ModelConstants boxModelConstants{};
              XMStoreFloat4x4(&boxModelConstants.mMatrix, boxModel);
              SilhouetteDetectorTester::Inp inp{
                  .camera = cameraConstantBuffer,
                  .modelTransform =
                      frameResource.DynamicBuffer.AddBuffer(boxModelConstants),
                  .texture = std::nullopt,
                  .mesh = Box,
                  .buffers = silhouetteDetectorBuffers,
                  .meshBuffers = silhouetteDetectorMeshBuffers,
              };
              silhouetteTester.Run(allocator, frameResource.DynamicBuffer, inp);
              allocator.TransitionResource(
                  silhouetteDetectorBuffers.EdgeCountBuffer.get()->get(),
                  ResourceStates::IndirectArgument,
                  ResourceStates::UnorderedAccess);
            }
            // Box
            /*{
              basicShader.Pre(allocator);

              XMMATRIX boxModel = XMMatrixTranspose(
                  XMMatrixTranslationFromVector(XMVECTOR{2, 5, 2, 0}));
              BasicShader::ShaderMask::ModelConstants boxModelConstants{};
              XMStoreFloat4x4(&boxModelConstants.mMatrix, boxModel);
              BasicShader::Inp inp{
                  .camera = cameraConstantBuffer,
                  .modelTransform =
                      frameResource.DynamicBuffer.AddBuffer(boxModelConstants),
                  .texture = std::nullopt,
                  .mesh = Box,
              };
              basicShader.Run(allocator, frameResource.DynamicBuffer, inp);
            }*/

            // Water
            {
              waterPipelineState.Apply(allocator);

              const auto &oceanQuadData = oceanDataFuture.get();
              for (auto &curr : oceanQuadData) {
                if (curr.N == 0)
                  continue;
                auto mask = waterRootSignature.Set(
                    allocator, RootSignatureUsage::Graphics);

                if (usedTextureAddress)
                  mask.texture = *usedTextureAddress;

                mask.heightMapHighest = displacementMapAddressHighest;
                mask.gradientsHighest = gradientsAddressHighest;
                mask.heightMapMedium = displacementMapAddressMedium;
                mask.gradientsMedium = gradientsAddressMedium;
                mask.heightMapLowest = displacementMapAddressLowest;
                mask.gradientsLowest = gradientsAddressLowest;

                mask.waterPBRBuffer = waterDataBuffer;

                mask.hullBuffer =
                    frameResource.DynamicBuffer.AddBuffer(curr.hullConstants);
                mask.vertexBuffer =
                    frameResource.DynamicBuffer.AddBuffer(curr.vertexConstants);
                mask.debugBuffer = debugConstantBuffer;
                mask.cameraBuffer = cameraConstantBuffer;
                mask.modelBuffer = modelBuffer;

                planeMesh.Draw(allocator, curr.N);
              }
            }
            // skybox
            {
              skyboxPipelineState.Apply(allocator);

              auto mask = skyboxRootSignature.Set(allocator,
                                                  RootSignatureUsage::Graphics);

              mask.skybox = skyboxTexture;
              mask.lightingBuffer = lightsConstantBuffer;

              mask.cameraBuffer = cameraConstantBuffer;

              skyboxMesh.Draw(allocator);
            }
          }

          // Deferred Shading Pass
          {
            allocator.SetRenderTargets({renderTargetView}, nullptr);
            deferredShadingPipelineState.Apply(allocator);
            frameResource.GBuffer.TranslateToView(allocator);
            allocator.TransitionResource(
                frameResource.DepthBuffer.
                operator Axodox::Graphics::D3D12::ResourceArgument(),
                ResourceStates::DepthWrite,
                ResourceStates::PixelShaderResource);
            auto mask = deferredShadingRootSignature.Set(
                allocator, RootSignatureUsage::Graphics);

            mask.BindGBuffer(frameResource.GBuffer);

            // Textures
            mask.skybox = skyboxTexture;
            mask.gradientsHighest =
                *drawingSimResource.HighestBuffer.gradients.ShaderResource(
                    allocator);
            mask.gradientsMedium =
                *drawingSimResource.MediumBuffer.gradients.ShaderResource(
                    allocator);
            mask.gradientsLowest =
                *drawingSimResource.LowestBuffer.gradients.ShaderResource(
                    allocator);

            // Buffers
            mask.lightingBuffer = lightsConstantBuffer;
            mask.cameraBuffer = cameraConstantBuffer;
            mask.debugBuffer = debugConstantBuffer;

            mask.deferredShaderBuffer =
                frameResource.DynamicBuffer.AddBuffer(defData);

            mask.geometryDepth = *frameResource.DepthBuffer.ShaderResource();

            deferredShadingPlane.Draw(allocator);
          }

          // SSR post process
          if (debugValues.enableSSR) {
            allocator.TransitionResource(
                *renderTargetView, ResourceStates::RenderTarget,
                ResourceStates::NonPixelShaderResource);

            auto mask = postProcessingRootSignature.Set(
                allocator, RootSignatureUsage::Compute);
            mask.InpColor = *frameResource.ScreenResourceView;
            mask.DepthBuffer = *frameResource.DepthBuffer.ShaderResource();
            mask.NormalBuffer = *frameResource.GBuffer.Normal.ShaderResource();
            mask.OutputTexture =
                *frameResource.PostProcessingBuffer.UnorderedAccess();
            mask.CameraBuffer = cameraConstantBuffer;

            postProcessingPipelineState.Apply(allocator);

            auto definition = frameResource.PostProcessingBuffer.Definition();
            allocator.Dispatch(definition->Width / 16 + 1,
                               definition->Height / 16 + 1);

            allocator.TransitionResources(
                {{frameResource.PostProcessingBuffer,
                  ResourceStates::UnorderedAccess, ResourceStates::CopySource},
                 {*renderTargetView, ResourceStates::NonPixelShaderResource,
                  ResourceStates::CopyDest}});

            allocator.CopyResource(frameResource.PostProcessingBuffer,
                                   *renderTargetView);

            allocator.TransitionResources(
                {{frameResource.PostProcessingBuffer,
                  ResourceStates::CopySource, ResourceStates::UnorderedAccess},
                 {*renderTargetView, ResourceStates::CopyDest,
                  ResourceStates::RenderTarget}});
          }
          allocator.TransitionResource(frameResource.DepthBuffer,
                                       ResourceStates::PixelShaderResource,
                                       ResourceStates::DepthWrite);
          frameResource.GBuffer.TranslateToTarget(allocator);

          // Retransition simulation resources for compute shaders

          drawingSimResource.HighestBuffer.gradients.UnorderedAccess(allocator);
          drawingSimResource.HighestBuffer.displacementMap.UnorderedAccess(
              allocator);
          drawingSimResource.MediumBuffer.gradients.UnorderedAccess(allocator);
          drawingSimResource.MediumBuffer.displacementMap.UnorderedAccess(
              allocator);
          drawingSimResource.LowestBuffer.gradients.UnorderedAccess(allocator);
          drawingSimResource.LowestBuffer.displacementMap.UnorderedAccess(
              allocator);
          if (usedTexture)
            (*usedTexture)->UnorderedAccess(allocator);
        }

        auto CPURenderEnd = std::chrono::high_resolution_clock::now();
        runtimeResults.CPUTime = CPURenderEnd - frameStart;
        // ImGUI
        {
          ImGui_ImplDX12_NewFrame();
          ImGui_ImplUwp_NewFrame();
          ImGui::NewFrame();

          if (ImGui::Begin("Application")) {
            ImGui::Text("Press ESC to quit");
            ImGui::Text("Press Space to stop time");
            ImGui::Text("frame %d", frameCounter);
            ImGui::Text(
                " %.3f s",
                GetDurationInFloatWithPrecision<std::chrono::seconds,
                                                std::chrono::milliseconds>(
                    getTimeSinceStart()));
            ImGui::Text(" %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate,
                        io.Framerate);
            settings.DrawImGui(beforeNextFrame);

            runtimeResults.DrawImGui(false);
            cam.DrawImGui(false);
          }
          ImGui::End();
          debugValues.DrawImGui(beforeNextFrame);
          simData.DrawImGui(beforeNextFrame);
          DrawImGuiForPSResources(waterData, sunData, defData, true);

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
      }

      // Present frame
      computeStage.wait();
      swapChain.Present();
    }
    // Wait until everything is done before deleting context

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