#pragma once
#define NOMINMAX
#define PLATFORM_WINDOWS

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
#include "Helpers.h"
