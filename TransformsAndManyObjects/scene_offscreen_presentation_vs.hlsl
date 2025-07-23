// Vertex Shader
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    
    // Generate fullscreen triangle vertices using vertex ID
    // This technique generates a large triangle that covers the entire screen
    // More efficient than using a quad with 4 vertices
    float2 texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.texCoord = texCoord;
    output.position = float4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f);
    
    // Flip Y coordinate for DirectX
    output.position.y = -output.position.y;
    
    return output;
}