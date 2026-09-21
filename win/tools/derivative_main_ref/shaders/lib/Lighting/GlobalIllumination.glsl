
uniform sampler2D shadowtex1;
uniform sampler2D shadowcolor0;
uniform sampler2D shadowcolor1;

const int shadowMapResolution = 2048;  // Shadowmap resolution [1024 2048 4096 8192 16384 32768]

//----------------------------------------------------------------------------//

#include "ShadowDistortion.glsl"

vec3 WorldToShadowProjPos(in vec3 worldPos) {
	vec3 shadowPos = transMAD(shadowModelView, worldPos);
	return projMAD(shadowProjection, shadowPos);
}

vec2 DistortShadowProjPos(in vec2 shadowClipPos) {
	shadowClipPos.xy *= rcp(DistortionFactor(shadowClipPos.xy));

	return shadowClipPos * 0.5 + 0.5;
}

//----------------------------------------------------------------------------//

vec3 CalculateRSM(in vec3 viewPos, in vec3 worldNormal, in float dither) {
	vec3 total = vec3(0.0);

	const float realShadowMapRes = shadowMapResolution * MC_SHADOW_QUALITY;
	vec3 worldPos = transMAD(gbufferModelViewInverse, viewPos);
	vec3 shadowPos = WorldToShadowProjPos(worldPos);

	vec3 shadowNormal = mat3(shadowModelView) * worldNormal;
	shadowNormal.z = -shadowNormal.z;

	// float scale = GI_RADIUS * 0.1 * shadowProjection[0][0];
	const float scale = GI_RADIUS * rcp(realShadowMapRes);
	const float rRadius = 1.0 / GI_RADIUS;
	const float sqRadius = GI_RADIUS * GI_RADIUS;
	const float radiusAdd = sqrt(sqRadius / GI_SAMPLES);
	const float rSteps = 1.0 / GI_SAMPLES;

	float skyLightmap = texelFetch(colortex7, ivec2(gl_FragCoord.xy * 2), 0).g;
	const float goldenAngle = TAU / (PHI1 + 1.0);
	const mat2 goldenRotate = mat2(cos(goldenAngle), -sin(goldenAngle), sin(goldenAngle), cos(goldenAngle));

	vec2 rot = sincos(dither * 64.0/*  * goldenAngle */) * scale;
	dither *= rSteps;

	for (uint i = 0u; i < GI_SAMPLES; ++i, rot *= goldenRotate) {
		float fi 					= float(i) * rSteps + dither;

		vec2 coord 					= shadowPos.xy + rot * fi;
		ivec2 sampleTexel 			= ivec2(DistortShadowProjPos(coord) * realShadowMapRes);

		#if defined DISTANT_HORIZONS && defined DH_SHADOW
			float sampleDepth 		= texelFetch(shadowtex1, sampleTexel, 0).x * 40.0 - 20.0;
		#else
			float sampleDepth 		= texelFetch(shadowtex1, sampleTexel, 0).x * 10.0 - 5.0;
		#endif

		vec3 sampleVector 			= vec3(coord, sampleDepth) - shadowPos;

		float sampleDist 	 		= dotSelf(sampleVector);
		if (sampleDist > sqRadius) 	continue;

		// float sampleDist 			= length(sampleVector);
		// if (sampleDist > GI_RADIUS) continue;

		vec3 sampleDir 				= normalize(sampleVector);

		float diffuse 				= saturate(dot(shadowNormal, sampleDir));
		if (diffuse < 1e-5) 		continue;

		vec3 sampleColor 			= texelFetch(shadowcolor1, sampleTexel, 0).rgb;

		vec3 sampleNormal 			= DecodeNormal(sampleColor.xy);
		sampleNormal.xy 			= -sampleNormal.xy;

		float bounce 				= saturate(dot(sampleNormal, sampleDir));				
		if (bounce < 1e-5) 			continue;

		float falloff 	 			= rcp(sampleDist + radiusAdd);
		// float falloff 				= rcp(sqr(sampleDist * rPI * rPI) + rRadius) * rcp(sampleDist + 1.0);

		#if defined IS_OVERWORLD
			float skylightWeight 	= saturate(exp2(-sqr(sampleColor.z - skyLightmap)) * 2.5 - 1.5);
		#else
			float skylightWeight 	= 1.0;
		#endif

		// vec3 albedo 				= SRGBtoLinear(texelFetch(shadowcolor0, sampleTexel, 0).rgb);
		vec3 albedo 				= pow(texelFetch(shadowcolor0, sampleTexel, 0).rgb, vec3(2.2));

		total += albedo * fi * falloff * skylightWeight * bounce * diffuse;
	}

	return total * sqRadius * rSteps * 5e-2;	
}
