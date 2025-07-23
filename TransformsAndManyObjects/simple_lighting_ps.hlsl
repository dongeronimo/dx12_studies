//shader outputs
struct PS_INPUT
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

//Per-Frame data for camera and light count
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

// Point Light structure
struct PointLightDataStruct
{
    float4 position; // xyz = position, w = unused/padding
    float attenuationConstant;
    float attenuationLinear;
    float attenuationQuadratic;
    float _padding1;
    float4 ColorDiffuse;
    float4 ColorSpecular;
    float4 ColorAmbient; // Ambient component of THIS light
};
StructuredBuffer<PointLightDataStruct> PointLights : register(t2); // Array of Point Light data

//a root constant (aka push constant), it's the index at PerObjectData
cbuffer RootConstants : register(b0)
{
    uint objectId; // Root constant passed by CPU
}
// Function to calculate the lighting contribution from a single point light
float3 CalculatePointLightContribution(
    float3 fragmentWorldPos,
    float3 fragmentNormalWS,
    float3 fragmentViewVecWS,
    PointLightDataStruct light)
{
    float4 g_hardcodedMaterialDiffuse = PerObjectData[objectId].diffuseColor; 
    float4 g_hardcodedMaterialSpecular = PerObjectData[objectId].specularColor;
    float g_hardcodedMaterialShininess = PerObjectData[objectId].shininess.x; 
    float4 g_hardcodedMaterialAmbient = PerObjectData[objectId].ambientColor; 
    float4 g_hardcodedMaterialEmissive = PerObjectData[objectId].emissiveColor; 
    
    // Normalize interpolated normal (crucial after interpolation)
    fragmentNormalWS = normalize(fragmentNormalWS);

    // Calculate vector from fragment TO the light source
    float3 lightVecWS = light.position.xyz - fragmentWorldPos;
    float lightDist = length(lightVecWS);
    lightVecWS = normalize(lightVecWS); // Normalize for dot products

    // Calculate attenuation based on distance
    float attenuation = 1.0f / (light.attenuationConstant +
                                light.attenuationLinear * lightDist +
                                light.attenuationQuadratic * (lightDist * lightDist));

    // --- Diffuse Component (Lambertian) ---
    //float val = 1.0 / 54351.41406f;
    float NdotL = max(0.0f, dot(fragmentNormalWS, lightVecWS));
    float3 diffuse = NdotL * light.ColorDiffuse.rgb * g_hardcodedMaterialDiffuse.rgb;

    // --- Specular Component (Blinn-Phong) ---
    float3 halfwayVec = normalize(lightVecWS + fragmentViewVecWS);
    float NdotH = max(0.0f, dot(fragmentNormalWS, halfwayVec));
    float specularFactor = pow(NdotH, g_hardcodedMaterialShininess);
    float3 specular = specularFactor * light.ColorSpecular.rgb * g_hardcodedMaterialSpecular.rgb;

    // Combine and Apply Attenuation
    return (diffuse + specular) * attenuation;
}

// Simple Reinhard tone mapping
float3 ReinhardToneMapping(float3 hdrColor, float exposure)
{
    return (hdrColor * exposure) / (hdrColor * exposure + 1.0);
}

// sRGB gamma correction
float3 ApplySRGBGamma(float3 linearColor)
{
    return pow(linearColor, 1.0 / 2.2);
}

// Pixel Shader entry point
float4 main(PS_INPUT input) : SV_TARGET
{
    float4 g_hardcodedMaterialDiffuse = PerObjectData[objectId].diffuseColor;
    float4 g_hardcodedMaterialSpecular = PerObjectData[objectId].specularColor;
    float g_hardcodedMaterialShininess = PerObjectData[objectId].shininess.x;
    float4 g_hardcodedMaterialAmbient = PerObjectData[objectId].ambientColor;
    float4 g_hardcodedMaterialEmissive = PerObjectData[objectId].emissiveColor;
    float exposure = PerFrameData[0].exposure.x;
    // Get per-frame data
    PerFrameDataStruct perFrame = PerFrameData[0];

    // Normalize interpolated vectors (crucial for correct lighting)
    float3 pixelNormalWS = normalize(input.normalWS);
    float3 pixelViewVecWS = normalize(input.viewVecWS);

    // Start with global ambient light (multiplied by material's ambient reflection)
    float3 finalLightColor = g_hardcodedMaterialAmbient.rgb;

    // Add emissive color
    finalLightColor += g_hardcodedMaterialEmissive.rgb;

    // Loop through all active point lights and sum their contributions
    for (int i = 0; i < perFrame.numberOfPointLights; ++i)
    {
        PointLightDataStruct currentLight = PointLights[i];
        finalLightColor += CalculatePointLightContribution(
            input.worldPos,
            pixelNormalWS,
            pixelViewVecWS,
            currentLight);

        // Optionally, if lights have their own ambient component, you can sum them here.
        // This is less common as global ambient is usually enough.
        // finalLightColor += currentLight.ColorAmbient.rgb * g_hardcodedMaterialAmbient.rgb;
    }

    
    float3 toneMappedColor = ReinhardToneMapping(finalLightColor, exposure);
    float3 gammaCorrectedColor = ApplySRGBGamma(toneMappedColor);
    return float4(gammaCorrectedColor, 1.0);
}