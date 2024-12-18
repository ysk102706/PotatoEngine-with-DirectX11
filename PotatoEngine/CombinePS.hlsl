#include "Shader.hlsli" 
#include "ToneMapping.hlsli"

Texture2D OriginTex : register(t0);
Texture2D BloomTex : register(t1);

struct PostProcessPSInput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD;
}; 

float4 main(PostProcessPSInput input) : SV_TARGET
{
    float3 origin = OriginTex.Sample(linearClampSS, input.texcoord); 
    float3 bloom = BloomTex.Sample(linearClampSS, input.texcoord); 
    
    float3 combine = origin + bloom * bloomStrength; 
    
    float3 tone = useToneMapping ? LinearToneMapping(combine, exposure, gamma) : combine; 
     
    return float4(tone, 1.0);
}