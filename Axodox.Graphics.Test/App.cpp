#include "pch.h"
#include "Camera.h"
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Popups.h>

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

struct App : implements<App, IFrameworkViewSource, IFrameworkView> {
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
    RootDescriptor<RootDescriptorType::ConstantBuffer> ConstantBuffer;
    RootDescriptorTable<1> Texture;
    RootDescriptorTable<1> Heightmap;
    StaticSampler TextureSampler;
    StaticSampler HeightmapSampler;

    SimpleRootDescription(const RootSignatureContext &context)
        : RootSignatureMask(context),
          ConstantBuffer(this, {0}, ShaderVisibility::Vertex),
          Texture(this, {DescriptorRangeType::ShaderResource},
                  ShaderVisibility::Pixel),
          TextureSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                  ShaderVisibility::Pixel),
        Heightmap(this, {DescriptorRangeType::ShaderResource},
					ShaderVisibility::Vertex),
    HeightmapSampler(this, {0}, Filter::Linear, TextureAddressMode::Clamp,
                  ShaderVisibility::Vertex){
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

  struct Constants {
    XMFLOAT4X4 WorldViewProjection;
    XMFLOAT4X4 WorldIT;
  };

  D3D12_INPUT_ELEMENT_DESC InputElement(const char* semanticName, uint32_t semanticIndex, DXGI_FORMAT format)
  {
    return {
      .SemanticName = semanticName,
      .SemanticIndex = semanticIndex,
      .Format = format,
      .InputSlot = 0,
      .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
      .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
      .InstanceDataStepRate = 0
    };
  }
  void Run() {
    CoreWindow window = CoreWindow::GetForCurrentThread();
    window.Activate();

    Camera cam;
    cam.SetView(XMVectorSet(-1.0f, 10.0f, 0.0f, 0),
                XMVectorSet(0.0f, 0.0f, 0.0f, 0),
                XMVectorSet(0.0f, 1.0f, 0.0f, 0));
    bool quit = false;
    // Events

    // If I redo the whole system, this could be handled when the callback is
    // made.
    window.KeyDown(
        [&cam, &quit](CoreWindow const &w, KeyEventArgs const &args) {
          if (args.VirtualKey() == Windows::System::VirtualKey::Escape)
            quit = true;
          else

            cam.KeyboardDown(args);
        });
    window.KeyUp([&cam](CoreWindow const &w, KeyEventArgs const &args) {
      cam.KeyboardUp(args);
    });
    window.PointerMoved(
        [&cam](CoreWindow const &w, PointerEventArgs const &args) {
          cam.MouseMove(args);
        });
    window.PointerWheelChanged(
        [&cam](CoreWindow const &w, PointerEventArgs const &args) {
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



const D3D12_INPUT_ELEMENT_DESC VertexInputDescription[] = {
    InputElement("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT),
    InputElement("NORMAL", 0, DXGI_FORMAT_R8G8B8A8_SNORM),
    InputElement("TEXCOORD", 0, DXGI_FORMAT_R16G16_UNORM),
    InputElement("SV_VertexID", 0, DXGI_FORMAT_R32_UINT),
  };
    GraphicsPipelineStateDefinition simplePipelineStateDefinition{
        .RootSignature = &simpleRootSignature,
        .VertexShader = &simpleVertexShader,
        .PixelShader = &simplePixelShader,
        .RasterizerState = RasterizerFlags::CullClockwise,
        .DepthStencilState = DepthStencilMode::WriteDepth,
        .InputLayout =VertexInputDescription,
        .RenderTargetFormats = {Format::B8G8R8A8_UNorm},
        .DepthStencilFormat = Format::D32_Float};
    auto simplePipelineState =
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

    const unsigned int subdivs = 500;
    ImmutableMesh planeMesh{immutableAllocationContext,
                           CreatePlane(1, XMUINT2(subdivs,subdivs))};
    //ImmutableTexture texture{immutableAllocationContext,
    //                         app_folder() / L"image.jpeg"};
    ImmutableTexture texture{immutableAllocationContext,
                             app_folder() / L"iceland_heightmap.png"};
    ImmutableTexture heightmap{immutableAllocationContext,
                             app_folder() / L"iceland_heightmap.png"};

    //ImmutableTexture texture{immutableAllocationContext,
    //                         app_folder() / L"gradient.jpg"};
    // Acquire memory
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
    static const XMFLOAT4 backgroundColor = {0, 1, 0, 0};
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
      Constants constants{};
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
        renderTargetView->Clear(allocator, backgroundColor);
        resources.DepthBuffer.DepthStencil()->Clear(allocator);
      }

      // Draw objects
      for (int i =0; i < 3;i++)
        for (int j = 0; j < 3; j++)
      {
          auto vec = XMVectorSet(i-1, 0, j-1, 1);
        {
          auto world =
              XMMatrixRotationX(XMConvertToRadians(-90.0))* XMMatrixTranslationFromVector(vec);

          auto worldViewProjection =
              XMMatrixTranspose(world * cam.GetViewProj());
          auto worldIT = XMMatrixInverse(nullptr, world);

          XMStoreFloat4x4(&constants.WorldViewProjection, worldViewProjection);
          XMStoreFloat4x4(&constants.WorldIT, worldIT);
        }
      
        auto mask =
            simpleRootSignature.Set(allocator, RootSignatureUsage::Graphics);
        mask.ConstantBuffer = resources.DynamicBuffer.AddBuffer(constants);
        mask.Texture = texture;
        mask.Heightmap = heightmap;

        allocator.SetRenderTargets({renderTargetView},
                                   resources.DepthBuffer.DepthStencil());
        simplePipelineState.Apply(allocator);
        planeMesh.Draw(allocator);
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
  }

  void SetWindow(CoreWindow const & /*window*/) {}
};

#include <fstream>
#include <string.h>
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        CoreApplication::Run(make<App>());
    }
    catch (const winrt::hresult_error& ex) {
        // Log the error message and HRESULT
        OutputDebugStringW(ex.message().c_str());
        std::wstringstream ss;
        ss << L"HRESULT: " << std::hex << ex.code() << std::endl;
        OutputDebugStringW(ss.str().c_str());
        // Optionally, you can display a message box
        fstream f(
            "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

        f <<to_string(ex.message());
    }
    catch (const std::exception& ex) {
        // Log the standard exception message
        OutputDebugStringA(ex.what());
        fstream f(
            "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

        f << ex.what();
    }
    catch (...) {
        // Catch all other exceptions
        OutputDebugStringW(L"Unknown error occurred.");
        fstream f(
            "E:\\Minden\\dev\\axodox-graphics\\Axodox.Graphics.Test\\out.txt");

        f << "Unknown error";
    }
}