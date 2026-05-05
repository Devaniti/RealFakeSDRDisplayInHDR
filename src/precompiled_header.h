#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "DirectXMath.h"
#include "Windows.h"
#include "d3d11.h"
#include "dxgi1_6.h"
#include "stb/stb_image.h"
#include "wrl/client.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"
