#include "Header.hlsli"

float4 main(float4 i_pos : SV_POSITION, float2 i_uv : TEXCOORD) : SV_TARGET
{
    float2 p = i_uv / iResolution.y;

    float3 color = float3(p.x, 0, 0);

    return float4(color, 1);
}
