
in vec2 screenCoord;

#include "/lib/Head/Common.inc"
#include "/lib/Head/Uniforms.inc"

uniform bool worldTimeChanged;

//----------------------------------------------------------------------------//

layout(location = 0) out vec4 indirectData;
#if defined IS_OVERWORLD
	layout(location = 1) out vec4 skyboxData;
	/* DRAWBUFFERS:01 */

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

	vec4 textureCatmullRom(in sampler2D tex, in vec2 coord) {
		vec2 res = textureSize(tex, 0);
		vec2 screenPixelSize = 1.0 / res;

		vec2 position = coord * res;
		vec2 centerPosition = floor(position - 0.5) + 0.5;

		vec2 f = position - centerPosition;

		vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
		vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
		vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
		vec2 w3 = f * f * (-0.5 + 0.5 * f);

		vec2 w12 = w1 + w2;

		vec2 tc0 = screenPixelSize * (centerPosition - 1.0);
		vec2 tc3 = screenPixelSize * (centerPosition + 2.0);
		vec2 tc12 = screenPixelSize * (centerPosition + w2 * rcp(w12));

		vec4 color = vec4(0.0);
		color += textureLod(tex, vec2(tc0.x, tc0.y), 0) * w0.x * w0.y;
		color += textureLod(tex, vec2(tc12.x, tc0.y), 0) * w12.x * w0.y;
		color += textureLod(tex, vec2(tc3.x, tc0.y), 0) * w3.x * w0.y;

		color += textureLod(tex, vec2(tc0.x, tc12.y), 0) * w0.x * w12.y;
		color += textureLod(tex, vec2(tc12.x, tc12.y), 0) * w12.x * w12.y;
		color += textureLod(tex, vec2(tc3.x, tc12.y), 0) * w3.x * w12.y;

		color += textureLod(tex, vec2(tc0.x, tc3.y), 0) * w0.x * w3.y;
		color += textureLod(tex, vec2(tc12.x, tc3.y), 0) * w12.x * w3.y;
		color += textureLod(tex, vec2(tc3.x, tc3.y), 0) * w3.x * w3.y;

		return color;
	}
#else
	/* DRAWBUFFERS:05 */
#endif

//----// FUNCTIONS //-----------------------------------------------------------------------------//

#include "/lib/Head/Functions.inc"

#ifdef SSAO_ENABLED
	#include "/lib/Lighting/AmbientOcclusion.glsl"
#endif

#ifdef GI_ENABLED
	#include "/lib/Lighting/GlobalIllumination.glsl"
#endif

vec4 TemporalLightAccumulation() {
	if (all(lessThan(screenCoord, vec2(0.5)))) {
		ivec2 texel = ivec2(gl_FragCoord.xy) * 2;
		float depth = GetDepth(texel);

		#if defined DISTANT_HORIZONS
			float dhDepth = GetDepthDH(texel);
			if (min(dhDepth, depth) >= 1.0) return vec4(0.0);
		#else
			if (depth >= 1.0) return vec4(0.0);
		#endif

		vec2 coord = screenCoord * 2.0;
		//depth += 0.38 * step(depth, 0.56);
		vec3 screenPos = vec3(coord, depth);
		vec3 viewPos = ScreenToViewSpace(screenPos);

		#if defined DISTANT_HORIZONS
			if (depth >= 1.0) {
				screenPos = vec3(coord, dhDepth);
				viewPos = ScreenToViewSpaceDH(screenPos);
			}
		#endif

		vec3 normal = GetNormals(texel);
		vec3 worldNormal = mat3(gbufferModelViewInverse) * normal;

		//float dither = R1(frameCounter, texelFetch(noisetex, ivec2(gl_FragCoord.xy / 0.5) & 255, 0).a);
		// float dither = RandNextF();

		vec4 currLight = vec4(vec3(0.0), 1.0);

		//vec2 velocity = (depth < 0.56) ? vec2(0.0) : CalculateCameraVelocity(coord, depth);

		#ifdef SSAO_ENABLED
			float dither = texelFetch(noisetex, texel & 255, 0).a;
			dither = fract(dither + frameCounter * rPI);
			#if defined DISTANT_HORIZONS
				if (depth >= 1.0) currLight.a = SpiralAO_DH(coord, viewPos, normal, dither);
				else 
			#endif
			if (depth > 0.56) currLight.a = SpiralAO(coord, viewPos, normal, dither);
			// if (depth > 0.56) currLight.a = GetSSAO(viewPos, normal, dither);
		#endif

		#ifdef GI_ENABLED
			currLight.rgb = CalculateRSM(viewPos, worldNormal, RandNextF());
		#endif

		vec2 prevCoord = Reproject(screenPos).xy;

		#if defined DISTANT_HORIZONS
			if (depth >= 1.0) prevCoord = ReprojectDH(screenPos).xy;
		#endif
		if (clamp(prevCoord, screenPixelSize, 1.0 - screenPixelSize) != prevCoord || worldTimeChanged) return currLight;

		prevCoord *= 0.5;

		vec4 prevData = texture(colortex0, prevCoord + vec2(0.5, 0.0));

		float currDist = GetDepthLinear(depth);

		//float NdotV = saturate(dot(worldNormal, -normalize(mat3(gbufferModelViewInverse) * viewPos)));

		float cameraMovement = distance(cameraPosition, previousCameraPosition);

		// float frameIndex = min(texture(colortex0, prevCoord + 0.5).x, 32.0);
		// frameIndex *= step((distance(currDist, prevData.a) - cameraMovement) / abs(currDist), 0.1);
		// frameIndex *= step(0.5, dot(normal, prevData.xyz));

		// vec4 blendWeight = vec4(frameIndex) * rcp(frameIndex + 1.0);
		float weight = step((distance(currDist, prevData.a) - cameraMovement) / abs(currDist), 0.1);
		weight *= step(0.5, dot(normal, prevData.xyz));

		vec4 blendWeight = vec4(vec3(0.97), 0.75) * weight;

		vec4 prevLight = texture(colortex0, prevCoord);

		return clamp16F(mix(currLight, prevLight, blendWeight));
		// return clamp16F(mix(currLight, prevLight, 0.95 * weight));
	} else if (screenCoord.y < 0.5) {
		ivec2 texel = ivec2(gl_FragCoord.xy) * 2 - ivec2(viewWidth, 0);
		float depth = GetDepth(texel);

		#if defined DISTANT_HORIZONS
			float dhDepth = GetDepthDH(texel);
			if (min(dhDepth, depth) >= 1.0) return vec4(0.0);

			float dist = depth < 1.0 ? GetDepthLinear(depth) : GetDepthLinearDH(dhDepth);
		#else
			if (depth >= 1.0) return vec4(0.0);
			float dist = GetDepthLinear(depth);
		#endif

		//depth += 0.38 * step(depth, 0.56);

		vec3 normal = GetNormals(texel);

		return vec4(normal, dist);
	}

	//return texelFetch(colortex0, ivec2(gl_FragCoord.xy), 0);
}

//----// MAIN //----------------------------------------------------------------------------------//
void main() {
	#if defined GI_ENABLED || defined SSAO_ENABLED
		indirectData = TemporalLightAccumulation();
	#else
		indirectData = vec4(vec3(0.0), 1.0);
	#endif

	#if defined IS_OVERWORLD
		#if defined DISTANT_HORIZONS
			#define Reproject ReprojectDH
		#endif

		ivec2 texel = ivec2(gl_FragCoord.xy);

		vec2 previousCoord = Reproject(vec3(screenCoord, 1.0)).xy;
		vec2 historyData = texture(colortex0, previousCoord * 0.5 + 0.5).xy;

		if (all(greaterThanEqual(screenCoord, vec2(0.5)))) {
			// Store frame index
			indirectData.x = 1.0 + texture(colortex0, Reproject(vec3(screenCoord * 2.0 - 1.0, 1.0)).xy * 0.5 + 0.5).x;
			// Store reversed depth
			ivec2 texel = ivec2(gl_FragCoord.xy) * 2 - ivec2(screenSize);
        	// depth = GetDepthDH(texel);
			indirectData.y = 1.0 - GetDepth(texel);
		}

		// float depth = texelFetch(depthtex0, texel, 0).x;
		// depth = GetDepthDH(texel);

		if (saturate(previousCoord) != previousCoord
		 || GetDepth(texel) >= 1.0 && historyData.y > 1e-6
		 || worldTimeChanged) {
			skyboxData = textureBicubic(colortex2, min(screenCoord * rcp(float(TEMPORAL_UPSCALING)), rcp(TEMPORAL_UPSCALING) - screenPixelSize));
			// skyboxData = texelFetch(colortex2, clamp(texel / TEMPORAL_UPSCALING, ivec2(0), ivec2(screenSize) / TEMPORAL_UPSCALING - 1), 0);
		} else {
			const int cloudsRenderFactor = TEMPORAL_UPSCALING * TEMPORAL_UPSCALING;

			ivec2 offset = checkerboardOffset[frameCounter % cloudsRenderFactor];
			ivec2 currentTexel = clamp((texel - offset) / TEMPORAL_UPSCALING, ivec2(0), ivec2(screenSize) / TEMPORAL_UPSCALING - 1);

			// float pixelDistance = dotSelf(offset) * TEMPORAL_UPSCALING;
			// float confidence    = fastExp(-pixelDistance * 0.025 * exp2(2.*historyData.x));
			
			// float finalBlend = saturate(historyData.x / (++historyData.x));
			// finalBlend *= confidence;

			float cameraMovement = fastExp(-16.0 * distance(cameraPosition, previousCameraPosition));
			historyData.x = min(historyData.x, MAX_BLENDED_FRAMES/*  * step(eyeAltitude, 1e6) */);
			float blendWeight = 1.0 - rcp(max(historyData.x - cloudsRenderFactor, 1.0));
			blendWeight *= mix(1.0, cameraMovement, blendWeight);
			vec2 pixelVelocity = 1.0 - abs(fract(previousCoord * screenSize) * 2.0 - 1.0);
			blendWeight *= sqrt(pixelVelocity.x * pixelVelocity.y) * 0.75 + 0.25;
			blendWeight *= mix(1.0, sqrt(pixelVelocity.x * pixelVelocity.y), blendWeight);

			vec4 prevData = clamp16F(textureCatmullRom(colortex1, previousCoord));
			// vec4 prevData = clamp16F(textureSmoothFilter(colortex1, previousCoord));
			skyboxData = texelFetch(colortex2, currentTexel, 0);
			if (offset == texel % TEMPORAL_UPSCALING) {
				skyboxData = mix(skyboxData, prevData, blendWeight);
			//} else skyboxData = mix(textureBicubicLod(colortex1, previousCoord, TEMPORAL_UPSCALING - 1), prevData, saturate(historyData.x / (++historyData.x)));
			} else skyboxData = prevData;
		}
	#endif
}
