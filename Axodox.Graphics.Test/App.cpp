#pragma once
#include "pch.h"
#include "App.h"
#include "Meshes.h"
#include "TestConfigLoader.h"

namespace Reun {
using namespace winrt;
using namespace Windows;
using namespace Windows::UI::Core;
using namespace DirectX;
using namespace Meshes;

App::App(AppShared &shared)
    : shared_(shared),
      swapChain{directQueue, shared_.window, SwapChainFlags::IsTearingAllowed},
      imgui_wrapper_(device, shared_.settings.framesInFlight,
                     shared_.settings.ImGuiIniPath) {

  cam.SetView(XMVectorSet(DefaultsValues::Cam::camStartPos.x,
                          DefaultsValues::Cam::camStartPos.y,
                          DefaultsValues::Cam::camStartPos.z, 0),
              XMVectorSet(0.0f, 0.0f, 0.0f, 0),
              XMVectorSet(0.0f, 1.0f, 0.0f, 0));

  cam.SetFirstPerson(DefaultsValues::Cam::startFirstPerson);
  SetWindow();
}
void App::DeleteApp(std::unique_ptr<App> &app) {
  app->Suspend();

  app.reset();
}

void App::Suspend() {
  shared_.cout << "Suspending App " << std::endl;
  WaitForShutDown();
}
void App::Resume() {
  shouldStop_ = false;
  shared_.cout << "Resuming App " << std::endl;
  StartRun();
}
void App::WaitForShutDown() {
  shouldStop_ = true;
  while (isRunning_) {
    std::this_thread::yield();
  }
}

void App::StartRun() {
  if (isRunning_)
    return;
  internalRestartRequest_ = false;
  isRunning_ = true;
  Run();
  isRunning_ = false;
}
using namespace Windows::UI::ViewManagement;
bool App::ShouldRestart() const {
  // we stopped execution, but not because of a quit request, the app wants a
  // restart!
  return !isRunning_ && internalRestartRequest_;
}

void App::KeyDown(CoreWindow const &, KeyEventArgs const &args) {
  if (ImGui::GetIO().WantCaptureKeyboard)
    return;
  auto applicationView = ApplicationView::GetForCurrentView();
  switch (args.VirtualKey()) {
  case Windows::System::VirtualKey::Escape:
    quitRequested_ = true;
    break;
  case Windows::System::VirtualKey::Space:
    settings.timeRunning = !settings.timeRunning;
    break;
  case Windows::System::VirtualKey::F1:
    settings.showImgui = !settings.showImgui;
    break;
  case Windows::System::VirtualKey::F2:

    if (!applicationView.IsFullScreenMode()) {
      bool success = applicationView.TryEnterFullScreenMode();
      if (!success) {
        OutputDebugString(L"Failed to enter fullscreen mode.");
      }
    } else {

      applicationView.ExitFullScreenMode();
    }
    break;

  default:
    cam.KeyboardDown(args);
    break;
  }
};

void App::KeyUp(CoreWindow const &, KeyEventArgs const &args) {

  if (ImGui::GetIO().WantCaptureKeyboard)
    return;
  cam.KeyboardUp(args);
};
void App::MouseMoved(CoreWindow const &, PointerEventArgs const &args) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  cam.MouseMove(args);
};
void App::MouseWheel(CoreWindow const &, PointerEventArgs const &args) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  cam.MouseWheel(args);
};

void App::SetWindow() {
  const auto &window = shared_.window;
  window.KeyDown([this](const CoreWindow &w, const KeyEventArgs &args) {
    this->KeyDown(w, args);
  });
  window.KeyUp([this](const CoreWindow &w, const KeyEventArgs &args) {
    this->KeyUp(w, args);
  });
  window.PointerMoved(
      [this](const CoreWindow &w, const PointerEventArgs &args) {
        this->MouseMoved(w, args);
      });
  window.PointerWheelChanged(
      [this](const CoreWindow &w, const PointerEventArgs &args) {
        this->MouseWheel(w, args);
      });
}

void DrawImGuiForPSResources(
    Reun::Graphics::WaterGraphicRootDescription::WaterPixelShaderData
        &waterData,
    Reun::Graphics::PixelLighting &sunData,
    Reun::Graphics::DeferredShading::DeferredShaderBuffers &defData,
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
    ImGui::SliderFloat("Foam Depth Falloff", &waterData.foamDepthFalloff, 0.0f,
                       10.0f);
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
    ImGui::SliderFloat("Ambient Mult", &sunData.lights[0].AmbientColor.w, 0.0f,
                       10.0f);
    ImGui::Separator();
    ImGui::Text("DeferredShaderBuffer Data");
    ImGui::SliderFloat("Env Map", &defData.EnvMapMult, 0, 2);
  }
  if (exclusiveWindow)
    ImGui::End();
}

void App::Run() {

  ImmutableMesh planeMesh{descriptors_.immutableAllocationContext,
                          CreateQuadPatch()};
  ImmutableMesh simplePlane{descriptors_.immutableAllocationContext,
                            CreatePlane(2, XMUINT2(2, 2))};

  ImmutableMesh deferredShadingPlane{descriptors_.immutableAllocationContext,
                                     CreateBackwardsPlane(2, XMUINT2(2, 2))};
  ImmutableMesh skyboxMesh{descriptors_.immutableAllocationContext,
                           CreateCube(2)};
  ImmutableMesh Box{descriptors_.immutableAllocationContext, CreateCube(2)};
  ImmutableMesh BoxWithoutBottom{descriptors_.immutableAllocationContext,
                                 CreateCubeWithoutBottom(1)};
  /*ImmutableMesh BoxOnlyWithIndexBuffer{immutableAllocationContext,
  CreateBoxInVSMesh()};*/
  // ImmutableMesh BoxWithoutBottom{immutableAllocationContext,
  // CreateCube(1)};

  const CubeMapPaths paths = {.PosX = app_folder() / "Assets/skybox/px.png",
                              .NegX = app_folder() / "Assets/skybox/nx.png",
                              .PosY = app_folder() / "Assets/skybox/py.png",
                              .NegY = app_folder() / "Assets/skybox/ny.png",
                              .PosZ = app_folder() / "Assets/skybox/pz.png",
                              .NegZ = app_folder() / "Assets/skybox/nz.png"};
  CubeMapTexture skyboxTexture{descriptors_.immutableAllocationContext, paths};
  // CubeMapTexture skyboxTexture{immutableAllocationContext,
  //                              app_folder() / "Assets/skybox/skybox3.hdr",
  //                              2024};

  //  Acquire memory
  MeshSpecificBuffers silhouetteDetectorMeshBuffers(
      descriptors_.immutableAllocationContext, Box);
  descriptors_.groupedResourceAllocator.Build();

  auto &mutableAllocationContext = descriptors_.mutableAllocationContext;
  auto &commonDescriptorHeap = descriptors_.commonDescriptorHeap;

  SimulationStage::ConstantGpuSources simulationConstantSources(
      descriptors_.mutableAllocationContext, simData);
  SimulationStage::MutableGpuSources simulationMutableSources(
      mutableAllocationContext, simData);

  // SilhouetteDetector::Buffers silhouetteDetectorBuffers(
  //     mutableAllocationContext, Box.GetIndexCount() * 4);

  std::array<Graphics::FrameResources, 2> frameResources{
      Graphics::FrameResources(mutableAllocationContext),
      Graphics::FrameResources(mutableAllocationContext)};

  std::array<SimulationStage::SimulationResources, 2> simulationResources{
      SimulationStage::SimulationResources(mutableAllocationContext, simData.N,
                                           simData.M),
      SimulationStage::SimulationResources(mutableAllocationContext, simData.N,
                                           simData.M)};
  // std::vector<FrameResources> frameResources;
  // frameResources.reserve((usize)shared_.settings.framesInFlight);
  // std::vector<SimulationStage::SimulationResources> simulationResources;
  // simulationResources.reserve((usize)shared_.settings.framesInFlight);
  // for (u8 i = 0; i < shared_.settings.framesInFlight; i++) {
  //   frameResources.emplace_back(mutableAllocationContext);

  //  simulationResources.emplace_back(mutableAllocationContext, simData.N,
  //                                   simData.M);
  //}

  descriptors_.committedResourceAllocator.Build();

  const u32 &N = simData.N;

  swapChain.Resizing(no_revoke, [this, &frameResources,
                                 &commonDescriptorHeap](SwapChain const *self) {
    for (auto &frame : frameResources)
      frame.ScreenResourceView.reset();
    commonDescriptorHeap.Clean();
    auto resolution = self->Resolution();
    cam.SetAspect(float(resolution.x) / float(resolution.y));
  });

  {
    auto resolution = swapChain.Resolution();
    cam.SetAspect(float(resolution.x) / float(resolution.y));
  }

  loopStartTime = std::chrono::high_resolution_clock::now();

  // Main loop
  // ------------------------------------------------
  while (!quitRequested_ && !shouldStop_ && !internalRestartRequest_) {

    // Process user input
    shared_.dispatcher.ProcessEvents(
        CoreProcessEventsOption::ProcessAllIfPresent);

    frameCounter_++;
    // Current frames resources
    auto &frameResource = frameResources[frameCounter_ & 0x1u];
    // Simulation resources for drawing.
    auto &drawingSimResource = simulationResources[frameCounter_ & 0x1u];
    // Simulation resources for calculating.
    auto &calculatingSimResource =
        simulationResources[(frameCounter_ + 1u) & 0x1u];

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
      if (beforeNextFrame_.changeFlag) {
        fullRenderPipeline.waterPipelineStateDefinition.RasterizerState.Flags =
            *beforeNextFrame_.changeFlag;
        newData.pipelineState =
            descriptors_.pipelineStateProvider_.CreatePipelineStateAsync(
                fullRenderPipeline.waterPipelineStateDefinition);
      }
      if (beforeNextFrame_.patchHighestChanged) {
        newData.highestData =
            SimulationStage::ConstantGpuSources<>::LODDataSource(
                mutableAllocationContext, simData.highest);
      }
      if (beforeNextFrame_.patchMediumChanged) {
        newData.mediumData =
            SimulationStage::ConstantGpuSources<>::LODDataSource(
                mutableAllocationContext, simData.medium);
      }
      if (beforeNextFrame_.patchLowestChanged) {
        newData.lowestData =
            SimulationStage::ConstantGpuSources<>::LODDataSource(
                mutableAllocationContext, simData.lowest);
      }
    }

    // Wait until buffers can be used
    frameResource.Wait();
    // This is necessary for the compute queue
    calculatingSimResource.Wait();

    if (beforeNextFrame_.changeFlag && newData.pipelineState) {
      fullRenderPipeline.waterPipelineState = newData.pipelineState->get();
      beforeNextFrame_.changeFlag = std::nullopt;
    }

    // Get DeltaTime
    CalculateTimeConstants();

    frameResource.MakeCompatible(*renderTargetView, mutableAllocationContext);

    bool camChanged = cam.Update(timeConstants_.trueDeltaTime) || first_loop;

    // Frame Begin
    {
      descriptors_.committedResourceAllocator.Build();
      descriptors_.depthStencilDescriptorHeap.Build();
      descriptors_.renderTargetDescriptorHeap.Build();
      commonDescriptorHeap.Build();
    }

    // QuadTrees
    std::future<std::vector<Graphics::WaterGraphicRootDescription::OceanData> &>
        oceanDataFuture;
    if (debugValues.drawMethod == DebugValues::DrawTechnology::Tesselation ||
        debugValues.drawMethod == DebugValues::DrawTechnology::PrismParallax ||
        first_loop) {
      oceanDataFuture = threadpool_execute<
          std::vector<Graphics::WaterGraphicRootDescription::OceanData> &>(
          [this, camChanged]()
              -> std::vector<Graphics::WaterGraphicRootDescription::OceanData>
                  & {
                    if (camChanged && !debugValues.lockQuadTree) {
                      cpuBuffers.oceanData.clear();
                      return Graphics::WaterGraphicRootDescription::
                          CollectOceanQuadInfoWithQuadTree(
                              cpuBuffers.oceanData, cam, oceanModelMatrix,
                              simData.quadTreeDistanceThreshold,
                              simData.maxDepth, debugValues, &runtimeResults_);
                    }
                    return cpuBuffers.oceanData;
                  });
    }

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
        // If there is new data, we have to halt all running computes
        for (auto &x : simulationResources) {
          x.Wait();
        }
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
                const SimulationStage::ConstantGpuSources<>::LODDataSource &src,
                const SimulationStage::ConstantGpuSources<>::LODDataSource
                    &dst) {
              copyRes(src.Tildeh0, dst.Tildeh0);
              copyRes(src.Frequencies, dst.Frequencies);
            };
        if (newData.highestData) {
          copyLOD(*newData.highestData, simulationConstantSources.Highest);
          beforeNextFrame_.patchHighestChanged = false;
        }
        if (newData.mediumData) {
          copyLOD(*newData.mediumData, simulationConstantSources.Medium);
          beforeNextFrame_.patchMediumChanged = false;
        }
        if (newData.lowestData) {
          copyLOD(*newData.lowestData, simulationConstantSources.Lowest);
          beforeNextFrame_.patchLowestChanged = false;
        }
      }

      // Since we are using this on different queues, it is uploaded twice.
      GpuVirtualAddress timeDataBuffer =
          simResource.DynamicBuffer.AddBuffer(timeConstants_);

      WaterSimulationComputeShader(
          simResource, simulationConstantSources, simulationMutableSources,
          simData, fullSimPipeline, computeAllocator, timeDataBuffer, N,
          debugValues, debugValues.getChannels());

      // Upload queue
      {
        auto commandList = computeAllocator.EndList();
        computeAllocator.BeginList();
        simResource.DynamicBuffer.UploadResources(computeAllocator);
        descriptors_.resourceUploader.UploadResourcesAsync(computeAllocator);
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
        allocator.TransitionResource(*renderTargetView, ResourceStates::Present,
                                     ResourceStates::RenderTarget);

        commonDescriptorHeap.Set(allocator);

        renderTargetView->Clear(allocator, settings.clearColor);
        frameResource.Clear(allocator);
      }

      // Global data
      Graphics::GlobalGPUBuffers globalBuffers =
          CreateGlobalGPUBuffers(frameResource.DynamicBuffer);

      {
        // Draw Ocean
        Graphics::ConstantGPUBuffers constantBuffers{
            .waterData = frameResource.DynamicBuffer.AddBuffer(waterData),
            .defData = frameResource.DynamicBuffer.AddBuffer(defData),
        };
        Graphics::OtherInput others{
            .oceanModelMatrix = oceanModelMatrix,
            .debugValues = debugValues,
            .oceanDataFuture = oceanDataFuture,
        };
        Graphics::Meshes meshes{
            .planeMesh = planeMesh,
            .simplePlane = simplePlane,
            .BoxWithoutBottom = BoxWithoutBottom,
            .skyboxMesh = skyboxMesh,
            .deferredShadingPlane = deferredShadingPlane,
        };
        Graphics::Textures textures{.skyboxTexture = skyboxTexture};
        Graphics::RenderFrameContext renderFrameContext{
            .renderTargetView = renderTargetView,
            .frameResources = frameResource,
            .commandQueue = {&directQueue, 1},
            .globalBuffers = globalBuffers,
            .constantBuffers = constantBuffers,
            .drawingSimResource = drawingSimResource,
            .others = others,
            .meshes = meshes,
            .textures = textures,

        };
        fullRenderPipeline.Execute(renderFrameContext);
      }

      auto CPURenderEnd = std::chrono::high_resolution_clock::now();
      runtimeResults_.CPUTime = CPURenderEnd - currentFrameStart;
      DrawImGuiMenu(allocator, frameResource, drawingSimResource);

      //  End frame command list
      {
        allocator.TransitionResource(*renderTargetView,
                                     ResourceStates::RenderTarget,
                                     ResourceStates::Present);
        auto drawCommandList = allocator.EndList();

        allocator.BeginList();
        frameResource.DynamicBuffer.UploadResources(allocator);
        descriptors_.resourceUploader.UploadResourcesAsync(allocator);
        auto initCommandList = allocator.EndList();

        directQueue.Execute(initCommandList);
        directQueue.Execute(drawCommandList);
      }
      frameResource.Marker = frameResource.Fence.EnqueueSignal(directQueue);
    }

    // Present frame
    computeStage.wait();
    swapChain.Present();
    first_loop = false;
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
  // hijack a fence and marker to use as last final shutdown sync
  auto &x = frameResources.front();
  x.Marker = x.Fence.EnqueueSignal(directQueue);
  x.Fence.Await(x.Marker);
}
Reun::Graphics::GlobalGPUBuffers
App::CreateGlobalGPUBuffers(DynamicBufferManager &bufferManager) {

  Graphics::GlobalGPUBuffers globalBuffers;

  CameraConstants cameraConstants{};
  DebugGPUBufferStuff debugBufferContent = From(debugValues, simData);

  XMStoreFloat3(&cameraConstants.cameraPos, cam.GetEye());
  XMStoreFloat4x4(&cameraConstants.vMatrix,
                  XMMatrixTranspose(cam.GetViewMatrix()));
  XMStoreFloat4x4(&cameraConstants.pMatrix, XMMatrixTranspose(cam.GetProj()));
  XMStoreFloat4x4(&cameraConstants.vpMatrix,
                  XMMatrixTranspose(cam.GetViewProj()));
  XMStoreFloat4x4(&cameraConstants.INVvMatrix,
                  XMMatrixTranspose(cam.GetINVView()));
  XMStoreFloat4x4(&cameraConstants.INVpMatrix,
                  XMMatrixTranspose(cam.GetINVProj()));
  XMStoreFloat4x4(&cameraConstants.INVvpMatrix,
                  XMMatrixTranspose(cam.GetINVViewProj()));
  globalBuffers.cameraConstantBuffer = bufferManager.AddBuffer(cameraConstants);
  globalBuffers.debugConstantBuffer =
      bufferManager.AddBuffer(debugBufferContent);
  globalBuffers.lightsConstantBuffer = bufferManager.AddBuffer(sunData);
  globalBuffers.timeDataBuffer = bufferManager.AddBuffer(timeConstants_);
  return globalBuffers;
}
void App::DrawImGuiMenu(
    CommandAllocator &allocator, Graphics::FrameResources &frameResource,
    SimulationStage::SimulationResources &drawingSimResource) {
  // ImGUI
  if (settings.showImgui) {
    imgui_wrapper_.Pre(allocator);

    if (ImGui::Begin("Application")) {
      shared_.prints += shared_.cout.str();
      shared_.cout.str("");
      ImGui::Text("Press ESC to quit");
      ImGui::Text("Press Space to stop time");
      ImGui::Text("frame %d", frameCounter_);
      ImGui::Text(" %.3f s",
                  GetDurationInFloatWithPrecision<std::chrono::seconds,
                                                  std::chrono::milliseconds>(
                      GetTimeSinceStart()));
      ImGui::Text(" %.3f ms/frame (%.1f FPS)",
                  1000.0f / imgui_wrapper_.GetIO().Framerate,
                  imgui_wrapper_.GetIO().Framerate);

      settings.DrawImGui(beforeNextFrame_);

      runtimeResults_.DrawImGui(false);
      cam.DrawImGui(false);
      for (int i = 0; i < 3; ++i) {
        ImGui::Text(std::format("{}", i).c_str());
        ImGui::SameLine();
        ImGui::Image(
            (void *)((*drawingSimResource.LODs[i]->gradients.ShaderResource(
                          allocator))
                         .GpuHandle()
                         .ptr),
            ImVec2(256, 256));

        if (i != 2)
          ImGui::SameLine();
      }

      ImGui::Text("Albedo");
      ImGui::SameLine();
      ImGui::Image((void *)((*frameResource.GBuffer.Albedo.ShaderResource())
                                .GpuHandle()
                                .ptr),
                   ImVec2(256, 256));
      ImGui::SameLine();

      ImGui::Text("Material");
      ImGui::SameLine();
      ImGui::Image(
          (void *)((*frameResource.GBuffer.MaterialValues.ShaderResource())
                       .GpuHandle()
                       .ptr),
          ImVec2(256, 256));
      ImGui::SameLine();
      ImGui::Text("Normal");
      ImGui::SameLine();

      ImGui::Image((void *)((*frameResource.GBuffer.Normal.ShaderResource())
                                .GpuHandle()
                                .ptr),
                   ImVec2(256, 256));
      ImGui::SameLine();

      ImGui::Text("LOGS:\n---------------------\n%s", shared_.prints.c_str());
    }
    ImGui::End();
    debugValues.DrawImGui(beforeNextFrame_);
    simData.DrawImGui(beforeNextFrame_);
    DrawImGuiForPSResources(waterData, sunData, defData, true);

    ShowImguiLoaderConfig(debugValues, simData, waterData, sunData, defData,
                          settings, cam, beforeNextFrame_, true);
    imgui_wrapper_.Render(allocator);
  }
}
App::SinceTimeStartTimeFrame App::GetTimeSinceStart() {
  return std::chrono::duration_cast<SinceTimeStartTimeFrame>(
      std::chrono::high_resolution_clock::now() - loopStartTime);
};
void App::CalculateTimeConstants() {

  auto oldFrameStart = currentFrameStart;
  currentFrameStart = std::chrono::high_resolution_clock::now();

  float deltaTime = GetDurationInFloatWithPrecision<std::chrono::seconds,
                                                    std::chrono::milliseconds>(
      currentFrameStart - oldFrameStart);

  if (settings.timeRunning) {
    gameTime += deltaTime;
  }
  timeConstants_ = TimeData{.trueDeltaTime = deltaTime,
                            .deltaTime = settings.timeRunning ? deltaTime : 0,
                            .timeSinceLaunch = gameTime};
}

Descriptors::Descriptors(GraphicsDevice &device, StartUpSettings &settings)
    : pipelineStateProvider_{device, settings.cacheLocation / "pipeline"},
      groupedResourceAllocator{device}, resourceUploader{device},
      commonDescriptorHeap{device, settings.framesInFlight},
      depthStencilDescriptorHeap{device}, renderTargetDescriptorHeap{device},
      committedResourceAllocator{device},
      immutableAllocationContext{
          .Device = &device,
          .ResourceAllocator = &groupedResourceAllocator,
          .ResourceUploader = &resourceUploader,
          .CommonDescriptorHeap = &commonDescriptorHeap,
          .RenderTargetDescriptorHeap = &renderTargetDescriptorHeap,
          .DepthStencilDescriptorHeap = &depthStencilDescriptorHeap},

      mutableAllocationContext{
          .Device = &device,
          .ResourceAllocator = &committedResourceAllocator,
          .ResourceUploader = &resourceUploader,
          .CommonDescriptorHeap = &commonDescriptorHeap,
          .RenderTargetDescriptorHeap = &renderTargetDescriptorHeap,
          .DepthStencilDescriptorHeap = &depthStencilDescriptorHeap} {}
void Descriptors::Build() {}
} // namespace Reun
