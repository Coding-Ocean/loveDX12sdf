#include "Header.hlsli"

float4 main(float4 i_pos : SV_POSITION, float2 i_uv : TEXCOORD) : SV_TARGET
{
    float2 p = (i_uv * 2 - iResolution.xy) / iResolution.y; //�f�J���g���W�n�ɕϊ�

    float3 color = float3(1, 1, 1);

    return float4(color, 1);
}
