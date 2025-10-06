struct VS_INPUT
{
    int3 i : INDEX; //v, vn, vt
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

struct VERTEX
{
    float4 pos;
};
struct NORMAL
{
    float3 normal;
};
struct TEX
{
    float2 tex;
};

struct MATRIX
{
    float4x4 mat;
};

ConstantBuffer<MATRIX> wvpMat : register(b0);

StructuredBuffer<VERTEX> Vertex : register(t1);
StructuredBuffer<NORMAL> Normal : register(t2);
StructuredBuffer<TEX> Texture : register(t3);

VS_OUTPUT main(VS_INPUT index)
{
    VS_OUTPUT output;
    output.pos = mul(Vertex[index.i[0]].pos, wvpMat.mat);
    output.texCoord = Texture[index.i[2]].tex;
    return output;
}