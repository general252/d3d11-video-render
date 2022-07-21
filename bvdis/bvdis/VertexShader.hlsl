struct VSIn
{
    float3 pos : POSITION;
    float2 tex : TexCoord;
};


struct VSOut
{
    float2 tex : TexCoord;
    float4 pos : SV_POSITION;
};

cbuffer CBuf
{
    matrix transform;
};


VSOut VSMain(VSIn vIn)
{
    VSOut vsout;
    
    vsout.pos = mul(float4(vIn.pos, 1), transform);
    vsout.tex = vIn.tex;
    
    return vsout;
}