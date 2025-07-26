// Shadow map view/projection matrices
cbuffer ShadowMapConstants : register(b1)
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float3 lightPosition;
    float padding;
}

// Root constant with object ID
cbuffer RootConstants : register(b0)
{
    uint objectId;
}

// Per-object data structure
struct PerObjectDataStruct
{
    float4x4 modelMat;
    float4x4 inverseTransposeModelMat;
    float4 baseColor;
    float metallicFactor;
    float roughnessFactor;
    float opacity;
    float refracti;
    float4 emissiveColor;
};

// Structured buffer for per-object data
StructuredBuffer<PerObjectDataStruct> PerObjectData : register(t0);

// Vertex input structure
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
    float3 tangent : TAN;
    float3 cotangent : COTAN;
};

// Vertex output / Pixel input structure
struct VS_OUTPUT
{
    float4 position : SV_Position;
    float depth : DEPTH; // Linear depth for variance shadow mapping
    float3 worldPos : WORLD_POS; // World position (optional, for debugging)
};

// Vertex Shader
VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // Get per-object data using the root constant object ID
    PerObjectDataStruct objData = PerObjectData[objectId];
    
    // Transform vertex to world space
    float4 worldPos = mul(float4(input.pos, 1.0f), objData.modelMat);
    
    // Transform to light's view space
    float4 viewPos = mul(worldPos, viewMatrix);
    
    // Transform to light's clip space
    output.position = mul(viewPos, projMatrix);
    
    // Calculate linear depth for variance shadow mapping
    // We store the distance from light to fragment
    output.depth = length(worldPos.xyz - lightPosition);
    
    // Store world position (optional, for debugging)
    output.worldPos = worldPos.xyz;
    
    return output;
}