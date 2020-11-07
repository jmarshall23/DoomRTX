cbuffer UniformBlock0 : register(b0)
{
	float4x4 mvp;
};

struct VSOutput {
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
	float3 Color : COLOR;
};

VSOutput VSMain(float4 Position : POSITION, float2 TexCoord : TEXCOORD0, float3 Color : COLOR)
{
	VSOutput result;
	result.Position = mul(mvp, Position);
	result.TexCoord = TexCoord;
	result.Color = Color;
	return result;
}

Texture2D uTex0 : register(t1);
SamplerState uSampler0 : register(s2);

float4 PSMain(VSOutput input) : SV_TARGET
{
	float4 albedo = uTex0.Sample(uSampler0, input.TexCoord);
	albedo.xyz = albedo * input.Color.xyz;
	return albedo;
}