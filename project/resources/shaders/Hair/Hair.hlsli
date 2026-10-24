struct ControllerPoint
{
    float3 position;
    float radius;
    float3 color;
    float nextToLength;
    float3 homePosition;
    float physicsWeight;
    float3 velocity;
    float padding;
};

struct GuideInfo
{
    uint vertexStartIndex;
    uint vertexCount;
};

//struct HairPhysicsConfig
//{
//    float stiffness; // 剛性
//    float restoringForce; // 復元力（初期姿勢に戻ろうとする力）
//    float damping; // 減衰（空気抵抗など）
//    float padding; // アラインメント用
//};

//struct FrameConfig
//{
//    float3 gravity; // 重力ベクトル (例: 0, -9.8, 0)
//    float deltaTime; // 前フレームからの経過時間
//    float3 windDirection; // 風の向きと強さ
//    float time; // 経過時間（風のうねり用）
//    float3 moveDirection;// 動いた方向
//    float padding;
//};

struct StrandVertex
{
    float3 position;
    float radius;
    float3 color;
    float padding;
};

struct StrandInfo
{
    uint vertexStartIndex;
    uint vertexCount;
    uint aabbStartIndex;
};

struct SegmentData
{
    uint v0_Index;
    uint v1_Index;
};

struct ChildStrand
{
    uint parentGuideIds[3]; // 影響を受ける親ガイドのID
    uint blendMode; // 0: 単一ガイド、1: 複数ブレンド
    float weights[3]; // ガイドへの影響力（ウエイト）
    float lengthScale; // 髪の長さの倍率
    float2 offset; // ガイドからの2Dオフセット位置
    float twistAngle; // ねじれの大きさ
    float clumpForce; // 束感（クランプ）の強さ
    uint seed; // 乱数シード
    float waveAmplitude; // うねりの強さ
    float waveFrequency; // うねりの細かさ
    float noise;// アホ毛
};

//struct HairMakeConfig
//{
//    float globalSpreadRadius; // 全体の広がり
//    float globalHairThickness; // 全体の太さ倍率
//    float2 padding2;
//};

struct HairConfig
{
    uint32_t numGuides;
    uint32_t pointPerGuide;
    uint32_t pointPerStrand;
    float32_t numStrands;
};

struct RaytracingAABB
{
    float minPositionX;
    float minPositionY;
    float minPositionZ;
    float maxPositionX;
    float maxPositionY;
    float maxPositionZ;
};

// --- 髪の調整用パラメーター ---
struct HairMaterialParameter
{
    float gStiffness;
    float gRestoringForce;
    float gDamping;
    float gGlobalHairThickness;

    float gGlobalSpreadRadius;
    float gRoughness;
    float gShift;
    float gSpecular;

    float gScatter;
    float3 gMoveDirection;

    float4x4 gCharacterMatrix;
};

// --- 環境・シーン用 ---
struct HairSceneParameter
{
    float3 gGravity;
    float gDeltaTime;

    float3 gWindDirection;
    float gTime;
};