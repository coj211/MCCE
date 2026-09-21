#version 450 compatibility

// /*
// const bool colortex4MipmapEnabled = true;
// */

layout(location = 0) out vec4 reflectionData;
//layout(location = 1) out vec3 sceneData;


#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"
#include "/lib/Atmosphere/Atmosphere.glsl"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Material.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#include "/lib/Surface/ScreenSpaceReflections.glsl"

uniform int bossBattle;

vec3 ComputeFakeSkyReflection(in vec3 reflectWorldDir) {
	#ifdef DARK_END
		bool darkEnd = bossBattle == 2 || bossBattle == 3;
	#else
		const bool darkEnd = false;
	#endif

	vec3 sunDisc = RenderSun(reflectWorldDir, worldSunVector);
	sunDisc *= vec3(0.99, 0.93, 0.65) * 0.1;
	if (darkEnd) sunDisc *= 0.0;
	return sunDisc + mix(vec3(0.396, 0.352, 0.108), vec3(0.04, 0.02, 0.05), float(darkEnd) * 0.9) * 2.0 * exp2(-max0(reflectWorldDir.y) * 1.5);	
}

vec4 CalculateSpecularReflections(/*inout vec3 color, */in vec3 normal, in vec3 screenPos, Material material, in vec3 viewPos) {
	//skylight = smoothstep(0.3, 0.8, skylight);
	vec3 viewDir = normalize(viewPos);

	vec3 rayDir;
	#ifdef ROUGH_REFLECTIONS
		if (material.isRough) {
			mat3 tangentToWorld;
			tangentToWorld[0] = normalize(cross(gbufferModelView[1].xyz, normal));
			tangentToWorld[1] = cross(normal, tangentToWorld[0]);
			tangentToWorld[2] = normal;

			vec3 tangentView = -viewDir * tangentToWorld;
			vec3 facetNormal = tangentToWorld * sampleGGXVNDF(tangentView, material.roughness, RandNext2F());
			rayDir = reflect(viewDir, facetNormal);
		} else
	#endif
	{ rayDir = reflect(viewDir, normal); }

	float NdotL = dot(normal, rayDir);
	if (NdotL < 1e-6) return vec4(0.0);

	//float dither = BlueNoiseTemporal(0.447213595);
	float dither = InterleavedGradientNoiseTemporal(gl_FragCoord.xy);
	//vec3 screenPos = vec3(screenCoord, depth);
	
	float NdotV = max(1e-6, dot(normal, -viewDir));
	//#ifdef HQ_TRACING
		bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, uint(RAYTRACE_SAMPLES * oneMinus(material.roughness)), screenPos);
	//#else
	//	bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, uint(RAYTRACE_SAMPLES * oneMinus(material.roughness)), screenPos);
	//#endif

	vec3 reflection;

	if (hit) {
		// reflection = textureLod(colortex4, screenPos.xy * screenPixelSize, int(8.0 * sqrt(material.roughness))).rgb;
		reflection = texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb;
	} else {
		float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
		reflection = ComputeFakeSkyReflection(mat3(gbufferModelViewInverse) * rayDir);
		reflection *= NdotU;
	}


	float dist = 0.0;
	float specular = 0.0;
	if (material.isRough) {
		//vec3 lightDir = normalize(reflect(viewDir, normal) + normal * roughness);
		vec3 halfWay = normalize(rayDir - viewDir);
		float LdotH = saturate(dot(rayDir, halfWay));

		float F = FresnelSchlick(LdotH, material.f0);
		float alpha2 = material.roughness * material.roughness;
		float V2 = V2SmithGGX(NdotV, NdotL, alpha2);
		float V1Inverse = V1SmithGGXInverse(NdotV, alpha2);

		specular = NdotL * F * V2 * V1Inverse;
		//specular *= 1.0 - saturate(material.roughness * 4.0 - 1.5);	//roughness clamp
		vec3 reflectViewPos = ScreenToViewSpace(vec3(screenPos.xy * screenPixelSize, GetDepthFix(ivec2(screenPos.xy))));
		float rDist = distance(reflectViewPos, viewPos);

		dist = saturate(max(rDist * 2.0, material.roughness * 3.0));
	} else {
		specular = FresnelDielectric(NdotV, material.f0);
	}

	specular *= oneMinus(material.isMetal);
	//color *= oneMinus(specular);

	return clamp16F(vec4(reflection * (specular + material.isMetal), dist));
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	//vec3 gbuffer3 = texelFetch(colortex3, texel, 0).rgb;
	//specTex = UnpackUnorm2x8(gbuffer3.z);
	Material material = GetMaterialData(texelFetch(colortex0, texel, 0).xy);

	//sceneData = texelFetch(colortex4, texel, 0).rgb;
	if (material.hasReflections) {
		vec2 screenCoord = gl_FragCoord.xy * screenPixelSize;

		vec3 normal = DecodeNormal(texelFetch(colortex3, texel, 0).xy);
		float depth = GetDepthFix(texel);
		vec3 screenPos = vec3(screenCoord, depth);
		vec3 viewPos = ScreenToViewSpace(screenPos);
		reflectionData = CalculateSpecularReflections(/*sceneData, */normal, screenPos, material, viewPos);
	}
}

/* DRAWBUFFERS:2 */
