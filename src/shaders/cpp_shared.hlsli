#ifndef CPP_SHADER_HLSLI
#define CPP_SHADER_HLSLI

#ifdef __cplusplus
#include <cstdint>
#include "DirectXMath.h"

namespace ShaderTypes
{
    using uint = uint32_t;
    using float4x4 = DirectX::XMMATRIX;
#endif

struct ConversionParametersStruct
{
    // Normalized - 1.0f = 80nits
    float DisplayLuminanceLevel;
    uint OETFType;
    float PowerFunctionCharacteristic;
    uint pad;
    float4x4 ColorTransform;
};

#ifdef __cplusplus
    
}
#endif

#endif
