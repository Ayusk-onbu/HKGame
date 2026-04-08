#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};



//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
//    output.color = gTexture.Sample(gSampler, input.texcoord);
//    return output;
//}


//float GetBayer4x4(uint2 pixelPos)
//{
//    const uint dither[16] =
//    {
//        0, 8, 2, 10,
//        12, 4, 14, 6,
//         3, 11, 1, 9,
//        15, 7, 13, 5
//    };
//    return float(dither[(pixelPos.x % 4) + (pixelPos.y % 4) * 4]) * (1.0 / 16.0);
//}

//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
    
//    float4 rawColor = gTexture.Sample(gSampler, input.texcoord);
    
//    float luminance = dot(rawColor.rgb, float3(0.2126, 0.7152, 0.0722));
    
//    uint2 pixelPos = uint2(input.texcoord * float2(480, 270)); // 低解像度化
//    float threshold = GetBayer4x4(pixelPos);
    
//    float dithered = luminance > threshold ? 1.0 : 0.0;
    
//    float scanline = sin(input.texcoord.y * 800.0) * 0.1;
    
//    float3 lcdColor = float3(0.4, 0.5, 0.2); // 液晶のベース色
//    float3 finalRGB = lerp(lcdColor * 0.2, lcdColor, dithered) - scanline;
    
//    output.color = float4(finalRGB, rawColor.a);
//    return output;
//}


//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
    
//    float2 uv = input.texcoord;
//    float4 texColor = gTexture.Sample(gSampler, uv);
    
//    float2 delta = float2(1.0 / 1280.0, 1.0 / 720.0);
//    float4 n = gTexture.Sample(gSampler, uv + float2(0, delta.y));
//    float4 s = gTexture.Sample(gSampler, uv - float2(0, delta.y));
//    float4 e = gTexture.Sample(gSampler, uv + float2(delta.x, 0));
//    float4 w = gTexture.Sample(gSampler, uv - float2(delta.x, 0));
//    float edge = length(n + s + e + w - 4.0 * texColor);
    
//    //float2 gridUV = frac(uv * float2(20.0, 11.0));
//    //float grid = (gridUV.x < 0.02 || gridUV.y < 0.03) ? 0.2 : 0.0;
    
//    float scanline = smoothstep(0.9, 1.0, sin(uv.y * 5.0));
    
//    float luminance = dot(texColor.rgb, float3(0.2126, 0.7152, 0.0722));
//    float3 baseColor = float3(0.0, 0.4, 0.8) * luminance;
//    float3 highlight = float3(0.4, 0.9, 1.0) * edge * 5.0;
    
//    float3 finalColor = baseColor + highlight /* + grid*/;
//    finalColor += scanline * float3(0.0, 0.5, 1.0) * 0.3;
    
//    output.color = float4(finalColor, texColor.a);
//    return output;
//}


//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
//    float2 uv = input.texcoord;
    
//    // 中心からの距離に応じてズレを大きくする
//    float dist = distance(uv, float2(0.5, 0.5));
//    float shift = dist * 0.02; // ズレの強さ
    
//    // R, G, B を別々の位置からサンプリング
//    float r = gTexture.Sample(gSampler, uv + float2(shift, 0)).r;
//    float g = gTexture.Sample(gSampler, uv).g;
//    float b = gTexture.Sample(gSampler, uv - float2(shift, 0)).b;
    
//    output.color = float4(r, g, b, 1.0);
//    return output;
//}


//float GetPseudoTime(float2 uv)
//{
//    return frac(uv.x * 0.1 + uv.y * 0.05);
//}

//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
//    float2 uv = input.texcoord;
    
//    // --- 1. 疑似アニメーションのシード値 ---
//    // 本来の Time の代わりに、色の輝度や座標を混ぜて「常に揺らぐ」値を作ります
//    float4 baseTex = gTexture.Sample(gSampler, uv);
//    float pulse = frac(dot(baseTex.rgb, float3(1, 1, 1)) * 5.0); // 色が変わると動く

//    // --- 2. 虹色のゆらぎ（万華鏡風） ---
//    // 座標を回転・歪ませることで、静止画でも動いているように見せます
//    float angle = pulse * 6.28;
//    float2 offset = float2(cos(angle), sin(angle)) * 0.005;
//    float3 rainbow = float3(
//        gTexture.Sample(gSampler, uv + offset).r,
//        gTexture.Sample(gSampler, uv).g,
//        gTexture.Sample(gSampler, uv - offset).b
//    );

//    // --- 3. キラキラ光る「星屑」エフェクト ---
//    // frac(sin(...)) を使って格子状に光る点を作り、
//     その明るさを baseTex の色味で変化させることで「またたき」を作ります
//    float2 grid = frac(uv * 30.0);
//    float star = smoothstep(0.45, 0.5, 1.0 - length(grid - 0.5));
//    float glitter = pow(star, 20.0) * pulse;

//    // --- 4. パステル・トーン仕上げ ---
//    float3 finalColor = rainbow * 1.1 + float3(0.2, 0.1, 0.3); // 全体をピンク紫寄りに
//    finalColor += glitter * float3(1.0, 0.8, 0.9); // ピンクのキラキラを足す

//    output.color = float4(finalColor, baseTex.a);
//    return output;
//}


PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float2 uv = input.texcoord;
    float4 texColor = gTexture.Sample(gSampler, uv);
    
    float2 delta = float2(1.0 / 1280.0, 1.0 / 720.0);
    float4 n = gTexture.Sample(gSampler, uv + float2(0, delta.y));
    float4 s = gTexture.Sample(gSampler, uv - float2(0, delta.y));
    float4 e = gTexture.Sample(gSampler, uv + float2(delta.x, 0));
    float4 w = gTexture.Sample(gSampler, uv - float2(delta.x, 0));
    float edge = length(n + s + e + w - 4.0 * texColor);
    
    //float2 gridUV = frac(uv * float2(20.0, 11.0));
    //float grid = (gridUV.x < 0.02 || gridUV.y < 0.03) ? 0.2 : 0.0;
    
    float scanline = smoothstep(0.9, 1.0, sin(uv.y * 5.0));
    
    float luminance = dot(texColor.rgb, float3(0.2126, 0.7152, 0.0722));
    float3 baseColor = float3(0.0, 0.4, 0.8) * luminance;
    float3 highlight = float3(0.4, 0.9, 1.0) * edge * 5.0;
    
    // --- 1. 疑似アニメーションのシード値 ---
    // 本来の Time の代わりに、色の輝度や座標を混ぜて「常に揺らぐ」値を作ります
    float4 baseTex = gTexture.Sample(gSampler, uv);
    float pulse = frac(dot(baseTex.rgb, float3(1, 1, 1)) * 5.0); // 色が変わると動く

    // --- 2. 虹色のゆらぎ（万華鏡風） ---
    // 座標を回転・歪ませることで、静止画でも動いているように見せます
    float angle = pulse * 6.28;
    float2 offset = float2(cos(angle), sin(angle)) * 0.005;
    float3 rainbow = float3(
        gTexture.Sample(gSampler, uv + offset).r,
        gTexture.Sample(gSampler, uv).g,
        gTexture.Sample(gSampler, uv - offset).b
    );

    // --- 3. キラキラ光る「星屑」エフェクト ---
    // frac(sin(...)) を使って格子状に光る点を作り、
   //  その明るさを baseTex の色味で変化させることで「またたき」を作ります
    float2 grid = frac(uv * 30.0);
    float star = smoothstep(0.45, 0.5, 1.0 - length(grid - 0.5));
    float glitter = pow(star, 20.0) * pulse;
    
    float3 finalColor = baseColor + highlight /* + grid*/;
    finalColor += scanline * float3(0.0, 0.5, 1.0) * 0.3;
    finalColor += glitter * float3(1.0, 0.8, 0.9); // ピンクのキラキラを足す
    
    output.color = float4(finalColor, texColor.a);
    return output;
}