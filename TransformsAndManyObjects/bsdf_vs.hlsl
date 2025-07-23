//vertex inputs
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
    float3 tangent : TAN; //for BSDF
    float3 cotangent : COTAN; //for BSDF
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float3 viewDir : VIEW_DIR;
};
//Per object data
//describes the model matrix
struct PerObjectDataStruct
{
    float4x4 modelMat;
    float4x4 inverseTransposeModelMat; // transpose(inverse(worldMatrix))
    //material data for BSDF
    float4 baseColor;
    float metallicFactor;
    float roughnessFactor;
    float opacity;
    float refracti;
    float4 emissiveColor;
};

struct PerFrameDataStruct
{
    float4x4 viewProjMatrix;
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float3 cameraPosition;
    int numberOfPointLights;
    float4 exposure;
};

struct PointLightsDataStruct
{
    float4 position;
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
    float _notUsed; //for alignment
    float4 ColorDiffuse;
    float4 ColorSpecular;
    float4 ColorAmbient;
};

StructuredBuffer<PerObjectDataStruct> PerObjectData : register(t0);
StructuredBuffer<PerFrameDataStruct> PerFrameData : register(t1);
StructuredBuffer<PointLightsDataStruct> PointLights : register(t2); 
//a root constant (aka push constant), it's the index at PerObjectData
cbuffer RootConstants : register(b0)
{
    uint objectId; // Root constant passed by CPU
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    PerFrameDataStruct perFrame = PerFrameData[0];
    float4x4 worldMatrix = PerObjectData[objectId].modelMat;
    float4x4 worldViewProjMatrix = mul(worldMatrix, perFrame.viewProjMatrix);
    float4x4 invTransModelMatrix = PerObjectData[objectId].inverseTransposeModelMat;
    
    // Transform position to world space
    float4 worldPos = mul(float4(input.pos, 1.0f), worldMatrix);
    output.worldPos = worldPos.xyz;
    
    // Transform to clip space
    output.position = mul(float4(input.pos, 1.0f), worldViewProjMatrix);
    
    // Transform normal to world space
    output.normal = normalize(mul(input.normal, (float3x3) invTransModelMatrix));
    
    // Transform tangent to world space
    output.tangent = normalize(mul(input.tangent, (float3x3) worldMatrix));
    
    // Calculate bitangent
    output.bitangent = normalize(cross(output.normal, output.tangent));
    
    // Pass through texture coordinates
    output.texCoord = input.uv;
    
    // Calculate view direction
    output.viewDir = normalize(perFrame.cameraPosition - output.worldPos);
    
    return output;
}