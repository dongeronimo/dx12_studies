// Vertex Shader
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// Pixel Shader
Texture2D<float4> sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

float4 main(VSOutput input) : SV_TARGET
{
    // Sample the offscreen buffer
    float4 color = sceneTexture.Sample(sceneSampler, input.texCoord);
    
    // You can add post-processing effects here if needed
    // For example: tone mapping, gamma correction, etc.
    
    return color;
}