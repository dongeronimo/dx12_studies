//vertex inputs
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
};

//shader outputs
struct VS_OUTPUT
{
    float4 positionCS : SV_POSITION; // Vertex position in clip space
    float3 worldPos : TEXCOORD0; // Vertex position in world space
    float3 normalWS : TEXCOORD1; // Vertex normal in world space
    float3 viewVecWS : TEXCOORD2; // Vector from vertex to camera in world space
    float2 uv : TEXCOORD3; // Pass UV to pixel shader
};

//describes the model matrix
struct PerObjectDataStruct
{
    float4x4 modelMat;
    float4x4 inverseTransposeModelMat;
    //material data.
    float4 diffuseColor;
    float4 specularColor;
    float4 ambientColor;
    float4 emissiveColor;
    float4 shininess;
};

//where i store the model matrices
StructuredBuffer<PerObjectDataStruct> PerObjectData : register(t0);

//a root constant (aka push constant), it's the index at PerObjectData
cbuffer RootConstants : register(b0)
{
    uint objectId; // Root constant passed by CPU
}

//Per-Frame data for camera
struct PerFrameDataStruct
{
    float4x4 viewProjMatrix;
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float3 cameraPosition;
    int numberOfPointLights;
    float4 exposure;
};

StructuredBuffer<PerFrameDataStruct> PerFrameData : register(t1);

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

// Array of Point Light data
StructuredBuffer<PointLightsDataStruct> PointLights : register(t2); // Assuming t2 for lights

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    PerFrameDataStruct perFrame = PerFrameData[0];
    float4x4 modelMatrix = PerObjectData[objectId].modelMat;
    float4x4 invTransModelMatrix = PerObjectData[objectId].inverseTransposeModelMat;
    
    //position from model space to world space
    output.worldPos = mul(float4(input.pos, 1.0f), modelMatrix).xyz;
    
    //normal to world space (CORRECTED: removed extra transpose)
    output.normalWS = normalize(mul(input.normal, (float3x3) invTransModelMatrix));
    
    // Calculate the vector from the vertex to the camera (viewer) in world space
    output.viewVecWS = normalize(perFrame.cameraPosition - output.worldPos);
    
    // Transform position to clip space (CORRECTED: proper matrix multiplication order)
    float4 worldPos4 = mul(float4(input.pos, 1.0f), modelMatrix); // Model to World
    output.positionCS = mul(worldPos4, perFrame.viewProjMatrix); // World to Clip
    
    // Pass UVs directly
    output.uv = input.uv;
    
    return output;
}