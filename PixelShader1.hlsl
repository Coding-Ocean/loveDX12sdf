#include<Header.hlsli>

float3 palette(float t)
{
    float3 a = float3(0.5, 0.5, 0.5);
    float3 b = float3(0.5, 0.5, 0.5);
    float3 c = float3(1, 1, 1);
    float3 d = float3(0.263, 0.416, 0.557);

    return a + b * cos(6.28318 * (c * t + d));
}

float4 main(float4 i_pos : SV_POSITION, float2 i_uv : TEXCOORD) : SV_TARGET
{
    float2 uv = (i_uv * 2 - iResolution.xy) / iResolution.y;
    float2 uv0 = uv;
    float3 finalCol = float3(0, 0, 0);
    for (int i = 0; i < 4; ++i)
    {
        uv *= 1.5;
        uv = uv - floor(uv);
        uv -= 0.5f;
    
        float d = length(uv) * exp(-length(uv0));
        float3 col = palette(length(uv0) + iTime * 0.2);
        d = sin(d * 8 + iTime * 0.2);
        d = abs(d);
        d = pow(0.04 / d, 2);
        finalCol += col * d;
    }
    return float4(finalCol, 1);
}

