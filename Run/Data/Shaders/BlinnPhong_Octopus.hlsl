//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 modelTangent : TANGENT;
	float3 modelBitangent : BITANGENT;
	float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
	float4 worldPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float4 worldTangent : TANGENT;
	float4 worldBitangent : BITANGENT;
	float4 worldNormal : NORMAL;
};

// -----------------------------------------------------------------------------------------------
struct PointLight
{
	float4 Position;
	float4 Color;
};
#define MAX_POINT_LIGHTS 64
// -----------------------------------------------------------------------------------------------
struct SpotLight
{
    float4 Position;                  
    float4 Color;                        

    float3 SpotLightDirection;          
    float  InnerRadius;                 

    float OuterRadius;                  
    float InnerPenumbraDotThreshold;    
    float OuterPenumbraDotThreshold;   
    float Padding;                      
};
#define MAX_SPOT_LIGHTS 8
//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
	float		c_time;
	int			c_debugInt;
	float		c_debugFloat;
	int			EMPTY_PADDING;
};
// -----------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;	// View transform
	float4x4 CameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 RenderToClipTransform;		// Projection transform
	float3   CameraPosition;
	float    Padding;
};
//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;		// Model transform
	float4 ModelColor;
};
//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b4)
{
	float3 SunDirection;
	float SunIntensity;

	float AmbientIntensity;
	float3  padders;

	int NumPointLights;
	float3 pointPadding;
	PointLight PointLights[MAX_POINT_LIGHTS];

	int NumSpotLights;
	float3 spotPadding;
	SpotLight SpotLights[MAX_SPOT_LIGHTS];
};
// -----------------------------------------------------------------------------------------------
Texture2D diffuseTexture	 : register(t0);
Texture2D normalTexture		 : register(t1);
Texture2D specGlosEmiTexture : register(t2);
//------------------------------------------------------------------------------------------------
SamplerState samplerState		: register(s0);
SamplerState normalSampler		: register(s1);
SamplerState specGlosEmiSampler : register(s2);
//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelPosition = float4(input.modelPosition, 1);
	float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
	float4 cameraPosition = mul(WorldToCameraTransform, worldPosition);
	float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
	float4 clipPosition = mul(RenderToClipTransform, renderPosition);

	float4 worldTangent   = mul(ModelToWorldTransform, float4(input.modelTangent, 0.0f));
	float4 worldBitangent = mul(ModelToWorldTransform, float4(input.modelBitangent, 0.0f));
	float4 worldNormal    = mul(ModelToWorldTransform, float4(input.modelNormal, 0.0f));

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.worldPosition = worldPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.worldTangent = worldTangent;
	v2p.worldBitangent = worldBitangent;
	v2p.worldNormal = worldNormal;
	return v2p;
}
// -----------------------------------------------------------------------------
float RangeMapClamped(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
	float  fraction = saturate((inValue - inStart) / (inEnd - inStart));
	float  outValue = (outStart + fraction * (outEnd - outStart));
	return outValue;
}
float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd)
{
	float fraction = (inValue - inStart) / (inEnd - inStart);
	float outValue = (outStart + fraction * (outEnd - outStart));
	return outValue;
}
float SmoothStep3(float x)
{
	return (3.0*(x*x)) - (2.0*x)*(x*x);
}
// -----------------------------------------------------------------------------
float3 EncodeXYZToRGB( float3 vec )
{
	return (vec + 1.0) * 0.5;
}
float3 DecodeRGBToXYZ( float3 color )
{
	return (color * 2.0) - 1.0;
}
//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
	//---------------------------------------SET COLORS-------------------------------------------------//
	float4 textureColor = diffuseTexture.Sample(samplerState, input.uv);
	float4 normalColor	= normalTexture.Sample(normalSampler, input.uv);
	float4 specGlosEmiColor = specGlosEmiTexture.Sample(specGlosEmiSampler, input.uv);
	float4 vertexColor = input.color;
	float4 modelColor = ModelColor;
	float4 diffuseTintColor = textureColor * vertexColor * modelColor;
	float3 pixelNormalTBNSpace = normalize( DecodeRGBToXYZ( normalColor.rgb ) );
	float specularity = specGlosEmiColor.r;  // Specular (Red channel)
	float glossiness = specGlosEmiColor.g;  // Glossiness (Green channel)
	float emissive = specGlosEmiColor.b;  // Emissive (Blue channel)
	//-------------------------------------------------------------------------------------------------------//

	//---------------------------------------TBN BASIS VECTORS-----------------------------------------------//
	float3 worldTangent   = normalize(input.worldTangent.xyz);
	float3 worldBitangent = normalize(input.worldBitangent.xyz);
	float3 worldNormal    = normalize(input.worldNormal.xyz);
	float3x3 tbnToWorld = float3x3(worldTangent, worldBitangent, worldNormal);
	float3 pixelNormalWorldSpace = mul(pixelNormalTBNSpace, tbnToWorld);
	//-------------------------------------------------------------------------------------------------------//

	//--------------------------------------------LIGHTING----------------------------------------------------//
	float3 totalDiffuseLight = float3( 0.f, 0.f, 0.f );
	float3 totalSpecularLight = float3( 0.f, 0.f, 0.f );
	float  specExp = RangeMap(glossiness, 0.f, 1.f, 1.f, 32.f);
	float3 pixelToCameraDirection = normalize(CameraPosition - input.worldPosition.xyz);
	//-------------------------------------------------------------------------------------------------------//

	//---------------------------------------DIRECTIONAL LIGHT-------------------------------------------------//
	// TODO: Change to progressive ambiance
	float4 ambient = AmbientIntensity * float4(1.0f, 1.0f, 1.0f, 1.0f);
	float4 directional = SunIntensity * saturate(dot(normalize(input.worldNormal.xyz), -SunDirection)) * ambient;
	float4 lightColor = ambient + directional;
	float  diffuseLightDot = dot(-SunDirection, pixelNormalWorldSpace);
	if(c_debugInt == 10)
	{
		diffuseLightDot = dot(-SunDirection, worldNormal);
	}
	float lightIntensity = saturate(RangeMapClamped(diffuseLightDot, -1.0, 1.0, -1 + 2*AmbientIntensity, 1.0));
	//-------------------------------------------------------------------------------------------------------//

	//---------------------------------------SPECULAR AND GLOSSINESS------------------------------------------//
	float3 lightDirection = normalize(-SunDirection);
	float3 halfwayNormal = normalize(lightDirection + pixelToCameraDirection);

	float specularAngle = max(dot(pixelNormalWorldSpace, halfwayNormal), 0.0f);
	float specular = pow(specularAngle, glossiness * 64.0f);
	lightColor.rgb += specularity * 5.f * specular * lightIntensity;
	//-------------------------------------------------------------------------------------------------------//

	//--------------------------------------------EMISSIVE---------------------------------------------------//
	float3 emissiveFinalColor = textureColor.rgb * emissive;
	//-------------------------------------------------------------------------------------------------------//

	//-----------------------------------------POINT LIGHTS---------------------------------------------------//
	for (int lightIndex = 0; lightIndex < NumPointLights; ++lightIndex)
	{
		// Set
		float4 pointLightPos = PointLights[lightIndex].Position;
		float4 pointlightColor = PointLights[lightIndex].Color;
		
		// Attenuation
		float3 pixelToLightDir = normalize(pointLightPos.xyz - input.worldPosition.xyz);
		float distance = length(pointLightPos.xyz - input.worldPosition.xyz);
		float linearfalloff = 0.09f;
		float quadratic = 0.032f;
		float attenuation = 1.0f / (1.0f + linearfalloff * distance + quadratic * distance * distance);

		// Diffuse
		float diffuseDot = saturate(dot(pixelNormalWorldSpace, pixelToLightDir));
		float3 diffuseLight = pointlightColor.rgb * diffuseDot * attenuation;

		// Specular
		float3 halfVector = normalize(pixelToLightDir + pixelToCameraDirection);
		float specularAngle = max(dot(pixelNormalWorldSpace, halfVector), 0.0f);
		float specularExponent = pow(specularAngle, glossiness * 64.0f);
		float3 specularStrength = specularity * specularExponent * pointlightColor.rgb * attenuation;

		// Combine
		lightColor.rgb += diffuseLight + specularStrength;
	}
	//-------------------------------------------------------------------------------------------------------//

	//------------------------------------------SPOT LIGHTS---------------------------------------------------//
	for (int spotLightIndex = 0; spotLightIndex < NumSpotLights; ++spotLightIndex)
	{
		// Set
		SpotLight light = SpotLights[spotLightIndex];
		float3 spotlightPos = light.Position.xyz;
		float3 spotlightColor = light.Color.rgb;
		float spotlightInnerRadius = light.InnerRadius;
		float spotlightOuterRadius = light.OuterRadius;
		float spotlightInnerThreshold = light.InnerPenumbraDotThreshold;
		float spotlightOuterThreshold = light.OuterPenumbraDotThreshold;
		float3 spotLightDirection = normalize(light.SpotLightDirection);

		// Pixel To Light
		float3 pixelToLightDisp = spotlightPos - input.worldPosition.xyz;
		float3 pixelToLightDirection = normalize(pixelToLightDisp);
		float  distance = length(pixelToLightDisp);
		float3 lightToPixelDir = -pixelToLightDirection;

		// Attenuation
		float attenuation = RangeMapClamped(distance, spotlightInnerRadius, spotlightOuterRadius, 1.0f, 0.0f);
		attenuation = SmoothStep3(attenuation);
		float dotAngle = dot(lightToPixelDir, spotLightDirection);
		float penumbraAttenuation = RangeMapClamped(dotAngle, spotlightOuterThreshold, spotlightInnerThreshold, 0.0f, 1.0f);
		penumbraAttenuation = SmoothStep3(penumbraAttenuation);
		float totalAttenuation = penumbraAttenuation * attenuation;

		// Diffuse
		float diffuseDot = saturate(dot(pixelNormalWorldSpace, pixelToLightDirection));
		float3 diffuseLight = spotlightColor * diffuseDot * totalAttenuation;

		// Specular
		float3 halfVector = normalize(pixelToLightDirection + pixelToCameraDirection);
		float specularAngle = max(dot(pixelNormalWorldSpace, halfVector), 0.0f);
		float specularExponent = pow(specularAngle, glossiness * 64.0f);
		float3 specularStrength = specularity * specularExponent * spotlightColor * totalAttenuation;

		// Combine
		lightColor.rgb += diffuseLight + specularStrength;
	}
	//-------------------------------------------------------------------------------------------------------//
	//float4 color = lightColor * textureColor * vertexColor * modelColor;
	//float4 color = float4(diffuseTintColor.rgb * lightIntensity, diffuseTintColor.a); 
	float4 color = float4(diffuseTintColor.rgb * lightColor.rgb, diffuseTintColor.a);
	color.rgb += emissiveFinalColor.rgb;
	clip(color.a - 0.01f);
	return color;
}
