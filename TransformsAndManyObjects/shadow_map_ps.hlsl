// Vertex output / Pixel input structure
struct VS_OUTPUT
{
    float4 position : SV_Position;
    float depth : DEPTH; // Linear depth for variance shadow mapping
    float3 worldPos : WORLD_POS; // World position (optional, for debugging)
};

// Pixel Shader for Variance Shadow Mapping
float4 main(VS_OUTPUT input) : SV_Target
{
    // For variance shadow mapping, we store:
    // R: mean depth (first moment)
    // G: mean squared depth (second moment)  
    // B: unused (could store additional data)
    // A: unused
    
    float depth = input.depth;
    float depthSquared = depth * depth;
    
    return float4(depth, depthSquared, 0.0f, 1.0f);
}