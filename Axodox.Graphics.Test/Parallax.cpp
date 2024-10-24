#include "pch.h"
#include "Parallax.h"

ConeMapCreater
ConeMapCreater::WithDefaultShaders(PipelineStateProvider &pipelineProvider,
                                   GraphicsDevice &device) {
  ComputeShader cs = ComputeShader(app_folder() / L"ConeCreater.cso");
  return ConeMapCreater(pipelineProvider, device, &cs);
}
