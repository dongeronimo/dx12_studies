// Input/Output structures
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

// Per-object data structure containing transformation matrices and material properties
struct PerObjectDataStruct
{
    float4x4 modelMat;
    float4x4 inverseTransposeModelMat; // transpose(inverse(worldMatrix)) for normal transformation
    // Material data for physically-based BSDF calculations
    float4 baseColor;
    float metallicFactor;
    float roughnessFactor;
    float opacity;
    float refracti;
    float4 emissiveColor;
};

// Per-frame data structure containing camera and rendering parameters
struct PerFrameDataStruct
{
    float4x4 viewProjMatrix;
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float3 cameraPosition;
    int numberOfPointLights;
    float4 exposure; // Exposure value for tone mapping
};

// Point light data structure with position, attenuation, and color properties
struct PointLightsDataStruct
{
    float4 position; // xyz = position, w unused
    float attenuationConstant; // Constant attenuation factor
    float attenuationLinear; // Linear attenuation factor
    float attenuationQuadratic; // Quadratic attenuation factor
    float _notUsed; // Padding for 16-byte alignment
    float4 ColorDiffuse; // Diffuse light color and intensity
    float4 ColorSpecular; // Specular light color and intensity
    float4 ColorAmbient; // Ambient light color and intensity
    
    // point light shadow mapping data
    float4x4 projectionMatrix; // Same projection matrix used for all 6 faces
    float shadowFarPlane; // Far plane distance for linearizing depth
    int shadowMapIndex; // Index into the shadow map array
    float2 _notUsed2;
};

// Structured buffer resources containing arrays of data for all objects/lights
StructuredBuffer<PerObjectDataStruct> PerObjectData : register(t0);
StructuredBuffer<PerFrameDataStruct> PerFrameData : register(t1);
StructuredBuffer<PointLightsDataStruct> PointLights : register(t2);

TextureCubeArray ShadowMaps : register(t3); // Array of cube maps (one per light)
SamplerState ShadowSampler : register(s0); // Sampler for shadow maps

// Root constant containing the index to access current object data
cbuffer RootConstants : register(b0)
{
    uint objectId; // Index into PerObjectData array for current object
}

// Physical constants for BRDF calculations
static const float PI = 3.14159265359f;
static const float MIN_ROUGHNESS = 0.04f; // Minimum roughness to prevent numerical issues
static const float EPSILON = 0.001f; // Small value to prevent division by zero

float CalculateVarianceShadow(float3 worldPos, PointLightsDataStruct light, int lightIndex)
{
    // Calculate vector from fragment to light
    float3 fragToLight = worldPos - light.position.xyz;
    float currentDepth = length(fragToLight) / light.shadowFarPlane; // Normalize to [0,1]
    
    // Sample the cube map using the direction vector
    float4 shadowData = ShadowMaps.Sample(ShadowSampler, float4(fragToLight, (float) lightIndex));
    
    // Extract variance shadow map data
    float storedDepth = shadowData.r; // First moment (mean)
    float storedDepthSquared = shadowData.g; // Second moment (mean squared)
    
    // Basic shadow test
    if (currentDepth <= storedDepth)
        return 1.0; // Not in shadow
    
    // Variance shadow mapping calculation
    float variance = storedDepthSquared - (storedDepth * storedDepth);
    variance = max(variance, 0.0001); // Avoid division by zero
    
    float d = currentDepth - storedDepth;
    float pMax = variance / (variance + d * d);
    
    return pMax;
}
/**
 * Calculates Fresnel reflectance using Schlick's approximation
 * @param cosTheta: Cosine of angle between view direction and half vector
 * @param F0: Base reflectivity at normal incidence
 * @return: Fresnel reflectance value
 */
float3 CalculateFresnelReflectance(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

/**
 * Calculates normal distribution function using GGX/Trowbridge-Reitz distribution
 * This determines how microfacets are oriented relative to the surface normal
 * @param N: Surface normal
 * @param H: Half vector between view and light directions
 * @param roughness: Surface roughness parameter
 * @return: Distribution term for BRDF
 */
float CalculateGGXDistribution(float3 N, float3 H, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    
    float numerator = alpha2;
    float denominator = (NdotH2 * (alpha2 - 1.0f) + 1.0f);
    denominator = PI * denominator * denominator;
    
    return numerator / max(denominator, EPSILON);
}

/**
 * Calculates geometry masking function using Schlick-GGX approximation
 * This accounts for microfacets shadowing each other
 * @param NdotV: Dot product of normal and view direction
 * @param roughness: Surface roughness parameter
 * @return: Geometry masking term
 */
float CalculateGeometryMasking(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f; // Direct lighting version of k
    
    float numerator = NdotV;
    float denominator = NdotV * (1.0f - k) + k;
    
    return numerator / max(denominator, EPSILON);
}

/**
 * Calculates Smith geometry function combining masking and shadowing
 * @param N: Surface normal
 * @param V: View direction
 * @param L: Light direction
 * @param roughness: Surface roughness parameter
 * @return: Combined geometry term for BRDF
 */
float CalculateSmithGeometry(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float geometryView = CalculateGeometryMasking(NdotV, roughness);
    float geometryLight = CalculateGeometryMasking(NdotL, roughness);
    
    return geometryView * geometryLight;
}

/**
 * Calculates point light attenuation based on distance
 * @param lightData: Point light data containing attenuation coefficients
 * @param distance: Distance from fragment to light
 * @return: Attenuation factor (0-1)
 */
float CalculatePointLightAttenuation(PointLightsDataStruct lightData, float distance)
{
    return 1.0f / (lightData.attenuationConstant +
                   lightData.attenuationLinear * distance +
                   lightData.attenuationQuadratic * distance * distance);
}

/**
 * Implements physically-based Cook-Torrance BRDF model
 * This is the core of the Principled BSDF, combining diffuse and specular terms
 * @param N: Surface normal
 * @param V: View direction (fragment to camera)
 * @param L: Light direction (fragment to light)
 * @param albedo: Base color of the material
 * @param metallic: Metallic factor (0 = dielectric, 1 = metallic)
 * @param roughness: Surface roughness (0 = mirror, 1 = completely rough)
 * @return: BRDF value for the given light-view configuration
 */
float3 EvaluateCookTorranceBRDF(float3 N, float3 V, float3 L, float3 albedo, float metallic, float roughness)
{
    float3 H = normalize(V + L); // Half vector between view and light
    
    // Calculate dot products needed for BRDF evaluation
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float HdotV = max(dot(H, V), 0.0f);
    
    // Calculate base reflectivity (F0) - lerp between dielectric and metallic values
    // Dielectric materials have low F0 (~0.04), metals use their albedo as F0
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    
    // Evaluate the three terms of Cook-Torrance BRDF
    float D = CalculateGGXDistribution(N, H, roughness); // Normal Distribution Function
    float G = CalculateSmithGeometry(N, V, L, roughness); // Geometry Function
    float3 F = CalculateFresnelReflectance(HdotV, F0); // Fresnel Term
    
    // Calculate energy conservation between specular and diffuse reflection
    float3 kS = F; // Specular contribution
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS; // Diffuse contribution
    kD *= 1.0f - metallic; // Metals have no diffuse reflection
    
    // Combine specular terms: D * G * F / (4 * NdotV * NdotL)
    float3 numerator = D * G * F;
    float denominator = 4.0f * NdotV * NdotL + EPSILON;
    float3 specular = numerator / denominator;
    
    // Add Lambertian diffuse reflection
    float3 diffuse = kD * albedo / PI;
    
    // Return combined BRDF weighted by light incidence angle
    return (diffuse + specular) * NdotL;
}

/**
 * Applies exposure-based tone mapping to convert HDR to LDR
 * @param hdrColor: High dynamic range color input
 * @param exposure: Exposure adjustment value
 * @return: Tone-mapped LDR color
 */
float3 ApplyExposureToneMapping(float3 hdrColor, float exposure)
{
    // Apply exposure adjustment
    float3 exposedColor = hdrColor * exposure;
    
    // ACES tone mapping - better for handling very bright values
    // ACES constants
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    
    exposedColor = saturate((exposedColor * (a * exposedColor + b)) / (exposedColor * (c * exposedColor + d) + e));
    
    return exposedColor;
}

/**
 * Alternative simpler tone mapping with luminance-based scaling
 * Use this if ACES is too aggressive
 */
float3 ApplyReinhardExtendedToneMapping(float3 hdrColor, float exposure)
{
    float3 exposedColor = hdrColor * exposure;
    
    // Extended Reinhard tone mapping with white point
    float whitePoint = 4.0f; // Adjust this to control where white is mapped
    float3 numerator = exposedColor * (1.0f + (exposedColor / (whitePoint * whitePoint)));
    return numerator / (1.0f + exposedColor);
}
/**
 * Main pixel shader entry point
 * Implements Principled BSDF shading with multiple point lights
 */
float4 main(VS_OUTPUT input) : SV_TARGET
{
    // Retrieve current object and frame data using the object ID
    PerObjectDataStruct currentObject = PerObjectData[objectId];
    PerFrameDataStruct frameData = PerFrameData[0]; // Assuming single frame data
    
    // Normalize interpolated vertex attributes
    float3 N = normalize(input.normal); // Surface normal
    float3 V = normalize(input.viewDir); // View direction (fragment to camera)
    
    // Extract material properties from current object data
    float3 albedo = currentObject.baseColor.rgb;
    float metallic = currentObject.metallicFactor;
    float roughness = max(currentObject.roughnessFactor, MIN_ROUGHNESS);
    float opacity = currentObject.opacity;
    float3 emissive = currentObject.emissiveColor.rgb;
    
    // Initialize accumulated lighting color
    float3 finalColor = float3(0.0f, 0.0f, 0.0f);
    
    // Process all point lights in the scene
    for (int lightIndex = 0; lightIndex < frameData.numberOfPointLights; lightIndex++)
    {
        PointLightsDataStruct currentLight = PointLights[lightIndex];
        
        // Calculate light direction and distance
        float3 lightWorldPos = currentLight.position.xyz;
        float3 lightVector = lightWorldPos - input.worldPos;
        float lightDistance = length(lightVector);
        float3 L = lightVector / lightDistance; // Normalized light direction
        
        // Calculate light attenuation based on distance
        float attenuation = CalculatePointLightAttenuation(currentLight, lightDistance);
        // Calculate shadow factor
        float shadowFactor = CalculateVarianceShadow(input.worldPos, currentLight, currentLight.shadowMapIndex);
    
        // Skip light contribution if attenuation is negligible
        if (attenuation < 0.001f)
            continue;
        
        // Evaluate BRDF for current light
        float3 brdfValue = EvaluateCookTorranceBRDF(N, V, L, albedo, metallic, roughness);
        
        // Combine diffuse and specular light colors (assuming they're the same for simplicity)
        float3 lightColor = currentLight.ColorDiffuse.rgb;
        float lightIntensity = currentLight.ColorDiffuse.a;
        
        // Accumulate light contribution with attenuation
        finalColor += brdfValue * lightColor * lightIntensity * attenuation * shadowFactor;
        
        //// Add ambient contribution (only once, not per light)
        //if (lightIndex == 0)
        //{
        //    float3 ambientColor = currentLight.ColorAmbient.rgb;
        //    float ambientIntensity = currentLight.ColorAmbient.a;
        //    finalColor += albedo * ambientColor * ambientIntensity;
        //}
    }
    
    // Add emissive contribution (self-illumination)
    finalColor += emissive;
    
    // Apply tone mapping using exposure parameter
    float exposure = frameData.exposure.x; // Use first component of exposure vector
    finalColor = ApplyExposureToneMapping(finalColor, exposure);
    
    // Apply gamma correction for sRGB display
    finalColor = pow(finalColor, 1.0f / 2.2f);
    
    // Return final color with material opacity
    return float4(finalColor, opacity * currentObject.baseColor.a);
}