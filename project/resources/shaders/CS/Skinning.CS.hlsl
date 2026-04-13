struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

struct Vertex
{
    float32_t4 position;
    float32_t2 texcoord;
    float32_t3 normal;
};

struct VertexInfluence
{
    float32_t4 weight;
    int32_t4 index;
};

struct SkinningInformation
{
    uint32_t numVertices;
};

StructuredBuffer<Well> gMatrixPalette : register(t0);

StructuredBuffer<Vertex> gInputVertices : register(t1);

StructuredBuffer<VertexInfluence> gInfluences : register(t2);

RWStructuredBuffer<Vertex> gOutputVertices : register(u0);

ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]

void main( uint3 DTid : SV_DispatchThreadID )
{
    uint32_t vertexIndex = DTid.x;
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        Vertex inputVertex = gInputVertices[vertexIndex];
        VertexInfluence influence = gInfluences[vertexIndex];
        
        Vertex outputVertex;
        
        // Transform Position
        outputVertex.position = mul(inputVertex.position, gMatrixPalette[influence.index.x].skeletonSpaceMatrix) * influence.weight.x;
        outputVertex.position += mul(inputVertex.position, gMatrixPalette[influence.index.y].skeletonSpaceMatrix) * influence.weight.y;
        outputVertex.position += mul(inputVertex.position, gMatrixPalette[influence.index.z].skeletonSpaceMatrix) * influence.weight.z;
        outputVertex.position += mul(inputVertex.position, gMatrixPalette[influence.index.w].skeletonSpaceMatrix) * influence.weight.w;
        outputVertex.position.w = 1.0f;
        
        // Transform Normal
        outputVertex.normal = mul(inputVertex.normal, (float32_t3x3) gMatrixPalette[influence.index.x].skeletonSpaceInverseTransposeMatrix) * influence.weight.x;
        outputVertex.normal += mul(inputVertex.normal, (float32_t3x3) gMatrixPalette[influence.index.y].skeletonSpaceInverseTransposeMatrix) * influence.weight.y;
        outputVertex.normal += mul(inputVertex.normal, (float32_t3x3) gMatrixPalette[influence.index.z].skeletonSpaceInverseTransposeMatrix) * influence.weight.z;
        outputVertex.normal += mul(inputVertex.normal, (float32_t3x3) gMatrixPalette[influence.index.w].skeletonSpaceInverseTransposeMatrix) * influence.weight.w;
        outputVertex.normal = normalize(outputVertex.normal);
        
        outputVertex.texcoord = inputVertex.texcoord;
        
        gOutputVertices[vertexIndex] = outputVertex;
    }

}