
#define PRECOMPUTED_ATMOSPHERIC_SCATTERING

out vec4 cloudData;

/* DRAWBUFFERS:2 */

//in vec2 screenCoord;

flat in vec3 sunIlluminance;
flat in vec3 moonIlluminance;
flat in vec3 skyIlluminance;

#include "/lib/Head/Common.inc"

//----------------------------------------------------------------------------//

uniform sampler2D noisetex;

uniform sampler3D colortex4;
uniform sampler2D colortex5;

uniform sampler2D depthtex0;

#if defined DISTANT_HORIZONS
	uniform sampler2D dhDepthTex0;
#endif

uniform float nightVision;
uniform float viewWidth;
uniform float viewHeight;
uniform float wetness;
uniform float eyeAltitude;
uniform float isLightningFlashing;
uniform float worldTimeCounter;

uniform vec3 cameraPosition;
uniform vec3 worldSunVector;
uniform vec3 worldLightVector;

uniform int frameCounter;
uniform int moonPhase;

uniform mat4 gbufferProjectionInverse;
uniform mat4 gbufferModelViewInverse;

uniform vec2 screenSize;
uniform vec2 screenPixelSize;

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#define GetDepth(texel) texelFetch(depthtex0, texel, 0).x

vec3 ScreenToViewSpaceRaw(in vec3 screenPos) {	
	vec3 NDCPos = screenPos * 2.0 - 1.0;
	vec3 viewPos = projMAD(gbufferProjectionInverse, NDCPos);
	viewPos /= gbufferProjectionInverse[2].w * NDCPos.z + gbufferProjectionInverse[3].w;

	return viewPos;
}

#include "/lib/Head/Noise.inc"

#include "/lib/Atmosphere/Atmosphere.glsl"

#include "/lib/Atmosphere/VolumetricClouds.glsl"

#include "/lib/Atmosphere/PlanarClouds.glsl"

#ifdef AURORA
	#include "/lib/Atmosphere/Aurora.glsl"
#endif

#if defined DISTANT_HORIZONS
	float depthMax4x4DH(in vec2 coord) {
		vec4 depthSamples0 = textureGather(dhDepthTex0, coord + vec2( 2.0,  2.0) * screenPixelSize);
		vec4 depthSamples1 = textureGather(dhDepthTex0, coord + vec2(-2.0,  2.0) * screenPixelSize);
		vec4 depthSamples2 = textureGather(dhDepthTex0, coord + vec2( 2.0, -2.0) * screenPixelSize);
		vec4 depthSamples3 = textureGather(dhDepthTex0, coord + vec2(-2.0, -2.0) * screenPixelSize);

		return max(
			max(maxOf(depthSamples0), maxOf(depthSamples1)),
			max(maxOf(depthSamples2), maxOf(depthSamples3))
		);
	}
#else
	float depthMax4x4(in vec2 coord) {
		vec4 depthSamples0 = textureGather(depthtex0, coord + vec2( 2.0,  2.0) * screenPixelSize);
		vec4 depthSamples1 = textureGather(depthtex0, coord + vec2(-2.0,  2.0) * screenPixelSize);
		vec4 depthSamples2 = textureGather(depthtex0, coord + vec2( 2.0, -2.0) * screenPixelSize);
		vec4 depthSamples3 = textureGather(depthtex0, coord + vec2(-2.0, -2.0) * screenPixelSize);

		return max(
			max(maxOf(depthSamples0), maxOf(depthSamples1)),
			max(maxOf(depthSamples2), maxOf(depthSamples3))
		);
	}
#endif

#if TEMPORAL_UPSCALING == 2
	const ivec2[4] checkerboardOffset = ivec2[4](
		ivec2(0, 0), ivec2(1, 1),
		ivec2(1, 0), ivec2(0, 1)
	);
#elif TEMPORAL_UPSCALING == 3
	const ivec2[9] checkerboardOffset = ivec2[9](
		ivec2(0, 0), ivec2(2, 0), ivec2(0, 2),
		ivec2(2, 2), ivec2(1, 1), ivec2(1, 0),
		ivec2(1, 2), ivec2(0, 1), ivec2(2, 1)
	);
#elif TEMPORAL_UPSCALING == 4
	const ivec2[16] checkerboardOffset = ivec2[16](
		ivec2(0, 0), ivec2(2, 0), ivec2(0, 2), ivec2(2, 2),
		ivec2(1, 1), ivec2(3, 1), ivec2(1, 3), ivec2(3, 3),
		ivec2(1, 0), ivec2(3, 0), ivec2(1, 2), ivec2(3, 2),
		ivec2(0, 1), ivec2(2, 1), ivec2(0, 3), ivec2(2, 3)
	);
#endif

const int cloudsRenderFactor = TEMPORAL_UPSCALING * TEMPORAL_UPSCALING;


void GetPlanetCurvePosition(inout vec3 p) {
    p.y = length(p + vec3(0.0, planetRadius, 0.0)) - planetRadius;
}

// float InterleavedGradientNoiseCheckerboard(in vec2 coord) {
// 	return fract(52.9829189 * fract(0.06711056 * coord.x + 0.00583715 * coord.y + 0.00623715 * ((frameCounter / cloudsRenderFactor) & 63)));
// }

vec4 RenderSkybox(in vec3 worldDir, in float dither/* , in float lightNoise */, in CloudProperties cloudProperties) {
	vec4 cloudsData = vec4(0.0, 0.0, 0.0, 1.0);
	// vec3 skyRadiance = textureBicubic(colortex5, ProjectSky(worldDir)).rgb;
	vec3 skyRadiance = texture(colortex5, ProjectSky(worldDir)).rgb;

	float LdotV = dot(worldDir, worldLightVector);

	vec4 phases;	/* forwardsLobe */										/* backwardsLobe */																	/* forwardsPeak */
	phases.x = 	HenyeyGreensteinPhase(LdotV, cloudForwardG) 	  * 0.7  + HenyeyGreensteinPhase(LdotV, cloudBackwardG)		  * cloudBackwardWeight  	  + CornetteShanksPhase(LdotV, 0.9) * cloudProperties.cloudPeakWeight;
	phases.y = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.7) * 0.35 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.7) * cloudBackwardWeight * 0.6 + CornetteShanksPhase(LdotV, 0.6) * cloudProperties.cloudPeakWeight * 0.5;
	phases.z = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.5) * 0.17 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.5) * cloudBackwardWeight * 0.3 + CornetteShanksPhase(LdotV, 0.4) * cloudProperties.cloudPeakWeight * 0.2;
	phases.w = 	HenyeyGreensteinPhase(LdotV, cloudForwardG * 0.3) * 0.08 + HenyeyGreensteinPhase(LdotV, cloudBackwardG * 0.3) * cloudBackwardWeight * 0.2 + CornetteShanksPhase(LdotV, 0.2) * cloudProperties.cloudPeakWeight * 0.1;
	//phases.x = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9), vec3(0.7, cloudBackwardWeight, cloudPeakWeight));
	//phases.y = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.7, vec3(0.35, cloudBackwardWeight * 0.6, cloudPeakWeight * 0.5));
	//phases.z = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.5, vec3(0.17, cloudBackwardWeight * 0.3, cloudPeakWeight * 0.2));
	//phases.w = MiePhaseClouds(LdotV, vec3(cloudForwardG, cloudBackwardG, 0.9) * 0.3, vec3(0.08, cloudBackwardWeight * 0.15, cloudPeakWeight * 0.1));

	vec3 planeOrigin = vec3(0.0, planetRadius + eyeAltitude, 0.0);
    bool intersectsGround = RaySphereIntersection(planeOrigin, worldDir, planetRadius).y >= 0.0;

	#ifdef VOLUMETRIC_CLOUDS
		if ((worldDir.y > 0.0 && eyeAltitude < cloudProperties.altitude)
		|| (intersectsGround && eyeAltitude > cloudProperties.maxAltitude)) {

			vec2 bottomIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.altitude);
			vec2 topIntersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + cloudProperties.maxAltitude);

			float startLength, endLength;
			if (eyeAltitude > cloudProperties.maxAltitude) {
				startLength = topIntersection.x;
				endLength = bottomIntersection.x;
			} else {
				startLength = bottomIntersection.y;
				endLength = topIntersection.y;
			}

			// The range of eye in cloudsData
			float rayRange = oneMinus(saturate((eyeAltitude - cloudProperties.maxAltitude) * 0.1)) *
							oneMinus(saturate((cloudProperties.altitude - eyeAltitude) * 0.1));

			// The ray distance in range
			float rayDist = bottomIntersection.y >= 0.0 && eyeAltitude > cloudProperties.altitude ? bottomIntersection.x : topIntersection.y;
			//rayDist = min(rayDist, cloudProperties.altitude * 10.0);

			startLength *= oneMinus(rayRange);
			endLength = mix(endLength, rayDist, rayRange);

			uint raySteps = CLOUD_CUMULUS_SAMPLES;
			raySteps = uint(mix(raySteps, uint(raySteps * 0.6), abs(worldDir.y))); // Steps Fade

			float rayLength = clamp(endLength - startLength, 0.0, 2e4) * rcp(raySteps);
			vec3 rayStep = rayLength * worldDir;
            GetPlanetCurvePosition(rayStep);
			vec3 rayPos = (startLength + rayLength * dither) * worldDir + cameraPosition;
            GetPlanetCurvePosition(rayPos);

			float noiseDetail = GetNoiseDetail(worldDir);
			//float lightNoise = InterleavedGradientNoise();

			//uint raySteps = max(uint(CLOUD_CUMULUS_SAMPLES - sqrt(dist) * 0.06), CLOUD_CUMULUS_SAMPLES / 2);
			//uint raySteps = uint(CLOUD_CUMULUS_SAMPLES);

			float scatteringSun = 0.0;
			float scatteringSky = 0.0;

			//float powderIntensity = saturate(CornetteShanksPhase(LdotV, 0.5));
			//float powderIntensity = 0.8 * sqr(dot(worldDir, worldLightVector) * 0.5 + 0.5);
			float transmittance = 1.0;

			for (uint i = 0u; i < raySteps; ++i, rayPos += rayStep) {
				if (transmittance < minTransmittance) break;
				if (rayPos.y < cloudProperties.altitude || rayPos.y > cloudProperties.maxAltitude) continue;

				float dist = distance(rayPos, cameraPosition);
				if (dist > planetRadius + cloudProperties.maxAltitude) continue;

				float density = CloudVolumeDensity(cloudProperties, rayPos, 5u, mix(noiseDetail, 1.0, exp2(-dist * 0.001f)));
				if (density < 1e-4) continue;

				vec2 lightNoise = hash2(fract(rayPos)) * 0.4 + 0.3;

				float sunlightOD = CloudVolumeSunLightOD(cloudProperties, rayPos, lightNoise.x);

				//float powder = 1.0 - fastExp(-density * 32.0);
				//powder = powder * oneMinus(powderIntensity) + powderIntensity;
				float powder = oneMinus(fastExp(-density * 36.0)) * 0.82;
				powder /= 1.0 - powder;

				float sunlightEnergy = 	fastExp(-sunlightOD * 2.0) * phases.x;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.8) * phases.y;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.3) * phases.z;
				sunlightEnergy += 		fastExp(-sunlightOD * 0.1) * phases.w;
				// float sunlightEnergy = 	rcp(sunlightOD * 2.0 + 1.0) * phases.x;
				// sunlightEnergy += 		rcp(sunlightOD * 0.9 + 1.0) * phases.y;
				// sunlightEnergy += 		rcp(sunlightOD * 0.4 + 1.0) * phases.z;
				// sunlightEnergy += 		rcp(sunlightOD * 0.2 + 1.0) * phases.w;

				float skylightEnergy = CloudVolumeSkyLightOD(cloudProperties, rayPos, lightNoise.y);
				skylightEnergy = fastExp(-skylightEnergy) + fastExp(-skylightEnergy * 0.1) * 0.1;

				float stepTransmittance = fastExp(-density * 0.12 * rayLength);
				float cloudsTemp = powder * transmittance * oneMinus(stepTransmittance);
				scatteringSun += sunlightEnergy * cloudsTemp;
				scatteringSky += skylightEnergy * cloudsTemp;
				transmittance *= stepTransmittance;	
			}

			if (transmittance < 1.0 - minTransmittance) {
				bool moonlit = worldSunVector.y < -0.04;
				vec3 scattering = scatteringSun * 22.0 * cloudProperties.sunlighting * (moonlit ? moonIlluminance : sunIlluminance);
				scattering += scatteringSky * 0.15 * cloudProperties.skylighting * skyIlluminance;

				// lightning
				if (isLightningFlashing > 1e-2) scattering += sqr(scatteringSky) * 0.1 * lightningColor;

				#ifdef AURORA
					scattering += scatteringSky * vec3(0.0, 0.005, 0.0025) * auroraAmount;
				#endif

				rayPos -= cameraPosition;
				#ifdef FULL_AERIAL_PERSPECTIVE
					vec3 airTransmittance;
					vec3 aerialPerspective = 20.0 * GetSkyRadianceToPoint(atmosphereModel, rayPos, worldSunVector, airTransmittance);
					scattering *= airTransmittance;
					scattering += aerialPerspective * oneMinus(transmittance);
					float atmosFade = fastExp(-length(rayPos) * (0.1 + 0.1 * wetness) * 1e-4);
				#else
					// if (dotSelf(planeOrigin) < dotSelf(rayPos)) {
					// 	vec3 trans0 = GetTransmittance(planeOrigin, worldDir);
					// 	vec3 trans1 = GetTransmittance(rayPos,    	worldDir);

					// 	airTransmittance = saturate(trans0 / trans1);
					// } else {
					// 	vec3 trans0 = GetTransmittance(planeOrigin, -worldDir);
					// 	vec3 trans1 = GetTransmittance(rayPos,    	-worldDir);

					// 	airTransmittance = saturate(trans1 / trans0);
					// }

					// scattering *= airTransmittance;
					// scattering += skyRadiance * oneMinus(transmittance) * oneMinus(airTransmittance);

					float atmosFade = fastExp(-length(rayPos) * (0.2 + 0.1 * wetness) * 1e-4);
				#endif
				scattering = scattering * atmosFade + skyRadiance * oneMinus(transmittance) * oneMinus(atmosFade);

				cloudsData = vec4(scattering, transmittance);
			}
		}
	#endif

	#ifdef PLANAR_CLOUDS
		//vec4 cloudsTemp = PlanarClouds(worldDir, dither, phases, transmittanceTemp);

		if ((worldDir.y > 0.0 && eyeAltitude < CLOUD_PLANE_ALTITUDE)
		|| (intersectsGround && eyeAltitude > CLOUD_PLANE_ALTITUDE)) {
			vec2 intersection = RaySphereIntersection(planeOrigin, worldDir, planetRadius + CLOUD_PLANE_ALTITUDE);
			float cloudDistance = eyeAltitude > CLOUD_PLANE_ALTITUDE ? intersection.x : intersection.y;

			if (cloudDistance > 0.0 && cloudDistance < planetRadius + CLOUD_PLANE_ALTITUDE) {
				vec3 cloudPos = worldDir * cloudDistance;

				#ifdef FULL_AERIAL_PERSPECTIVE
					vec3 airTransmittance;
					vec3 aerialPerspective = 20.0 * GetSkyRadianceToPoint(atmosphereModel, cloudPos, worldSunVector, airTransmittance);
				// #else
				// 	if (dotSelf(planeOrigin) < dotSelf(cloudPos)) {
				// 		vec3 trans0 = GetTransmittance(planeOrigin, worldDir);
				// 		vec3 trans1 = GetTransmittance(cloudPos,    worldDir);

				// 		airTransmittance = saturate(trans0 / trans1);
				// 	} else {
				// 		vec3 trans0 = GetTransmittance(planeOrigin, -worldDir);
				// 		vec3 trans1 = GetTransmittance(cloudPos,    -worldDir);

				// 		airTransmittance = saturate(trans1 / trans0);
				// 	}
				#endif
				cloudPos += cameraPosition;

				vec4 cloudsTemp = vec4(0.0, 0.0, 0.0, 1.0);

				#ifdef CIRROCUMULUS_CLOUDS
					#ifdef CLOUDS_WEATHER
						if (cloudDynamicWeather.x < 0.4)
					#endif
					{
						vec4 sampleTemp = PlanarSample1(cloudDistance, cloudPos.xz, LdotV, dither, phases, worldDir);

						if (sampleTemp.a > minTransmittance) {
							#ifdef FULL_AERIAL_PERSPECTIVE
								float atmosFade = fastExp(-cloudDistance * fma(0.025, wetness, 0.05) * 0.00015);
								sampleTemp.rgb *= airTransmittance;
								sampleTemp.rgb += aerialPerspective * sampleTemp.a;
							#else
								float atmosFade = fastExp(-cloudDistance * fma(0.05, wetness, 0.1) * 0.00015);
							// 	sampleTemp.rgb += skyRadiance * sampleTemp.a * oneMinus(airTransmittance);
							#endif
							sampleTemp.rgb = sampleTemp.rgb * atmosFade + skyRadiance * sampleTemp.a * oneMinus(atmosFade);
						}

						cloudsTemp.rgb = sampleTemp.rgb;
						cloudsTemp.a -= sampleTemp.a;
					}
				#endif
				#if CIRRUS_CLOUDS > 0
					#ifdef CLOUDS_WEATHER
						if (cloudDynamicWeather.y < 0.5)
					#endif
					{
						vec4 sampleTemp = PlanarSample0(cloudDistance, cloudPos.xz, LdotV);

						if (sampleTemp.a > minTransmittance) {
							#ifdef FULL_AERIAL_PERSPECTIVE
								float atmosFade = fastExp(-cloudDistance * fma(0.025, wetness, 0.05) * 0.00015);
								sampleTemp.rgb *= airTransmittance;
								sampleTemp.rgb += aerialPerspective * sampleTemp.a;
							#else
								float atmosFade = fastExp(-cloudDistance * fma(0.05, wetness, 0.1) * 0.00015);
							// 	sampleTemp.rgb += skyRadiance * sampleTemp.a * oneMinus(airTransmittance);
							#endif
							sampleTemp.rgb = sampleTemp.rgb * atmosFade + skyRadiance * sampleTemp.a * oneMinus(atmosFade);
						}

						cloudsTemp.rgb += sampleTemp.rgb * cloudsTemp.a;
						cloudsTemp.a *= 1.0 - sampleTemp.a;
					}
				#endif

				if (eyeAltitude < CLOUD_PLANE_ALTITUDE) {
					cloudsData.rgb += cloudsTemp.rgb * cloudsData.a;
				} else {
					cloudsData.rgb = cloudsData.rgb * cloudsTemp.a + cloudsTemp.rgb;
				}

				cloudsData.a *= cloudsTemp.a;
			}
		}
	#endif

	vec4 outData;
	outData.rgb = /* skyRadiance * cloudsData.a +  */cloudsData.rgb;

	#ifdef AURORA
		if (auroraAmount > 1e-2) outData.rgb += NightAurora(worldDir) * cloudsData.a;
	#endif

	outData.a = remap(minTransmittance, 1.0, cloudsData.a);

	return clamp16F(outData);
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	ivec2 texel = ivec2(gl_FragCoord.xy);

	ivec2 cloudTexel = texel * TEMPORAL_UPSCALING + checkerboardOffset[frameCounter % cloudsRenderFactor];
	vec2 coord = cloudTexel * screenPixelSize;

	// Current clouds
	#if defined DISTANT_HORIZONS
		if (min(GetDepth(RawCoord(coord)), depthMax4x4DH(coord)) >= 1.0) {
	#else
		if (depthMax4x4(coord) >= 1.0) {
	#endif
		vec3 viewPos  = ScreenToViewSpaceRaw(vec3(coord, 1.0));
		vec3 worldDir = mat3(gbufferModelViewInverse) * normalize(viewPos);

		float dither = R1(frameCounter / cloudsRenderFactor, texelFetch(noisetex, cloudTexel & 255, 0).a);
		// float lightNoise = InterleavedGradientNoiseCheckerboard(cloudTexel);

		cloudData = RenderSkybox(worldDir, dither/* , lightNoise */, GetGlobalCloudProperties());
	}
}
