cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;
	float4x4 CameraToRenderTransform;
	float4x4 RenderToClipTransform;
};
// -----------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;
	float4 ModelColor;
}
// -----------------------------------------------------------------------------
cbuffer WorldConstants : register(b4)
{
    float4 CameraPosition;
    float  FogNearDistance;
    float  FogFarDistance;
    float2 WorldPadding;
}
// -----------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);
// -----------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelSpacePosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};
// -----------------------------------------------------------------------------
struct v2p_t
{
	float4 clipSpacePosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
    float3 worldPosition : WorldPos;
};
// -----------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelSpacePosition = float4(input.modelSpacePosition, 1.0f);
	float4 worldSpacePosition = mul(ModelToWorldTransform, modelSpacePosition);
	float4 cameraSpacePosition = mul(WorldToCameraTransform, worldSpacePosition);
	float4 renderSpacePosition = mul(CameraToRenderTransform, cameraSpacePosition);
	float4 clipSpacePosition = mul(RenderToClipTransform, renderSpacePosition);

	v2p_t v2p;
	v2p.clipSpacePosition = clipSpacePosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
    v2p.worldPosition = cameraSpacePosition.xyz;
	return v2p;
}
// -----------------------------------------------------------------------------
float3 DiminishingAdd(float3 a, float3 b)
{
    return 1.f - (1.f - a) * (1.f - b);
}
// -----------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
	// Sampling the base color
	float4 textureColor = float4(1,1,1,1);
	
	// Lighting calculation
    float outdoorInfluence = input.color.r;
    float indoorInfluence  = input.color.g;
    float vertexBrightness = input.color.b;
	
    float3 outdoorLightingInfluence = outdoorInfluence;
    float3 indoorLightingInfluence = indoorInfluence;
    float3 diffuseColor = DiminishingAdd(outdoorLightingInfluence, indoorLightingInfluence);
	
	// Applying lighting and vertex modulation
    float3 baseColor = textureColor.rgb * diffuseColor * vertexBrightness * ModelColor.rgb;
	
	float3 FogColor = float3(0.33f, 0.33f, 0.33f);
	float fogDistToCamera = input.worldPosition.z;

	float fogFraction = saturate((fogDistToCamera - FogNearDistance) / (FogFarDistance - FogNearDistance));
	fogFraction = fogFraction * fogFraction;

	float3 finalColor = lerp(baseColor, FogColor, fogFraction);
	
	//float4 vertexColor = input.color;
	//float4 color = vertexColor * textureColor * ModelColor;
	return float4(finalColor, textureColor.a * ModelColor.a);
}