#version 450 compatibility

#define IS_NETHER

// /*
// const bool colortex4MipmapEnabled = true;
// */

layout(location = 0) out vec4 reflectionData;
//layout(location = 1) out vec3 sceneData;


#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

//----// STRUCTS //-------------------------------------------------------------------------------//

#include "/lib/Head/Material.inc"

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#include "/lib/Surface/ScreenSpaceReflections.glsl"

#include "/lib/Atmosphere/Fogs.glsl"

vec4 CalculateSpecularReflections(/*inout vec3 color, */in vec3 normal, in vec3 screenPos, Material material, in vec3 viewPos) {	
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
	
	float NdotV = max(1e-6, dot(-viewDir, normal));
	//#ifdef HQ_TRACING
		bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, uint(RAYTRACE_SAMPLES * oneMinus(material.roughness)), screenPos);
	//#else
	//	bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, uint(RAYTRACE_SAMPLES * oneMinus(material.roughness)), screenPos);
	//#endif

	// vec3 reflection = hit ? textureLod(colortex4, screenPos.xy * screenPixelSize, int(8.0 * sqrt(material.roughness))).rgb : vec3(0.0);
	vec3 reflection = hit ? texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb : vec3(0.0);

	vec3 reflectViewPos = ScreenToViewSpace(vec3(screenPos.xy * screenPixelSize, GetDepthFix(ivec2(screenPos.xy))));
	float rDist = distance(reflectViewPos, viewPos);

	//reflection /= compositeOutputFactor;
	NetherFog(reflection, rDist);
	//reflection *= compositeOutputFactor;

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
		//vec3 reflectViewPos = ScreenToViewSpace(screenPos.xy, GetDepthFix(screenPos.xy));
		//float rDist = distance(reflectViewPos, viewPos);

		dist = saturate(max(rDist * 2.0, material.roughness * 3.0));
		//specular = mix(specular, 1.0, material.isMetal);
		//reflection *= saturate(specular);
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
