
layout(location = 0) out vec3 colortex7Out;
layout(location = 1) out vec4 reflectionData;
layout(location = 2) out vec4 colortex3Out;

/* DRAWBUFFERS:723 */

#include "/lib/Head/Common.inc"

uniform sampler2D tex;
#ifdef MC_NORMAL_MAP
    uniform sampler2D normals;
#endif

#include "/lib/Head/Uniforms.inc"

in vec2 texcoord;
in vec2 lightmap;

in vec4 tint;
in vec4 viewPos;
//in vec3 worldNormal;

flat in mat3 tbnMatrix;

#include "/lib/Atmosphere/Atmosphere.glsl"

//#include "/lib/Surface/ManualTBN.glsl"

#include "/lib/Surface/RainEffect.glsl"

#include "/lib/Head/Functions.inc"

#include "/lib/Head/Material.inc"

#include "/lib/Surface/ScreenSpaceReflections.glsl"

vec4 CalculateSpecularReflections(in vec3 normal, in float skylight, in vec3 viewPos) {
	skylight = smoothstep(0.3, 0.8, skylight);
	vec3 viewDir = normalize(viewPos);

	vec3 rayDir = reflect(viewDir, normal);

	float NdotL = dot(normal, rayDir);
	if (NdotL < 1e-6) return vec4(0.0);

	//float dither = BlueNoiseTemporal(0.447213595);
	float dither = InterleavedGradientNoiseTemporal(gl_FragCoord.xy);
	vec3 screenPos = vec3(gl_FragCoord.xy * screenPixelSize, gl_FragCoord.z);
	
	float NdotV = max(1e-6, dot(normal, -viewDir));
	//#ifdef HQ_TRACING
		bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, RAYTRACE_SAMPLES, screenPos);
	//#else
	//	bool hit = ScreenSpaceRayTrace(viewPos, rayDir, dither, RAYTRACE_SAMPLES, screenPos);
	//#endif

	vec3 reflection = vec3(0.0);

	if (hit) {
		reflection = texelFetch(colortex4, ivec2(screenPos.xy), 0).rgb;
	}
	#if defined IS_OVERWORLD
		else if (skylight > 1e-3) {
			vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
			float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
			//float tileSize = floor(min(viewWidth * rcp(3.0), viewHeight * 0.5)) * 0.25;
			vec4 skyboxData = textureBicubic(colortex5, ProjectSky(rayDirWorld) + vec2(0.0, skyCaptureRes.y * screenPixelSize.y));

			// vec3 sunmoon = RenderSun(rayDirWorld, worldSunVector);
			// sunmoon += RenderMoonReflection(rayDirWorld, worldSunVector);

			// skyboxData.rgb += sunmoon * skyboxData.a * AtmosphereAbsorption(rayDirWorld, AtmosphereExtent);
			reflection = skyboxData.rgb * skylight * NdotU;
		}
	#elif defined IS_END
		else {
			vec3 rayDirWorld = mat3(gbufferModelViewInverse) * rayDir;
			float NdotU = saturate((dot(normal, gbufferModelView[1].xyz) + 0.7) * 2.0) * 0.75 + 0.25;
			#ifdef DARK_END
				bool darkEnd = bossBattle == 2 || bossBattle == 3;
			#else
				const bool darkEnd = false;
			#endif
			vec3 sunDisc = RenderSun(rayDirWorld, worldSunVector);
			sunDisc *= vec3(0.99, 0.93, 0.65) * 0.1;
			reflection = mix(vec3(0.396, 0.352, 0.108), vec3(0.04, 0.02, 0.05), float(darkEnd) * 0.9) * 2.0 * exp2(-max0(rayDirWorld.y) * 1.5);	
			if (!darkEnd) reflection += sunDisc;
			reflection *= NdotU;
		}
	#endif

	float specular;
	if (isEyeInWater == 1) { // 全反射
		specular = FresnelDielectricN(NdotV, 1.000293 / WATER_REFRACT_IOR);
	}else{
		specular = FresnelDielectricN(NdotV, GLASS_REFRACT_IOR);
	}

	return clamp16F(vec4(reflection * specular, specular));
}

void main() {
	vec4 albedo = texture(tex, texcoord) * tint;

    //mat3 tbnMatrix = manualTBN(viewPos.xyz, texcoord);

    #ifdef MC_NORMAL_MAP
        vec3 normalData = texture(normals, texcoord).rgb;
        DecodeNormalTex(normalData);
    #else
        vec3 normalData = vec3(0.0, 0.0, 1.0);
    #endif

	normalData = normalize(tbnMatrix * normalData);

	colortex7Out.xy = lightmap + (bayer4(gl_FragCoord.xy) - 0.5) * rcp(255.0);
	colortex7Out.z = 16.1 / 255.0;

	colortex3Out.xy = EncodeNormal(normalData);
	colortex3Out.z = PackUnorm2x8(albedo.rg);
	colortex3Out.w = PackUnorm2x8(albedo.ba);

	reflectionData = CalculateSpecularReflections(normalData, cube(lightmap.g), viewPos.xyz);
}
