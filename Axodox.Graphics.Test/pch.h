﻿#pragma once
#define NOMINMAX

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>

#define PLATFORM_WINDOWS
#include "../Axodox.Graphics.Shared/Include/Axodox.Graphics.D3D12.h"

#include "../ImGUI/Includes/includes.h"

#include "Typedefs.h"
#include "ConstantGPUBuffer.h"