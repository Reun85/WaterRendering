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
using namespace Windows ::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Composition;

using namespace Axodox::Graphics::D3D12;
using namespace Axodox::Infrastructure;
using namespace Axodox::Storage;
using namespace DirectX;
using namespace DirectX::PackedVector;
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
    static const constexpr float3 camStartPos = float3(-0.1, 300, 0);
    static const constexpr RasterizerFlags flags = RasterizerFlags::Wireframe;
    // static constexpr RasterizerFlags flags = RasterizerFlags::CullClockwise;
    static const constexpr XMFLOAT4 backgroundColor = {0, 1, 0, 0};
    static const constexpr bool startFirstPerson = true;
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
    MutableTexture PostProcessingBuffer;
    descriptor_ptr<ShaderResourceView> ScreenResourceView;

    FrameResources(const ResourceAllocationContext &context)
        : Allocator(*context.Device), Fence(*context.Device), Marker(),
          DynamicBuffer(*context.Device), DepthBuffer(context),
          PostProcessingBuffer(context) {}
  };

  struct SimpleRootDescription : public RootSignatureMask {
    RootDescriptor<RootDescriptorType::ConstantBuffer> HullBuffer;
    RootDescriptor<RootDescriptorType::ConstantBuffer> DomainBuffer;
    RootDescriptorTable<1> Texture;
    RootDescriptorTable<1> Heightmap;
    StaticSampler TextureSampler;
    StaticSampler HeightmapSampler;

    SimpleRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          HullBuffer(this, {0}, ShaderVisibility::Hull),
          DomainBuffer(this, {0}, ShaderVisibility::Domain),
          Texture(this, {DescriptorRangeType::ShaderResource},
                  ShaderVisibility::Pixel),
          TextureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                         ShaderVisibility::Pixel),
          Heightmap(this, {DescriptorRangeType::ShaderResource},
                    ShaderVisibility::Domain),
          HeightmapSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                           ShaderVisibility::Domain) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct PostProcessingRootDescription : public RootSignatureMask {
    RootDescriptor<RootDescriptorType::ConstantBuffer> ConstantBuffer;
    RootDescriptorTable<1> InputTexture;
    RootDescriptorTable<1> OutputTexture;
    StaticSampler Sampler;

    PostProcessingRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context), ConstantBuffer(this, {0}),
          InputTexture(this, {DescriptorRangeType::ShaderResource}),
          OutputTexture(this, {DescriptorRangeType::UnorderedAccess}),
          Sampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp) {
      Flags = RootSignatureFlags::AllowInputAssemblerInputLayout;
    }
  };

  struct DomainConstants {

    // There is no 2x2?
    XMFLOAT4X4 TextureTransform;
    XMFLOAT4X4 WorldIT;
  };

  struct HullConstants {
    XMFLOAT4X4 WorldViewProjection;
    // zneg,xneg, zpos, xpos
    XMFLOAT4 TesselationFactor;
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

    // If I redo the whole system, this could be handled when the callback is
    // made.
    window.KeyDown([&cam, &quit, &firstperson](CoreWindow const &,
                                               KeyEventArgs const &args) {
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
      cam.KeyboardUp(args);
    });
    window.PointerMoved(
        [&cam](CoreWindow const &, PointerEventArgs const &args) {
          cam.MouseMove(args);
        });
    window.PointerWheelChanged(
        [&cam](CoreWindow const &, PointerEventArgs const &args) {
          cam.MouseWheel(args);
        });

    CoreDispatcher dispatcher = window.Dispatcher();

    GraphicsDevice device{};
    CommandQueue directQueue{device};
    CoreSwapChain swapChain{directQueue, window,
                            SwapChainFlags::IsShaderResource};

    PipelineStateProvider pipelineStateProvider{device};

    RootSignature<SimpleRootDescription> simpleRootSignature{device};

    VertexShader simpleVertexShader{app_folder() / L"SimpleVertexShader.cso"};
    PixelShader simplePixelShader{app_folder() / L"SimplePixelShader.cso"};
    HullShader hullShader{app_folder() / L"hullShader.cso"};
    DomainShader domainShader{app_folder() / L"domainShader.cso"};

    GraphicsPipelineStateDefinition simplePipelineStateDefinition{
        .RootSignature = &simpleRootSignature,
        .VertexShader = &simpleVertexShader,
        .DomainShader = &domainShader,
        .HullShader = &hullShader,
        .PixelShader = &simplePixelShader,
        .RasterizerState = Defaults::flags,
        .DepthStencilState = DepthStencilMode::WriteDepth,
        .InputLayout = VertexPositionTexture::Layout,
        .TopologyType = PrimitiveTopologyType::Patch,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
        .DepthStencilFormat = Format::D32_Float};
    Axodox::Graphics::D3D12::PipelineState simplePipelineState =
        pipelineStateProvider
            .CreatePipelineStateAsync(simplePipelineStateDefinition)
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
    ImmutableTexture texture{immutableAllocationContext,
                             app_folder() / L"iceland_heightmap.png"};
    ImmutableTexture heightmap{immutableAllocationContext,
                               app_folder() / L"iceland_heightmap.png"};

    // ImmutableTexture texture{immutableAllocationContext,
    //                          app_folder() / L"gradient.jpg"};
    //  Acquire memory
    groupedResourceAllocator.Build();

    auto mutableAllocationContext = immutableAllocationContext;
    CommittedResourceAllocator committedResourceAllocator{device};
    mutableAllocationContext.ResourceAllocator = &committedResourceAllocator;

    array<FrameResources, 2> frames{mutableAllocationContext,
                                    mutableAllocationContext};

    swapChain.Resizing(no_revoke, [&](SwapChain *) {
      for (auto &frame : frames)
        frame.ScreenResourceView.reset();
      commonDescriptorHeap.Clean();
    });

    // Frame counter
    auto i = 0u;
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

      // Ensure post processing buffer
      if (!resources.PostProcessingBuffer ||
          !TextureDefinition::AreSizeCompatible(
              *resources.PostProcessingBuffer.Definition(),
              renderTargetView->Definition())) {
        auto postProcessingDefinition =
            renderTargetView->Definition().MakeSizeCompatible(
                Format::B8G8R8A8_UNorm, TextureFlags::UnorderedAccess);
        resources.PostProcessingBuffer.Allocate(postProcessingDefinition);
      }

      // Update constants
      HullConstants hullConstants{};
      DomainConstants domainConstants{};
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
        renderTargetView->Clear(allocator, Defaults::backgroundColor);
        resources.DepthBuffer.DepthStencil()->Clear(allocator);
      }

      // Draw objects
      {
        float3 center = {0, 0, 0};
        float2 fullSizeXZ = {Defaults::planeSize, Defaults::planeSize};
        QuadTree qt;
        XMFLOAT3 cam_eye;

        XMStoreFloat3(&cam_eye, cam.GetEye());
        qt.Build(center, fullSizeXZ, float3(cam_eye.x, cam_eye.y, cam_eye.z));

        auto worldBasic = XMMatrixRotationX(XMConvertToRadians(-90.0));
        for (auto it = qt.begin(); it != qt.end(); ++it) {

          // if ((&*it - &qt.GetRoot()) != ((i / 60) % qt.GetSize()))
          //   continue;
          {
            auto t1 = it->size;
            auto t2 = it->center;
            auto t3 = float3(t2.x, center.y, t2.y);
            auto world =
                worldBasic *
                XMMatrixScaling(it->size.x / fullSizeXZ.x, 1,
                                it->size.y / fullSizeXZ.y) *
                XMMatrixTranslation(it->center.x, center.y, it->center.y);
            // auto world =
            //     worldBasic * XMMatrixScaling(it->size.x, 1, it->size.y) *
            //     XMMatrixTranslation(it->center.x, center.y, it->center.y);
            auto worldViewProjection =
                XMMatrixTranspose(world * cam.GetViewProj());
            auto worldIT = XMMatrixInverse(nullptr, world);

            XMStoreFloat4x4(&hullConstants.WorldViewProjection,
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
            auto res = it.GetSmallerNeighbor();
            static const constexpr auto l = [](float x) -> float {
              if (x == 0)
                return 1;
              else
                return 1 / x;
            };
            hullConstants.TesselationFactor = {l(res.zneg), l(res.xneg),
                                               l(res.zpos), l(res.xpos)};
            // hullConstants.TesselationFactor[0] = l(res.zneg);
            // hullConstants.TesselationFactor[1] = l(res.xneg);
            // hullConstants.TesselationFactor[2] = l(res.right);
            // hullConstants.TesselationFactor[3] = l(res.xpos);
          }

          auto mask =
              simpleRootSignature.Set(allocator, RootSignatureUsage::Graphics);
          mask.HullBuffer = resources.DynamicBuffer.AddBuffer(hullConstants);
          mask.DomainBuffer =
              resources.DynamicBuffer.AddBuffer(domainConstants);
          mask.Texture = texture;
          mask.Heightmap = heightmap;

          allocator.SetRenderTargets({renderTargetView},
                                     resources.DepthBuffer.DepthStencil());
          simplePipelineState.Apply(allocator);
          planeMesh.Draw(allocator);
        }
      }
      //{

      //  auto worldBasic = XMMatrixRotationX(XMConvertToRadians(-90.0));
      //  auto world =

      //      worldBasic *
      //      XMMatrixScaling(Defaults::planeSize / 2, 1,
      //                      Defaults::planeSize / 2) *
      //      XMMatrixTranslation(
      //          -Defaults::planeSize / 4 + Defaults::planeSize / 2, -0.2,
      //          -Defaults::planeSize / 4 + Defaults::planeSize / 2);

      //  auto worldViewProjection = XMMatrixTranspose(world *
      //  cam.GetViewProj()); auto worldIT = XMMatrixInverse(nullptr, world);

      //  XMStoreFloat4x4(&hullConstants.WorldViewProjection,
      //                  worldViewProjection);
      //  XMStoreFloat4x4(&domainConstants.WorldIT, worldIT);

      //  // Why no 2x2?
      //  XMStoreFloat4x4(&domainConstants.TextureTransform,
      //  XMMatrixIdentity());

      //  hullConstants.TesselationFactor = {1, 1, 1, 1};
      //  auto mask =
      //      simpleRootSignature.Set(allocator, RootSignatureUsage::Graphics);
      //  mask.HullBuffer = resources.DynamicBuffer.AddBuffer(hullConstants);
      //  mask.DomainBuffer =
      //  resources.DynamicBuffer.AddBuffer(domainConstants); mask.Texture =
      //  texture; mask.Heightmap = heightmap;

      //  allocator.SetRenderTargets({renderTargetView},
      //                             resources.DepthBuffer.DepthStencil());
      //  simplePipelineState.Apply(allocator);

      //  planeMesh.Draw(allocator);
      //}

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
  }

  void SetWindow(CoreWindow const & /*window*/) {}
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

  try {
    CoreApplication::Run(make<App>());
  } catch (const winrt::hresult_error &ex) {
    std::wstringstream ss;
    ss << L"HRESULT: " << std::hex << ex.code() << std::endl;
    OutputDebugStringW(ss.str().c_str());
    ofstream f(
        "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

    f << to_string(ex.message());
  } catch (const std::exception &ex) {
    ofstream f(
        "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

    f << ex.what();
  } catch (...) {
    // Catch all other exceptions
    OutputDebugStringW(L"Unknown error occurred.");
    ofstream f(
        "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

    f << "Unknown error";
  }
}