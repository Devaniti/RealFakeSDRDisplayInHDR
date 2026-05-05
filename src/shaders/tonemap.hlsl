#include "cpp_shared.hlsli"

#pragma warning(disable : 3571)

Texture2D<float4> SDRImage : register(t0);
Texture2D<float4> ImGUITarget : register(t1);

cbuffer PerObject : register(b0)
{
    ConversionParametersStruct ConversionParameters;
};

struct VSInput
{
    uint vertexID : SV_VertexID;
};

struct PSInput
{
    float4 position : SV_Position;
};

PSInput VSMain(VSInput input)
{
    PSInput output = (PSInput)0;

    static const float2 vertexBuffer[6] =
        {
            float2(-1.0f, -1.0f),
            float2(-1.0f, 1.0f),
            float2(1.0f, -1.0f),
            float2(1.0f, -1.0f),
            float2(-1.0f, 1.0f),
            float2(1.0f, 1.0f)};

    output.position = float4(vertexBuffer[input.vertexID], 0.5, 1.0f);

    return output;
}

float Power2_2OETF(float value)
{
    if (value < 0.0f)
        return 0.0f;

    return pow(value, ConversionParameters.PowerFunctionCharacteristic);
}

float sRGBInverseEOTF(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value < 0.04045f)
        return value / 12.92f;

    return pow((value + 0.055f) / 1.055f, 2.4f);
}

float ApplyOETF(float value)
{
    switch (ConversionParameters.OETFType)
    {
    case 0:
        return Power2_2OETF(value);
    case 1:
        return sRGBInverseEOTF(value);
    default:
        return 0.0f;
    }
}

float3 ApplyOETF(float3 value)
{
    return float3(ApplyOETF(value.x), ApplyOETF(value.y), ApplyOETF(value.z));
}

// Return color in CCCS color space
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 SDRData = SDRImage.Load(int3(input.position.xy, 0));
    float4 ImGUIData = ImGUITarget.Load(int3(input.position.xy, 0));
    float3 ComposedData = SDRData.rgb * (1 - ImGUIData.a) + ImGUIData.rgb * ImGUIData.a;

    float3 LinearColor = ApplyOETF(ComposedData);

	LinearColor = mul(ConversionParameters.ColorTransform, float4(LinearColor, 1.0f)).rgb;

    LinearColor *= ConversionParameters.DisplayLuminanceLevel;

    return float4(LinearColor, 0.0f);
}
