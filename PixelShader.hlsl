#include "Header.hlsli"

float2x2 rot2D(float t)
{
    return float2x2(cos(t), -sin(t), sin(t), cos(t));
}
float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.) / k;
    return min(a, b) - h * h * h * k * (1. / 6.);
}
float sdCircle(float2 p, float r)
{
    return length(p) - r;
}
float sdRect(in float2 p, in float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}
float sdMoon(float2 p, float d, float ra, float rb)
{
    p.y = abs(p.y);
    float a = (ra * ra - rb * rb + d * d) / (2.0 * d);
    float b = sqrt(max(ra * ra - a * a, 0.0));
    if (d * (p.x * b - p.y * a) > d * d * max(b - p.y, 0.0))
        return length(p - float2(a, b));
    return max((length(p) - ra), -(length(p - float2(d, 0)) - rb));
}
float4 main(float4 i_pos : SV_POSITION, float2 i_uv : TEXCOORD) : SV_TARGET
{
    float2 p = (i_uv * 2 - iResolution.xy) / iResolution.y; //デカルト座標系に変換
    float time = iTime * 0.4;
    
    float2 p_ = p;
    p_.x += sin(time) * 0.5;
    float circle = sdCircle(p_, 0.2);
    
    p_ = p;
    p_.x += 0.8;
    p_ = mul(p_, rot2D(-time));
    float rect = sdRect(p_, float2(0.2, 0.2));
    
    p_ = p;
    p_.x -= 0.8;
    p_ = mul(p_, rot2D(time));
    float moon = sdMoon(p_, 0.2, 0.2, 0.2);
    
    float d = smin(moon, smin(rect, circle, 0.5), 0.5);
    
    float3 color = float3(1, 1, 1);
    color *= exp(-20 * abs(d));
    color *= smoothstep(0.3, 0.6, abs(cos(d * 100)));
    return float4(color, 1);
}
