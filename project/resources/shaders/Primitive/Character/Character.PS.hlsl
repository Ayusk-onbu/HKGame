#include "Character.hlsli"
#include "../../Common.hlsli"

struct PixelShaderOutPut
{
    float32_t4 color : SV_TARGET0;
    float32_t4 sssRadius : SV_TARGET1;
};

Texture2D<float32_t4> gTextures[] : register(t0);
SamplerState gSampler : register(s0);

// メラニンによる表皮の基本色(吸収色)を算出
float32_t3 ComputeMelaninColor(float32_t melaninAmount, float32_t melaninTypeRatio)
{
    // ユーメラニン
    float32_t3 sigma_a_eumelanin = float32_t3(0.66f, 1.95f, 4.35f);
    
    // フェオメラニン
    float32_t3 sigma_a_pheomelanin = float32_t3(0.12f, 1.10f, 3.80f);

    // 比率で混合
    float32_t3 absorptionCoeff = lerp(sigma_a_pheomelanin, sigma_a_eumelanin, melaninTypeRatio);
    
    // 総量を乗算して吸光度を決定
    float32_t3 totalAbsorption = melaninAmount * absorptionCoeff;
    
    // Beer-Lambertの法則
    // 基本色（Albedo）を算出
    return exp(-totalAbsorption);
}

// メラニンとヘモグロビンの吸収を組み合わせて皮膚の色を算出
float3 ComputeSkinAbsorption(
    float melaninAmount, // メラニン量 (0.0 ~ 1.0)
    float melaninTypeRatio, // メラニン種類 (0.0:フェオ ~ 1.0:ユー)
    float bloodVolume, // 血流量・赤みマスク (0.0 ~ 1.0)
    float oxygenation)       // 酸素飽和度 (0.0:暗い紫赤 ~ 1.0:鮮やかな赤)
{
    // メラニン
    float3 sigma_a_eumelanin = float3(0.66, 1.95, 4.35);
    float3 sigma_a_pheomelanin = float3(0.12, 1.10, 3.80);

    // ヘモグロビン（緑と青を強力に吸収し、赤だけを通す）
    float3 sigma_a_oxy = float3(0.15, 3.20, 4.20); // 酸化ヘモグロビン (動脈・鮮血)
    float3 sigma_a_deoxy = float3(0.40, 2.80, 3.90); // 脱酸化ヘモグロビン (静脈・暗赤)

    // メラニン吸光度
    float3 absMelanin = melaninAmount * lerp(sigma_a_pheomelanin, sigma_a_eumelanin, melaninTypeRatio);

    // ヘモグロビン吸光度
    float3 absBlood = bloodVolume * lerp(sigma_a_deoxy, sigma_a_oxy, oxygenation);

    // 吸光度の合算
    float3 totalAbsorption = absMelanin + absBlood;

    // Beer-Lambert法則で光の透過率を算出
    return exp(-totalAbsorption);
}

// コラーゲンの散乱と糖化の影響を適用するための関数
float32_t3 ApplyCollagenScattering(
    float32_t3 absorbedColor, // 吸光色
    float32_t collagenDensity, // コラーゲンの密度 (0.0 ~ 2.0 高密度・透明感)
    float32_t glycationFactor)// 糖化度(0.0 ～ 1.0 強い黄くすみ)
{   
    // 糖化物質による黄ばみの吸光係数
    float3 sigma_a_ages = float3(0.05, 0.25, 0.80);

    // 糖化による黄くすみ
    // 青色光が強力に削られるので、黄色〜茶色の濁りが残る
    float3 glycationColor = exp(-glycationFactor * sigma_a_ages);

    // 糖化による散乱効率の低下
    // 糖化が進んだコラーゲンは硬化・破壊され、光を綺麗に散乱させにくくなる
    float scatteringEfficiency = collagenDensity * lerp(1.0, 0.5, glycationFactor);

    // 最終的な肌の内部拡散色
    return absorbedColor * glycationColor * scatteringEfficiency;
}

// GGX法線分布関数を使用して、肌の表面の微細な凹凸による反射を計算する関数
float32_t D_GGX(float32_t NdotH, float32_t roughness)
{
    float32_t a = roughness * roughness;
    float32_t a2 = a * a;
    float32_t denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * denom * denom);
}

// Schlick近似によるフレネル反射
float32_t3 FresnelSchlick(float32_t VdotH, float32_t3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

// 簡易的な可視性関数 (Smith Joint GGX 近似)
float Vis_SmithJointApprox(float NdotV, float NdotL, float roughness)
{
    float a = roughness;
    float lambdaV = NdotL * (NdotV * (1.0 - a) + a);
    float lambdaL = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / max(lambdaV + lambdaL, 0.0001);
}

// Dual Lobe GGX 表面反射率を算出する関数
float3 ComputeDualLobeGGX(
    float3 N, float3 V, float3 L,
    float roughnessPrimary, // 1. 皮脂層の粗さ (鋭いハイライト: 0.1 ~ 0.25)
    float roughnessSecondary, // 2. 角質層の粗さ (広がるハイライト: 0.45 ~ 0.7)
    float primaryWeight)      // 皮脂層の反射比率 (0.05 ~ 0.15)
{
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // 人間の皮膚の基本フレネル反射率 F0 (屈折率 IOR ≈ 1.4 で約 0.04 ~ 0.045)
    float3 F0 = float3(0.045, 0.045, 0.045);
    float3 F = FresnelSchlick(VdotH, F0);

    // --- Lobe 1: 皮脂膜（シャープで強い鏡面反射） ---
    float D1 = D_GGX(NdotH, roughnessPrimary);
    float Vis1 = Vis_SmithJointApprox(NdotV, NdotL, roughnessPrimary);
    float3 specPrimary = (D1 * Vis1) * F;

    // --- Lobe 2: 角質層（柔らかくワイドに広がる反射） ---
    float D2 = D_GGX(NdotH, roughnessSecondary);
    float Vis2 = Vis_SmithJointApprox(NdotV, NdotL, roughnessSecondary);
    float3 specSecondary = (D2 * Vis2) * F;

    // 2つのLobeを合成 (皮脂層の割合 primaryWeight でブレンド)
    float3 finalSpecular = lerp(specSecondary, specPrimary, primaryWeight);

    return finalSpecular * NdotL;
}

// 光が肌内部を往復する物理挙動に基づく BaseColor 算出
float3 ComputePhysicalSkinAlbedo1(
    float melaninAmount, // メラニン量 (0.0 ~ 1.0)
    float melaninTypeRatio, // 0.0:フェオメラニン(赤黄) ~ 1.0:ユーメラニン(黒茶)
    float bloodVolume, // 血流量 (0.0 ~ 1.0)
    float oxygenation, // 酸素飽和度 (0.0:暗赤 ~ 1.0:鮮血)
    float collagenDensity, // コラーゲン密度 (0.5 ~ 1.5)
    float glycationFactor // 糖化度 (0.0 ~ 1.0)
)
{
    // ==========================================
    // 1. 各物質の吸光係数 (Sigma Absorption)
    // ==========================================
    // メラニン
    float3 sigma_a_eumelanin = float3(0.66, 1.95, 4.35);
    float3 sigma_a_pheomelanin = float3(0.12, 1.10, 3.80);
    float3 sigma_a_melanin = lerp(sigma_a_pheomelanin, sigma_a_eumelanin, melaninTypeRatio);

    // ヘモグロビン (緑・青を強く吸収)
    float3 sigma_a_oxy = float3(0.15, 3.20, 4.20);
    float3 sigma_a_deoxy = float3(0.40, 2.80, 3.90);
    float3 sigma_a_blood = lerp(sigma_a_deoxy, sigma_a_oxy, oxygenation);

    // 糖化物質 (AGEs: 青を強力に吸収して黄色・茶色に見せる)
    float3 sigma_a_ages = float3(0.05, 0.25, 0.80);

    // ==========================================
    // 2. 表皮ゾーン (メラニンによる透過 1Pass目)
    // ==========================================
    // 光が表皮を通過して真皮に届く透過率
    float3 T_epidermis = exp(-melaninAmount * sigma_a_melanin);

    // ==========================================
    // 3. 真皮ゾーン (コラーゲン散乱 & ヘモグロビン吸光)
    // ==========================================
    // (A) コラーゲンによる構造散乱（青・緑が強く跳ね返る Rayleigh 散乱モデル）
    // 密度が高いほど散乱強度が増す
    float3 collagenScatterCoeff = float3(0.3, 0.6, 0.9) * collagenDensity;
    
    // 糖化による黄くすみ（青の吸収）と散乱効率の低下
    float3 T_glycation = exp(-glycationFactor * sigma_a_ages);
    float3 R_collagen = saturate(collagenScatterCoeff * T_glycation);

    // (B) 血管層まで届いた光の吸収と反射
    // ヘモグロビンに緑・青が吸われ、赤が残って跳ね返る
    float3 R_blood = exp(-bloodVolume * sigma_a_blood);

    // (C) 真皮全体での反射光の合成
    // コラーゲン密度が高いと浅い位置で青緑が跳ね返り(透明感)、低いと深くまで届いて血色が強くでる
    float3 R_dermis = lerp(R_blood, R_collagen, saturate(collagenDensity - 0.2));

    // ==========================================
    // 4. 表皮ゾーン (復路: 再びメラニンを通って目に入る)
    // ==========================================
    // 真皮で反射した光が、再び表皮のメラニン層を通過して外に出るため T_epidermis をもう一度掛ける
    float3 finalAlbedo = R_dermis * (T_epidermis * T_epidermis);

    return saturate(finalAlbedo);
}

// 【チューニング済】物理挙動に基づく BaseColor 算出関数
float3 ComputePhysicalSkinAlbedo(
    float melaninAmount, // メラニン量 (標準: 0.1 ~ 0.3)
    float melaninTypeRatio, // ユーメラニン比率 (0.0:赤黄 ~ 1.0:黒茶)
    float bloodVolume, // 血流量 (標準: 0.2 ~ 0.4)
    float oxygenation, // 酸素飽和度 (標準: 0.8 ~ 0.95)
    float collagenDensity, // コラーゲン密度 (標準: 1.0)
    float glycationFactor // 糖化度 (標準: 0.0)
)
{
    // ----------------------------------------------------
    // スケール定数（土人形化を防ぐ超重要パラメータ）
    // ----------------------------------------------------
    static const float EPIDERMIS_THICKNESS = 0.06f; // 表皮の厚み (mm換算スケール)
    static const float DERMIS_THICKNESS = 0.18f; // 真皮の厚み (mm換算スケール)

    // 1. 各物質の吸光係数
    float3 sigma_a_eumelanin = float3(0.66f, 1.95f, 4.35f);
    float3 sigma_a_pheomelanin = float3(0.12f, 1.10f, 3.80f);
    float3 sigma_a_melanin = lerp(sigma_a_pheomelanin, sigma_a_eumelanin, melaninTypeRatio);

    float3 sigma_a_oxy = float3(0.15f, 3.20f, 4.20f);
    float3 sigma_a_deoxy = float3(0.40f, 2.80f, 3.90f);
    float3 sigma_a_blood = lerp(sigma_a_deoxy, sigma_a_oxy, oxygenation);

    float3 sigma_a_ages = float3(0.05f, 0.25f, 0.80f);

    // 2. 表皮ゾーン (往路: 厚みスケールを適用)
    float3 absMelanin = melaninAmount * sigma_a_melanin * EPIDERMIS_THICKNESS;
    float3 T_epidermis = exp(-absMelanin);

    // 3. 真皮ゾーン (コラーゲン散乱 & ヘモグロビン吸光)
    // 肌の基本色に明るさと透明感を与えるベース散乱値
    float3 baseCollagenColor = float3(0.85f, 0.75f, 0.70f);
    float3 collagenScatterCoeff = baseCollagenColor * collagenDensity;
    
    float3 T_glycation = exp(-glycationFactor * sigma_a_ages);
    float3 R_collagen = saturate(collagenScatterCoeff * T_glycation);

    // 血管吸収 (厚みスケールを適用)
    float3 absBlood = bloodVolume * sigma_a_blood * DERMIS_THICKNESS;
    float3 R_blood = exp(-absBlood);

    // 真皮内部での合成（コラーゲンのレイリー散乱光 ＋ 深部の血色光）
    float3 R_dermis = lerp(R_blood * 0.85f, R_collagen, saturate(collagenDensity * 0.6f));

    // 4. 表皮ゾーン (復路: 2回通って目に入る)
    float3 finalAlbedo = R_dermis * (T_epidermis * T_epidermis);

    return saturate(finalAlbedo);
}

//PixelShaderOutPut main(VertexShaderOutput input)
//{
//    PixelShaderOutPut output;
    
//    // バインドレスサンプリング
//    float32_t4 mask1 = gTextures[input.skinMask1TextureIndex].Sample(gSampler, input.texcoord);
//    float32_t4 mask2 = gTextures[input.skinMask2TextureIndex].Sample(gSampler, input.texcoord);
//    float32_t3 normalMap = gTextures[input.normalTextureIndex].Sample(gSampler, input.texcoord).rgb;
    
//    // ==========================================
//    // マスクパラメータの取得（安全なデフォルト補正）
//    // ==========================================
//    // マスクテクスチャが黒(0.0)の場合でも土人形化しないよう適切なデフォルト値を乗算
//    float melaninAmount = lerp(0.15f, mask1.r, step(0.001f, mask1.r)) * input.melaninScale;
//    float bloodVolume = lerp(0.30f, mask1.g, step(0.001f, mask1.g)) * input.bloodVolumeScale;
//    float melaninTypeRatio = mask1.b;
//    float sssMask = (mask1.a > 0.001f) ? mask1.a : 1.0f;
    
//    float roughPrimary = max(mask2.r * input.roughnessScale, 0.18f); // 皮脂層（鋭い）
//    float roughSecondary = max(mask2.g * input.roughnessScale, 0.55f); // 角質層（柔らかい）
//    float primaryWeight = (mask2.b > 0.001f) ? mask2.b : 0.08f; // 皮脂の割合
//    float collagen = (mask2.a > 0.001f) ? mask2.a : 1.0f; // コラーゲン密度

//    // ==========================================
//    // 幾何学ベクトルの計算 (修正: ノーマルマップの適用)
//    // ==========================================
//    float32_t3 worldNormal = input.normal;
//    float32_t3 N = normalize(worldNormal); // ノーマルマップを正しく適用！
    
//    float32_t3 V = normalize(float32_t3(0.0f, 0.0f, -1.0f) - input.worldPosition); // カメラ方向
//    float32_t3 L = normalize(float32_t3(0.5f, 1.0f, -0.5f)); // ライト方向
    
//    float32_t NdotL = max(dot(N, L), 0.0f);
    
//    // ==========================================
//    // ライティング & カラーの計算
//    // ==========================================
//    // 肌の物理アルベド（Base Color）の算出
//    float32_t3 skinDiffuseAlbedo = ComputePhysicalSkinAlbedo(
//        melaninAmount, melaninTypeRatio, bloodVolume, 0.90f, collagen, 0.0f
//    );
    
//    // 鏡面反射 (Specular)
//    float3 specularColor = ComputeDualLobeGGX(N, V, L, roughPrimary, roughSecondary, primaryWeight);
    
//    // 光源カラー
//    float32_t3 lightColor = float32_t3(1.0f, 0.96f, 0.92f) * 1.3f; // 自然な太陽光（やや温かみのある白）
    
//    // 正しいライティング合成（lightColorの2重掛けを修正）
//    float32_t3 diffuseLighting = skinDiffuseAlbedo * NdotL;
//    float32_t3 combinedLighting = (diffuseLighting + specularColor) * lightColor;
    
//    // アンビエントライト（環境光）を少し足して影の漆黒化を防ぐ
//    float3 ambient = skinDiffuseAlbedo * float3(0.12f, 0.10f, 0.12f);
    
//    output.color.rgb = combinedLighting + ambient;
//    output.color.a = 1.0f;
    
//    // SSS散乱半径を出力（赤を広く、青緑を狭く）
//    float32_t3 baseSSSRadius = float32_t3(1.0f, 0.30f, 0.12f);
//    float32_t3 pixelScatterRadius = baseSSSRadius * skinDiffuseAlbedo * input.sssScale * sssMask;
//    output.sssRadius = float32_t4(pixelScatterRadius, 1.0f);
    
//    return output;
//}

PixelShaderOutPut main(VertexShaderOutput input)
{
    PixelShaderOutPut output;
    
    // 各テクスチャをバインドレスサンプリング
    float32_t4 albedo = gTextures[input.colorTextureIndex].Sample(gSampler, input.texcoord);
    float32_t3 normalMap = gTextures[input.normalTextureIndex].Sample(gSampler, input.texcoord).rgb;
    float32_t4 mask1 = gTextures[input.skinMask1TextureIndex].Sample(gSampler, input.texcoord);
    float32_t4 mask2 = gTextures[input.skinMask2TextureIndex].Sample(gSampler, input.texcoord);
    
    // インスタンスごとのスケールを掛けて適用
    float32_t melaninAmount = mask1.r * input.melaninScale;
    float32_t bloodVolume = mask1.g * input.bloodVolumeScale;
    float32_t melaninTypeRatio = mask1.b;
    float32_t sssMask = mask1.a;
    
    float32_t roughPrimary = mask2.r * input.roughnessScale;
    float32_t roughSecondary = mask2.g * input.roughnessScale;
    float32_t primaryWeight = mask2.b;
    float32_t collagen = mask2.a;
    
    // 接空間からワールド空間への法線変換 (Common.hlsliの関数を利用)
    //float32_t3 worldNormal = CalculateWorldNormal(normalMap, input.worldPosition, input.texcoord, input.normal);
    float3 worldNormal = input.normal;
   
    float32_t3 N = normalize(input.normal);
    float32_t3 V = normalize( /*カメラの座標*/float32_t3(0.0f, 0.0f, -1.0f) - input.worldPosition);
    float32_t3 L = normalize( /*ライティングの座標*/float32_t3(0.5f, 1.0f, -0.5f)); // 光源方向（斜め上から）
    
    float32_t NdotL = max(dot(N, L), 0.0f);
    
    // ==================
    // 【 肌の色を計算 】
    // ==================
    
    // メラニン＋ヘモグロビン(将来的にMaskで作成する)の吸収色
    float32_t3 absorbedColor = ComputeSkinAbsorption(melaninAmount, melaninTypeRatio, bloodVolume, 0.95f);
    // コラーゲン散乱と糖化の影響を適用
    float32_t3 skinDiffuseAlbedo = ApplyCollagenScattering(absorbedColor, collagen, 0.0f);
    //float32_t3 skinDiffuseAlbedo = ComputePhysicalSkinAlbedo(melaninAmount, melaninTypeRatio, bloodVolume, 0.95f, collagen, 0.0f);
    
    // 表面のツヤ
    float3 specularColor = ComputeDualLobeGGX(N, V, L, roughPrimary, roughSecondary, primaryWeight);
    
    // 光源の強度・カラー
    float32_t3 lightColor = float32_t3(1.0f, 0.95f, 0.95f) * 1.4f;
    
    float32_t3 diffuseLighting = skinDiffuseAlbedo * lightColor * NdotL;
    float32_t3 combinedLighting = (diffuseLighting + specularColor) * lightColor;
    
    // 最終カラー計算
    output.color.rgb = combinedLighting;
    //output.color.a = albedo.a * input.color.a;
    output.color.a = 1.0f;
    
    // SSSの半径を出力
    float32_t3 baseSSSRadius = float32_t3(1.0f, 0.35f, 0.15f); // RGBそれぞれの散乱半径
    
    float32_t3 pixelScatterRadius = baseSSSRadius * skinDiffuseAlbedo * input.sssScale * sssMask;
    output.sssRadius = float32_t4(pixelScatterRadius, 1.0f);
    
    return output;
}

//PixelShaderOutPut main(VertexShaderOutput input)
//{
//    PixelShaderOutPut output;
    
//    // 各テクスチャをバインドレスサンプリング
//    float4 albedo = gTextures[input.colorTextureIndex].Sample(gSampler, input.texcoord);
//    float3 normalMap = gTextures[input.normalTextureIndex].Sample(gSampler, input.texcoord).rgb;
   
//    // 接空間からワールド空間への法線変換 (Common.hlsliの関数を利用)
//    float3 worldNormal = CalculateWorldNormal(normalMap, input.worldPosition, input.texcoord, input.normal);
//    //float3 worldNormal = input.normal;
   
//    float32_t3 N = normalize(input.normal);
//    float32_t3 V = normalize(float32_t3(0.0f, 0.0f, -1.0f) - input.worldPosition);
//    float32_t3 L = normalize(float32_t3(0.5f, 1.0f, -0.5f)); // 光源方向（斜め上から）
    
//    float32_t NdotL = max(dot(N, L), 0.0f);
    
//    // ==================
//    // 【 肌の色を計算 】
//    // ==================
    
//    // メラニン＋ヘモグロビン(将来的にMaskで作成する)の吸収色
//    float32_t3 absorbedColor = ComputeSkinAbsorption(0.20f, 0.70f, 0.12f, 0.95f);
    
//    // コラーゲン散乱と糖化の影響を適用
//    float32_t3 skinDiffuseAlbedo = ApplyCollagenScattering(absorbedColor, 1.35f, 0.0f);
    
//    // 表面のツヤ
//    float3 specularColor = ComputeDualLobeGGX(N, V, L, 0.15f, 0.55f, 0.08f);
    
//    // 光源の強度・カラー
//    float32_t3 lightColor = float32_t3(1.0f, 0.95f, 0.95f) * 1.4f;
    
//    float32_t3 diffuseLighting = skinDiffuseAlbedo * lightColor * NdotL;
//    float32_t3 combinedLighting = (diffuseLighting + specularColor) * lightColor;
    
//    // 最終カラー計算
//    output.color.rgb = combinedLighting;
//    //output.color.a = albedo.a * input.color.a;
//    output.color.a = 1.0f;
    
//    // SSSの半径を出力
//    float32_t3 baseSSSRadius = float32_t3(1.0f, 0.35f, 0.15f); // RGBそれぞれの散乱半径
//    float32_t globalSSSScale = 1.5f; // SSSの全体的なスケール
    
//    float32_t3 pixelScatterRadius = baseSSSRadius * skinDiffuseAlbedo * globalSSSScale;
//    output.sssRadius = float32_t4(pixelScatterRadius, 1.0f);
    
//    return output;
//}