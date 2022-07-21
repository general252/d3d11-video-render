Texture2D<float> luminanceChannel : t0; // y
Texture2D<float2> chrominanceChannel : t1; // uv

SamplerState samplerState;

static const float3x3 YUVtoRGBCoeffMatrix = {
    1.164383f, 1.164383f, 1.164383f,
	0.000000f, -0.391762f, 2.017232f,
	1.596027f, -0.812968f, 0.000000f
};

float3 ConvertYUVtoRGB(float3 yuv)
{
	// Derived from https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx
	// Section: Converting 8-bit YUV to RGB888

	// These values are calculated from (16 / 255) and (128 / 255)
    yuv -= float3(0.062745f, 0.501960f, 0.501960f);
    yuv = mul(yuv, YUVtoRGBCoeffMatrix);

	// saturate: 对x分量限制在[0,1]范围
    return saturate(yuv);
}

float4 PSMain(float2 textureCoord : TexCoord) : SV_Target
{
    float y = luminanceChannel.Sample(samplerState, textureCoord);
    float2 uv = chrominanceChannel.Sample(samplerState, textureCoord);
    
    return float4(ConvertYUVtoRGB(float3(y, uv)), 1);
}