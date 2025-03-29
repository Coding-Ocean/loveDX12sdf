#include "Header.hlsli"

#define MAX_STEPS 50
#define MAX_DIST 90.0
#define SURF_DIST 0.001

float2x2 rot2D(float t)
{
    return float2x2(cos(t), -sin(t), sin(t), cos(t));
}
float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.) / k;
    return min(a, b) - h * h * h * k * (1. / 6.);
}
float sdSphere(float3 p, float s)
{
    return length(p) - s;
}
float sdBox(float3 p, float3 b)
{
    float3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdBoxFrame(float3 p, float3 b, float e)
{
    p = abs(p) - b;
    float3 q = abs(p + e) - e;
    return min(min(
      length(max(float3(p.x, q.y, q.z), 0.0)) + min(max(p.x, max(q.y, q.z)), 0.0),
      length(max(float3(q.x, p.y, q.z), 0.0)) + min(max(q.x, max(p.y, q.z)), 0.0)),
      length(max(float3(q.x, q.y, p.z), 0.0)) + min(max(q.x, max(q.y, p.z)), 0.0));
}
//get distance from ray position to nearest surface
float getDistFrom(float3 rp)
{
    //sphere
    float3 sp = float3(sin(iTime * 0.3) * -2.0, 1.5, 0.0);//sphere position
    float sphere = sdSphere(rp - sp, 1.0);
    
    //box
    float3 rp_ = rp;//copy
    rp_ -= float3(-3, 1, 0);
    rp_.xz = mul(rp_.xz, rot2D(iTime));
    rp_.xy = mul(rp_.xy, rot2D(iTime));
    float box = sdBox(rp_, 0.6);
    
    //frame
    rp_ = rp; //copy
    rp_ -= float3(3, 1, 0);
    rp_.xz = mul(rp_.xz, rot2D(iTime));
    rp_.xy = mul(rp_.xy, rot2D(iTime));
    float frame = sdBoxFrame(rp_, 0.6, 0.03);

    //plane
    float plane = rp.y + 1.;
    
    return min(plane, smin(frame, smin(sphere, box, 1.0), 1.0));
}
float rayMarch(float3 ro, float3 rd)
{
    float t = 0.0; //distance traveled
    for (int i = 0; i < MAX_STEPS; i++)
    {
        float3 rp = ro + rd * t;//ray position
        float d = getDistFrom(rp);//get distance from ray position to nearest surface
        t += d;
        if (t > MAX_DIST || d < SURF_DIST)
            break;
    }
    return t;
}
float3 getNormal(float3 rp)
{
    float d = getDistFrom(rp);
    float2 sft = float2(0.001, 0); //shift value
    float3 n = d - float3(
        getDistFrom(rp - sft.xyy), //rp-float3(0.001,0,0)
        getDistFrom(rp - sft.yxy), //rp-float3(0,0.001,0)
        getDistFrom(rp - sft.yyx) //rp-float3(0,0,0.001)
    );
    return normalize(n);
}
float lighting(float3 rp)
{
    float3 lp = float3(0, 6, -3);//light position
    float3 lv = lp - rp;//light vector
    float len = length(lv);
    lv /= len; //normalize
    float3 nv = getNormal(rp);
    float brightness = dot(nv, lv);
    
    //影
    float t = rayMarch(rp + nv * 0.002, lv);//現在のレイ位置からライト方向にレイを飛ばす
    if (t < len)
        brightness *= .3; //影なので暗くする
    
    return brightness;
}
float4 main(float4 i_pos : SV_POSITION, float2 i_uv : TEXCOORD) : SV_TARGET
{
    float2 p = (i_uv * 2 - iResolution.xy) / iResolution.y;

    float3 ro = float3(0, 1, -5);//ray origin
    float3 rd = normalize(float3(p, 1.));//ray direction
    

    float t = rayMarch(ro, rd);//distance traveled
    
    float3 col = float3(1., 1., 1.); //final color
    //col *= 1 - t / 8; //tの値視覚化
    //return float4(col, 1.);
    
    float3 rp = ro + rd * t;//ray position
    col *= lighting(rp);
    return float4(col, 1);
}
