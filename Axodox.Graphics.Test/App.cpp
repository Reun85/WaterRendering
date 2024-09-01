#include "pch.h"
#include "Camera.h"
#include "QuadTree.h"
#include <fstream>
#include <string.h>
#include "Defaults.h"
#include "Simulation.h"
#include "Helpers.h"
#include "pix3.h"
#include "ComputePipeline.h"
#include "GraphicsPipeline.h"
#include "SkyboxPipeline.hpp"

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

  struct TimeData {
    float deltaTime;
    float timeSinceLaunch;
  };
  struct RuntimeSettings {
    bool timeRunning = true;
    XMFLOAT4 clearColor = Defaults::App::clearColor;
    bool quit = false;
    void DrawImGui(NeedToDo &out, bool exclusiveWindow = false) {
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
    XMFLOAT4 blendDistances = XMFLOAT4(40.f, 150, 1000, 5000);
    XMFLOAT3 foamColor = XMFLOAT3(1, 1, 1);
    f32 foamDepthAttenuation = 2.f;

    bool useFoam = true;
    bool useChannel1 = true;
    bool useChannel2 = true;
    bool useChannel3 = false;
    bool onlyShowNormals = false;
    const static u32 DebugBitsStart = 24;
    const static u32 DebugBitsEnd = 31;
    std::array<bool, DebugBitsEnd - DebugBitsStart + 1> DebugBits{false};
    const static constexpr std::array<std::optional<const char *>,
                                      DebugBitsEnd - DebugBitsStart + 1>
        DebugBitsNames{"Normal overflow", "Display Foam", std::nullopt};

    Mode mode = Mode::Full;

    RasterizerFlags rasterizerFlags = RasterizerFlags::CullNone;
    void DrawImGui(NeedToDo &out, bool exclusiveWindow = true) {
      bool cont = true;
      if (exclusiveWindow)
        cont = ImGui::Begin("Debug Values");
      if (cont) {
        // useTexture
        useTextureImGuiDraw();
        // Culling
        CullingImGuiDraw(out);

        ImGui::Checkbox("Use foam", &useFoam);
        ImGui::Checkbox("Use channel Highest", &useChannel1);
        ImGui::Checkbox("Use channel Medium", &useChannel2);
        ImGui::Checkbox("Use channel Lowest", &useChannel3);
        ImGui::InputFloat4("Blend Distances", (float *)&blendDistances);
        ImGui::InputFloat3("Foam Color", (float *)&foamColor);
        ImGui::InputFloat("Foam Depth Attenuation", &foamDepthAttenuation);
        for (u32 i = DebugBitsStart; i <= DebugBitsEnd; i++) {
          if (DebugBitsNames[i - DebugBitsStart])
            ImGui::Checkbox(*DebugBitsNames[i - DebugBitsStart],
                            &DebugBits[i - DebugBitsStart]);
          else
            ImGui::Checkbox(std::format("Debug Bit {}", i).c_str(),
                            &DebugBits[i - DebugBitsStart]);
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

    res.foamInfo = XMFLOAT4(deb.foamColor.x, deb.foamColor.y, deb.foamColor.z,
                            deb.foamDepthAttenuation);
    set_flag(res.flags, 6, true);
    set_flag(res.flags, 0, false);
    set_flag(res.flags, 2, deb.useFoam);
    set_flag(res.flags, 3, deb.useChannel1);
    set_flag(res.flags, 4, deb.useChannel2);
    set_flag(res.flags, 5, deb.useChannel3);

    for (int i = 0; i < deb.DebugBits.size(); i++) {
      set_flag(res.flags, i + DebugValues::DebugBitsStart, deb.DebugBits[i]);
    }

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
    }
    return res;
  }

  struct RuntimeResults {
    uint qtNodes = 0;
    uint drawnNodes = 0;
    std::chrono::nanoseconds QuadTreeBuildTime{0};

    std::chrono::nanoseconds NavigatingTheQuadTree{0};
    std::chrono::nanoseconds CPUTime{0};
    void DrawImGui(bool exclusiveWindow = false) {
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
        ImGui::Text("CPU time %.3f ms/frame",
                    GetDurationInFloatWithPrecision<std::chrono::milliseconds,
                                                    std::chrono::nanoseconds>(
                        (CPUTime)));
      }
      if (exclusiveWindow)
        ImGui::End();
    }
  };

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
      WaterGraphicRootDescription::NewPixelShaderData &waterData,
      WaterGraphicRootDescription::PixelLighting &sunData,
      bool exclusiveWindow = true) {
    bool cont = true;
    if (exclusiveWindow) {
      cont = ImGui::Begin("PS Data");
    }
    if (cont) {
      ImGui::ColorEdit3("Surface Color", &waterData.SurfaceColor.x);
      ImGui::SliderFloat("Roughness", &waterData.Roughness, 0.0f, 1.0f);

      ImGui::ColorEdit3("Tip Color", &waterData._TipColor.x);
      ImGui::ColorEdit3("Scatter Color", &waterData._ScatterColor.x);
      ImGui::ColorEdit3("Ambient Color", &waterData._AmbientColor.x);
      ImGui::SliderFloat("Ambient Mult", &waterData._AmbientMult, 0.0f, 10.0f);
      ImGui::SliderFloat("Normal Depth Attenuation",
                         &waterData.NormalDepthAttenuation, 0, 2);
      ImGui::SliderFloat("Foam Roughness Modifier",
                         &waterData.foamRoughnessModifier, 0.0f, 10.0f);
      ImGui::SliderFloat("Foam Depth Falloff", &waterData.foamDepthFalloff,
                         0.0f, 10.0f);
      ImGui::SliderFloat("Height Modifier", &waterData._HeightModifier, 0.0f,
                         10.0f);
      ImGui::SliderFloat("Wave Peak Scatter Strength",
                         &waterData._WavePeakScatterStrength, 0.0f, 10.0f);
      ImGui::SliderFloat("Scatter Strength", &waterData._ScatterStrength, 0.0f,
                         10.0f);
      ImGui::SliderFloat("Scatter Shadow Strength",
                         &waterData._ScatterShadowStrength, 0.0f, 10.0f);
      ImGui::SliderFloat("Env Map Mult", &waterData._EnvMapMult, 0.0f, 10.0f);

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
  static void DrawImGuiForPSResources(
      WaterGraphicRootDescription::PixelShaderPBRData &waterData,
      WaterGraphicRootDescription::PixelLighting &sunData,
      bool exclusiveWindow = true) {
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
    struct OceanData {
      WaterGraphicRootDescription::VertexConstants vertexConstants;
      WaterGraphicRootDescription::HullConstants hullConstants;
      u16 N = 0;
    };
    std::vector<OceanData> oceanData;
  };

  void Run() const {
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    Camera cam;
    cam.SetView(
        XMVectorSet(Defaults::Cam::camStartPos.x, Defaults::Cam::camStartPos.y,
                    Defaults::Cam::camStartPos.z, 0),
        XMVectorSet(0.0f, 0.0f, 0.0f, 0), XMVectorSet(0.0f, 1.0f, 0.0f, 0));

    cam.SetFirstPerson(Defaults::Cam::startFirstPerson);
    // Events

    RuntimeSettings settings;
    DebugValues debugValues;

    SetUpWindowInput(window, settings, cam);

    CoreDispatcher dispatcher = window.Dispatcher();

    GraphicsDevice device{};
    CommandQueue directQueue{device};
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsTearingAllowed};

    PipelineStateProvider pipelineStateProvider{device};

    // Graphics pipeline
    RootSignature<WaterGraphicRootDescription> waterRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"VertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"PixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

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
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm,
                                Format::R16G16B16A16_Float,
                                Format::R16G16B16A16_Float,
                                Format::B8G8R8A8_UNorm},
        .DepthStencilFormat =
            Format::R32_Typeless // Typeless so we can transition it to shader
                                 // resource.
    };

    Axodox::Graphics::D3D12::PipelineState waterPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(waterPipelineStateDefinition)
            .get();

    VertexShader atmosphereVS{app_folder() / L"AtmosphereVS.cso"};
    PixelShader atmospherePS{app_folder() / L"AtmospherePS.cso"};
    RootSignature<SkyboxRootDescription> skyboxRootSignature{device};
    DepthStencilState skyboxDepthStencilState{DepthStencilMode::ReadDepth};
    skyboxDepthStencilState.Comparison = ComparisonFunction::LessOrEqual;
    GraphicsPipelineStateDefinition skyboxPipelineStateDefinition{
        .RootSignature = &skyboxRootSignature,
        .VertexShader = &atmosphereVS,
        .PixelShader = &atmospherePS,
        .RasterizerState = RasterizerFlags::CullNone,
        .DepthStencilState = skyboxDepthStencilState,
        .InputLayout = VertexPositionNormalTexture::Layout,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm,
                                Format::R16G16B16A16_Float,
                                Format::R16G16B16A16_Float,
                                Format::B8G8R8A8_UNorm},
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
        .DepthStencilState = DepthStencilMode::IgnoreDepth,
        .InputLayout = VertexPositionNormalTexture::Layout,
        .TopologyType = PrimitiveTopologyType::Triangle,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
        .DepthStencilFormat = {}};
    Axodox::Graphics::D3D12::PipelineState deferredShadingPipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(deferredShadingPipelineStateDefinition)
            .get();

    WaterGraphicRootDescription::NewPixelShaderData waterData;
    WaterGraphicRootDescription::PixelLighting sunData =
        WaterGraphicRootDescription::PixelLighting::SunData();

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

    const CubeMapPaths paths = {
        .sides = {.PosX = app_folder() / "Assets/skybox/px.png",
                  .NegX = app_folder() / "Assets/skybox/nx.png",
                  .PosY = app_folder() / "Assets/skybox/py.png",
                  .NegY = app_folder() / "Assets/skybox/ny.png",
                  .PosZ = app_folder() / "Assets/skybox/pz.png",
                  .NegZ = app_folder() / "Assets/skybox/nz.png"}};
    CubeMapTexture skyboxTexture{immutableAllocationContext,
                                 paths}; /*CubeMapTexture
   skyboxTexture{immutableAllocationContext, app_folder() /
   "Assets/skybox/skybox3.hdr", 2024};*/

    //  Acquire memory
    groupedResourceAllocator.Build();

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    SimulationStage::ConstantGpuSources simulationConstantSources(
        mutableAllocationContext, simData);
    SimulationStage::MutableGpuSources simulationMutableSources(
        mutableAllocationContext, simData);

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
        no_revoke, [&frameResources, &commonDescriptorHeap](SwapChain const *) {
          for (auto &frame : frameResources)
            frame.ScreenResourceView.reset();
          commonDescriptorHeap.Clean();
        });

    std::string ImGuiIniPath;
    {
      auto localFolder = winrt::Windows::Storage::ApplicationData::Current()
                             .LocalFolder()
                             .Path();
      std::wstring localFolderWstr = localFolder.c_str();
      ImGuiIniPath =
          std::string(localFolderWstr.begin(), localFolderWstr.end()) +
          "\\imgui.ini";
    }

    ID3D12DescriptorHeap *ImGuiDescriptorHeap =
        InitImGui(device, frameResources.size(), ImGuiIniPath);
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
    beforeNextFrame.PatchHighestChanged = true;
    CommandFence beforeNextFence(device);
    std::vector<CommandFenceMarker> AdditionalMarkers;

    RuntimeCPUBuffers cpuBuffers;

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

      std::optional<std::future<PipelineState>> newPipelineState;
      std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
          newHighestData;
      std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
          newMediumData;
      std::optional<SimulationStage::ConstantGpuSources<>::LODDataSource>
          newLowestData;
      {
        if (beforeNextFrame.changeFlag) {
          waterPipelineStateDefinition.RasterizerState.Flags =
              *beforeNextFrame.changeFlag;
          newPipelineState = pipelineStateProvider.CreatePipelineStateAsync(
              waterPipelineStateDefinition);
        }
        if (beforeNextFrame.PatchHighestChanged) {
          newHighestData = SimulationStage::ConstantGpuSources<>::LODDataSource(
              mutableAllocationContext, simData.Highest);
        }
        if (beforeNextFrame.PatchMediumChanged) {
          newMediumData = SimulationStage::ConstantGpuSources<>::LODDataSource(
              mutableAllocationContext, simData.Medium);
        }
        if (beforeNextFrame.PatchLowestChanged) {
          newLowestData = SimulationStage::ConstantGpuSources<>::LODDataSource(
              mutableAllocationContext, simData.Lowest);
        }
      }

      // Wait until buffers can be used
      if (frameResource.Marker)
        frameResource.Fence.Await(frameResource.Marker);
      if (drawingSimResource.FrameDoneMarker)
        drawingSimResource.Fence.Await(drawingSimResource.FrameDoneMarker);
      // This is necessary for the compute queue

      if (beforeNextFrame.changeFlag || newPipelineState) {
        waterPipelineState = newPipelineState->get();
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

      // Ensure depth buffer matches frame size
      if (!frameResource.DepthBuffer ||
          !TextureDefinition::AreSizeCompatible(
              *frameResource.DepthBuffer.Definition(),
              renderTargetView->Definition())) {

        auto depthDefinition =
            renderTargetView->Definition().MakeSizeCompatible(
                Format::R32_Typeless,
                (TextureFlags)(D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL));
        TextureViewDefinitions depthViews{
            .ShaderResource = D3D12_SHADER_RESOURCE_VIEW_DESC{},
            .RenderTarget = std::nullopt,
            .DepthStencil = D3D12_DEPTH_STENCIL_VIEW_DESC{},
            .UnorderedAccess = std::nullopt};
        depthViews.DepthStencil->Format = DXGI_FORMAT_D32_FLOAT;
        depthViews.DepthStencil->ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthViews.DepthStencil->Flags = D3D12_DSV_FLAG_NONE;

        depthViews.ShaderResource->Format = DXGI_FORMAT_R32_FLOAT;
        depthViews.ShaderResource->ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;
        depthViews.ShaderResource->Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        depthViews.ShaderResource->Texture2D.MipLevels = 1;

        frameResource.DepthBuffer.Allocate(depthDefinition, depthViews);
      }

      // Ensure screen shader resource view
      if (!frameResource.ScreenResourceView ||
          frameResource.ScreenResourceView->Resource() !=
              renderTargetView->Resource()) {
        Texture screenTexture{renderTargetView->Resource()};
        frameResource.ScreenResourceView =
            commonDescriptorHeap.CreateShaderResourceView(&screenTexture);
      }

      // Ensure GBuffer compatibility
      frameResource.DeferredShadingBuffers.EnsureCompatibility(
          *renderTargetView, commonDescriptorHeap);

      auto resolution = swapChain.Resolution();
      cam.SetAspect(float(resolution.x) / float(resolution.y));
      cam.Update(deltaTime);

      // Frame Begin
      {
        committedResourceAllocator.Build();
        depthStencilDescriptorHeap.Build();
        renderTargetDescriptorHeap.Build();
        commonDescriptorHeap.Build();
      }

      // Compute shader stage
      {
        auto &simResource = calculatingSimResource;
        auto &computeAllocator = simResource.Allocator;
        computeAllocator.Reset();
        computeAllocator.BeginList();
        commonDescriptorHeap.Set(computeAllocator);

        // If a change has been issued change constant buffers
        if (newHighestData || newMediumData || newLowestData) {
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
          if (newHighestData) {
            copyLOD(*newHighestData, simulationConstantSources.Highest);
            beforeNextFrame.PatchHighestChanged = false;
          }
          if (newMediumData) {
            copyLOD(*newMediumData, simulationConstantSources.Medium);
            beforeNextFrame.PatchMediumChanged = false;
          }
          if (newLowestData) {
            copyLOD(*newLowestData, simulationConstantSources.Lowest);
            beforeNextFrame.PatchLowestChanged = false;
          }
        }

        GpuVirtualAddress timeDataBuffer =
            simResource.DynamicBuffer.AddBuffer(timeConstants);

        WaterSimulationComputeShader(
            simResource, simulationConstantSources, simulationMutableSources,
            simData, fullSimPipeline, computeAllocator, timeDataBuffer, N);

        // Upload queue
        {
          auto commandList = computeAllocator.EndList();
          computeAllocator.BeginList();
          simResource.DynamicBuffer.UploadResources(computeAllocator);
          resourceUploader.UploadResourcesAsync(computeAllocator);
          auto initCommandList = computeAllocator.EndList();

          directQueue.Execute(initCommandList);
          directQueue.Execute(commandList);

          simResource.FrameDoneMarker =
              simResource.Fence.EnqueueSignal(directQueue);
        }
      }

      // Graphics Stage
      {

        RuntimeResults runtimeResults;
        auto &allocator = frameResource.Allocator;
        {
          allocator.Reset();
          allocator.BeginList();
          allocator.TransitionResource(*renderTargetView,
                                       ResourceStates::Present,
                                       ResourceStates::RenderTarget);

          commonDescriptorHeap.Set(allocator);
          // Clears frame with background color
          renderTargetView->Clear(allocator, settings.clearColor);
          frameResource.DepthBuffer.DepthStencil()->Clear(allocator);
        }

        // Global data
        GpuVirtualAddress cameraConstantBuffer;
        GpuVirtualAddress debugConstantBuffer;
        GpuVirtualAddress sunDataBuffer;
        {
          WaterGraphicRootDescription::cameraConstants cameraConstants{};
          DebugGPUBufferStuff debugBufferContent = From(debugValues, simData);

          XMStoreFloat3(&cameraConstants.cameraPos, cam.GetEye());
          XMStoreFloat4x4(&cameraConstants.vMatrix,
                          XMMatrixTranspose(cam.GetViewMatrix()));
          XMStoreFloat4x4(&cameraConstants.pMatrix,
                          XMMatrixTranspose(cam.GetProj()));
          XMStoreFloat4x4(&cameraConstants.vpMatrix,
                          XMMatrixTranspose(cam.GetViewProj()));
          cameraConstantBuffer =
              frameResource.DynamicBuffer.AddBuffer(cameraConstants);
          debugConstantBuffer =
              frameResource.DynamicBuffer.AddBuffer(debugBufferContent);
          sunDataBuffer = frameResource.DynamicBuffer.AddBuffer(sunData);
        }

        // Draw Ocean
        {

          // Will be accessed in multiple parts
          auto modelMatrix = XMMatrixIdentity();

          // Need to reset after drawing
          std::optional<GpuVirtualAddress> usedTextureAddress;
          std::optional<MutableTextureWithState *> usedTexture;
          // Collect Quad Info
          {

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
                     cam.GetFrustum(), modelMatrix,
                     simData.quadTreeDistanceThreshold);

            runtimeResults.QuadTreeBuildTime += std::chrono::duration_cast<
                decltype(runtimeResults.QuadTreeBuildTime)>(
                std::chrono::high_resolution_clock::now() - start);

            runtimeResults.qtNodes += qt.GetSize();

            // The best choice is to upload planeBottomLeft and
            // planeTopRight and kinda of UV coordinate that can go
            // outside [0,1] and the fract is the actual UV value.

            // Fill buffer with Quad Info
            {
              cpuBuffers.oceanData.clear();
              start = std::chrono::high_resolution_clock::now();
              RuntimeCPUBuffers::OceanData *curr =
                  &cpuBuffers.oceanData.emplace_back();

              for (auto it = qt.begin(); it != qt.end(); ++it) {
                runtimeResults.NavigatingTheQuadTree +=
                    std::chrono::duration_cast<
                        decltype(runtimeResults.NavigatingTheQuadTree)>(
                        (std::chrono::high_resolution_clock::now() - start));
                runtimeResults.drawnNodes++;

                {
                  curr->vertexConstants.instanceData[curr->N].scaling = {
                      it->size.x, it->size.y};
                  curr->vertexConstants.instanceData[curr->N].offset = {
                      it->center.x, it->center.y};
                }
                {
                  start = std::chrono::high_resolution_clock::now();

                  auto res = it.GetSmallerNeighbor();

                  runtimeResults.NavigatingTheQuadTree +=
                      std::chrono::duration_cast<
                          decltype(runtimeResults.NavigatingTheQuadTree)>(
                          (std::chrono::high_resolution_clock::now() - start));
                  static const constexpr auto l = [](const float x) -> float {
                    if (x == 0)
                      return 1;
                    else
                      return x;
                  };
                  curr->hullConstants.instanceData[curr->N].TesselationFactor =
                      {l(res.zneg), l(res.xneg), l(res.zpos), l(res.xpos)};
                }

                curr->N = curr->N + 1;
                if (curr->N == Defaults::App::maxInstances) {
                  curr = &cpuBuffers.oceanData.emplace_back();
                }

                start = std::chrono::high_resolution_clock::now();
              }

              // If a quarter of the capacity is unused shrink the vector in a
              // way that the unused capacity is halfed
              // how though?
            }
          }

          // Shadow Map pass
          {
            // ...
          }

          // GBuffer Pass

          frameResource.DeferredShadingBuffers.Clear(allocator);
          allocator.SetRenderTargets(
              {
                  frameResource.DeferredShadingBuffers.Albedo.RenderTarget(),
                  frameResource.DeferredShadingBuffers.Normal.RenderTarget(),
                  frameResource.DeferredShadingBuffers.Position.RenderTarget(),
                  frameResource.DeferredShadingBuffers.MaterialValues
                      .RenderTarget(),
              },
              frameResource.DepthBuffer.DepthStencil());
          {
            WaterGraphicRootDescription::ModelConstants modelConstants{};

            XMStoreFloat4x4(&modelConstants.mMatrix,
                            XMMatrixTranspose(modelMatrix));

            // Debug stuff
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

            // Upload absolute contants

            GpuVirtualAddress modelBuffer =
                frameResource.DynamicBuffer.AddBuffer(modelConstants);

            GpuVirtualAddress waterDataBuffer =
                frameResource.DynamicBuffer.AddBuffer(waterData);

            // Pre translate resources
            GpuVirtualAddress displacementMapAddressHighest =
                *drawingSimResource.HighestBuffer.displacementMap
                     .ShaderResource(allocator);
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

            waterPipelineState.Apply(allocator);

            for (auto &curr : cpuBuffers.oceanData) {
              if (curr.N == 0)
                continue;
              auto mask = waterRootSignature.Set(allocator,
                                                 RootSignatureUsage::Graphics);

              if (usedTextureAddress)
                mask.texture = *usedTextureAddress;

              mask.skybox = skyboxTexture;

              mask.heightMapHighest = displacementMapAddressHighest;
              mask.gradientsHighest = gradientsAddressHighest;
              mask.heightMapMedium = displacementMapAddressMedium;
              mask.gradientsMedium = gradientsAddressMedium;
              mask.heightMapLowest = displacementMapAddressLowest;
              mask.gradientsLowest = gradientsAddressLowest;

              mask.waterPBRBuffer = waterDataBuffer;
              mask.lightingBuffer = sunDataBuffer;

              mask.hullBuffer =
                  frameResource.DynamicBuffer.AddBuffer(curr.hullConstants);
              mask.vertexBuffer =
                  frameResource.DynamicBuffer.AddBuffer(curr.vertexConstants);
              mask.debugBuffer = debugConstantBuffer;
              mask.cameraBuffer = cameraConstantBuffer;
              mask.modelBuffer = modelBuffer;

              planeMesh.Draw(allocator, curr.N);
            }
            // skybox
            {
              skyboxPipelineState.Apply(allocator);
              auto mask = skyboxRootSignature.Set(allocator,
                                                  RootSignatureUsage::Graphics);

              mask.skybox = skyboxTexture;
              mask.lightingBuffer = sunDataBuffer;

              mask.cameraBuffer = cameraConstantBuffer;

              skyboxMesh.Draw(allocator);
            }
          }

          // Deferred Shading Pass
          allocator.SetRenderTargets(
              {renderTargetView},
              frameResource.DeferredShadingBuffers.DepthStencil.DepthStencil());
          {
            frameResource.DeferredShadingBuffers.TranslateToView(allocator);
            allocator.TransitionResource(frameResource.DepthBuffer,
                                         ResourceStates::DepthWrite,
                                         ResourceStates::PixelShaderResource);
            deferredShadingPipelineState.Apply(allocator);
            auto mask = deferredShadingRootSignature.Set(
                allocator, RootSignatureUsage::Graphics);

            mask.albedo =
                *frameResource.DeferredShadingBuffers.Albedo.ShaderResource();
            mask.normal =
                *frameResource.DeferredShadingBuffers.Normal.ShaderResource();
            mask.position =
                *frameResource.DeferredShadingBuffers.Position.ShaderResource();
            mask.materialValues = *frameResource.DeferredShadingBuffers
                                       .MaterialValues.ShaderResource();
            mask.geometryDepth = *frameResource.DepthBuffer.ShaderResource();

            deferredShadingPlane.Draw(allocator);

            allocator.TransitionResource(frameResource.DepthBuffer,
                                         ResourceStates::PixelShaderResource,
                                         ResourceStates::DepthWrite);
            frameResource.DeferredShadingBuffers.TranslateToTarget(allocator);
          }

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
          DrawImGuiForPSResources(waterData, sunData, true);

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