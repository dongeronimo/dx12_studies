//inputs
struct VS_INPUT
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : UV;
};
//outputs
struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};
//describes the model matrix
struct PerObjectDataStruct
{
    float4x4 modelMat;
};
//where i store the model matrices
StructuredBuffer<PerObjectDataStruct> PerObjectData: register(t0);
//a root constant (aka push constant), it's the index at PerObjectData
cbuffer RootConstants : register(b0)
{
    uint objectId; // Root constant passed by CPU
}

struct PerFrameDataStruct
{
    float4x4 viewProjectionMatrix;
};

StructuredBuffer<PerFrameDataStruct> PerFrameData : register(t1);

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 modelMatrix = PerObjectData[objectId].modelMat;
    float4x4 mvpMatrix = mul(modelMatrix, PerFrameData[0].viewProjectionMatrix);
    output.pos = mul(float4(input.pos, 1.0f), mvpMatrix);
    output.color = float4(input.uv, 1.0f, 1.0f);
    return output;
}