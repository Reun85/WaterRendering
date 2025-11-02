#pragma once
#define NOMINMAX
#define PLATFORM_WINDOWS

#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <span>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <math.h>
#include <chrono>
#include <random>
#include <complex>
#include <numbers>
#include <sstream>
#include <array>
#include <ranges>

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Storage.h>

#include "../Axodox.Graphics.Shared/Include/Axodox.Graphics.D3D12.h"

#include "../ImGUI/Includes/includes.h"

// Own
#include "Typedefs.h"
#include "WrapperAddons/includes.h"
#include "constants.h"
#include "Defaults.h"
#include "Helpers.h"
#include "Camera.h"
#include "ImGuiHelper.h"
