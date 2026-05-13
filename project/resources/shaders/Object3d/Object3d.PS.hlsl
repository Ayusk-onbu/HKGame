#include "Object3d.hlsli"


struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t environmentCoefficient;
};
ConstantBuffer<Material> gMaterial : register(b0);

struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの向き
    float32_t intensity; // 輝度
    int32_t shadowType; // シャドウの種類
};
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);


struct Camera
{
    float32_t3 worldPosition;
};
ConstantBuffer<Camera> gCamera : register(b2);

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
};

struct MultiPointLightData
{
    uint numLights;
    float32_t3 padding;
    PointLight lights[100];
};

ConstantBuffer<MultiPointLightData> gMultiPointLight : register(b3);

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float32_t3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
};

struct MultiSpotLightData
{
    uint numLights;
    float32_t3 padding;
    SpotLight lights[100];
};

ConstantBuffer<MultiSpotLightData> gMultiSpotLight : register(b4);

struct AreaLight
{
    float32_t4 color;
    float32_t4 points[4];
    float32_t intensity;
    int32_t twoSided;
};

ConstantBuffer<AreaLight> gAreaLight : register(b5);

struct PixelShaderOutPut
{
    float32_t4 color : SV_TARGET0;
};
Texture2D<float32_t4> gTexture : register(t0);

// LTC(t1),(t2)
Texture2D<float32_t4> gLTC1 : register(t1);
Texture2D<float32_t4> gLTC2 : register(t2);
TextureCube<float32_t4> gEnvironmentTexture : register(t3);

SamplerState gSampler : register(s0);

// Suport Function
float32_t IntegrateEdge(float32_t3 v1, float32_t3 v2, float32_t3 N)
{
    float32_t x = dot(v1, v2);
    float32_t y = abs(x);
    float32_t a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float32_t b = 3.4175940 + (4.161724 + y) * y;
    float32_t v = a / b;
    float32_t theta_sintheta = (x > 0.0) ? v : 0.5 * rsqrt(max(1.0 - x * x, 1e-7)) - v;
    return dot(cross(v1, v2) * theta_sintheta, N);
}

float32_t3 LTC_Evaluate(float32_t3 N, float32_t3 V, float32_t3 P, float32_t3x3 Minv, float32_t4 points[4], bool twoSided)
{
    // Create a local frame from normal N
    float32_t3 T1 = normalize(V - N * dot(V, N));
    float32_t3 T2 = cross(N, T1);
    
    // Rotate the inverse matrix into tangent space
    // Transform Minv from tangent space to world space orientation
    float32_t3x3 R = float32_t3x3(T1, T2, N);
    Minv = mul(Minv, R);

    // Map the four vertices of the light into the cosine-distributed space
    float32_t3 L[4];
    L[0] = mul(Minv, (points[0].xyz - P));
    L[1] = mul(Minv, (points[1].xyz - P));
    L[2] = mul(Minv, (points[2].xyz - P));
    L[3] = mul(Minv, (points[3].xyz - P));

    // Normalize to the unit sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    // Apply Stokes' theorem to sum the integrals along the four edges
    float32_t sum = 0.0;
    // Normal vector is (0,0,1) in the local frame
    sum += IntegrateEdge(L[0], L[1], float32_t3(0, 0, 1));
    sum += IntegrateEdge(L[1], L[2], float32_t3(0, 0, 1));
    sum += IntegrateEdge(L[2], L[3], float32_t3(0, 0, 1));
    sum += IntegrateEdge(L[3], L[0], float32_t3(0, 0, 1));
    if (twoSided)
    {
        sum = abs(sum); // 両面の場合は絶対値を取る
    }
    else
    {
        sum = max(0.0, sum); // 片面の場合は0でクランプ
    }

    // Return the result
    return float32_t3(sum, sum, sum);
}

PixelShaderOutPut main(VertexShaderOutput input)
{
    PixelShaderOutPut output;
    float4 transformUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformUV.xy);
    
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    // Directional Light
    float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
    float NDotH = dot(normalize(input.normal), halfVector);
    float specularDirectionalLightPow = pow(saturate(NDotH), gMaterial.shininess);
    
    if (gMaterial.enableLighting != 0)
    {
        float cos = 1.0f;
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        // 
        float32_t3 diffuseDirectionalLight =
        gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        //
        float32_t3 specularDirectionalLight =
        gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularDirectionalLightPow * float32_t3(1.0f, 1.0f, 1.0f);
       
        
        // Point Light
        // [ Total ] 
        float32_t3 totalPointDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t3 totalPointSpecular = float32_t3(0.0f, 0.0f, 0.0f);
        
        // [ Calculate ]
        for (uint32_t i = 0; i < gMultiPointLight.numLights; ++i)
        {
            PointLight light = gMultiPointLight.lights[i];
        
            float32_t distance = length(light.position - input.worldPosition);
            float32_t factor = pow(saturate(-distance / light.radius + 1.0f), light.decay);
    
            float32_t3 pointLightDirection = normalize(input.worldPosition - light.position);
            float32_t3 halfVectorPoint = normalize(-pointLightDirection + toEye);
            float NDotHPoint = dot(normalize(input.normal), halfVectorPoint);
            float specularPointLightPow = pow(saturate(NDotHPoint), gMaterial.shininess);
    
            totalPointDiffuse +=
            gMaterial.color.rgb * textureColor.rgb * light.color.rgb * 1.0f/*cos*/ * light.intensity * factor;
        
            totalPointSpecular +=
            light.color.rgb * light.intensity * specularPointLightPow * float32_t3(1.0f, 1.0f, 1.0f) * factor;
        }
        
        // Spot Light
        // [ Total ]
        float32_t3 totalSpotDiffuse = float32_t3(0.0f, 0.0f, 0.0f);
        float32_t3 totalSpotSpecular = float32_t3(0.0f, 0.0f, 0.0f);
        
        for (uint32_t i = 0; i < gMultiSpotLight.numLights; ++i)
        {
            SpotLight light = gMultiSpotLight.lights[i];
            
            float32_t attenuationDistance = length(light.position - input.worldPosition);
            float32_t attenuationFactor = pow(saturate(-attenuationDistance / light.distance + 1.0f), light.decay);
    
            float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - light.position);
            float32_t3 halfVectorSpot = normalize(-spotLightDirectionOnSurface + toEye);
            float NDotHSpot = dot(normalize(input.normal), halfVectorSpot);
            float specularSpotLightPow = pow(saturate(NDotHSpot), gMaterial.shininess);
    
            float32_t cosAngle = dot(spotLightDirectionOnSurface, light.direction);
            float32_t falloffFactor = saturate((cosAngle - light.cosAngle) / (light.cosFalloffStart - light.cosAngle));
            
            totalSpotDiffuse +=
            gMaterial.color.rgb * textureColor.rgb * light.color.rgb * cos * light.intensity * attenuationFactor * falloffFactor;
            
            totalSpotSpecular +=
            light.color.rgb * light.intensity * specularSpotLightPow * float32_t3(1.0f, 1.0f, 1.0f) * attenuationFactor * falloffFactor;
        }
        
        // *** New Code ***
        float32_t3 N = normalize(input.normal);
        float32_t3 V = normalize(gCamera.worldPosition - input.worldPosition);
        
        // Get LTC by UV Math
        float32_t dotNV = saturate(dot(N, V));
        float32_t roughness = 1.0 - (gMaterial.shininess / 100.0);
        float32_t2 uv = float32_t2(roughness, sqrt(1.0 - dotNV));
        uv = uv * ((64.0 - 1.0) / 64.0) + (0.5 / 64.0);
        
        // Get Matrix from LTC Texture
        float32_t4 t1 = gLTC1.Sample(gSampler, uv);
        float32_t4 t2 = gLTC2.Sample(gSampler, uv);
        float32_t3x3 Minv = float32_t3x3(
            float32_t3(t1.x, 0, t1.y),
            float32_t3(0, 1, 0),
            float32_t3(t1.z, 0, t1.w)
        );
        float32_t specularFresnel = t2.x * gMaterial.color.r + (1.0 - t2.x) * t2.y;
        float32_t3 specular = LTC_Evaluate(N, V, input.worldPosition, Minv, gAreaLight.points, gAreaLight.twoSided != 0);
        specular *= specularFresnel;
        float32_t3x3 identity = float32_t3x3(1, 0, 0, 0, 1, 0, 0, 0, 1);
        float32_t3 diffuse = LTC_Evaluate(N, V, input.worldPosition, identity, gAreaLight.points, gAreaLight.twoSided != 0);
        float32_t3 areaLightResult = (specular + diffuse * textureColor.rgb) * gAreaLight.color.rgb * gAreaLight.intensity;
        
        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + totalPointDiffuse + totalPointSpecular + totalSpotDiffuse + totalSpotSpecular + areaLightResult;
        
        float32_t3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float32_t3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
        float32_t4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
        
        output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
        
        output.color.a = gMaterial.color.a * textureColor.a;
    }else{
        output.color = gMaterial.color * textureColor;
    }
        return output;
}