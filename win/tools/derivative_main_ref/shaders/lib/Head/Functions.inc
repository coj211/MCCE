
#include "Noise.inc"

void GetBlocklightFalloff(inout float blocklight) {
	blocklight = rcp(sqr(16.0 - 15.0 * blocklight)) + sqr(blocklight) * 0.05;
	blocklight = remap(rcp(sqr(16.0)), 1.0, blocklight);
}

vec3 ScreenToViewSpaceRaw(in vec3 screenPos) {	
	vec3 NDCPos = screenPos * 2.0 - 1.0;
	vec3 viewPos = projMAD(gbufferProjectionInverse, NDCPos);
	viewPos /= gbufferProjectionInverse[2].w * NDCPos.z + gbufferProjectionInverse[3].w;

	return viewPos;
}

#if defined DISTANT_HORIZONS
	vec3 ScreenToViewSpaceRawDH(in vec3 screenPos) {	
		vec3 NDCPos = screenPos * 2.0 - 1.0;
		vec3 viewPos = projMAD(dhProjectionInverse, NDCPos);
		viewPos /= dhProjectionInverse[2].w * NDCPos.z + dhProjectionInverse[3].w;

		return viewPos;
	}
#endif

vec3 ScreenToViewSpace(in vec3 screenPos) {
	vec3 NDCPos = screenPos * 2.0 - 1.0;
	#ifdef TAA_ENABLED
		NDCPos.xy -= taaOffset;
	#endif
	vec3 viewPos = projMAD(gbufferProjectionInverse, NDCPos);
	viewPos /= gbufferProjectionInverse[2].w * NDCPos.z + gbufferProjectionInverse[3].w;

	return viewPos;
}

#if defined DISTANT_HORIZONS
	vec3 ScreenToViewSpaceDH(in vec3 screenPos) {
		vec3 NDCPos = screenPos * 2.0 - 1.0;
		#ifdef TAA_ENABLED
			NDCPos.xy -= taaOffset;
		#endif
		vec3 viewPos = projMAD(dhProjectionInverse, NDCPos);
		viewPos /= dhProjectionInverse[2].w * NDCPos.z + dhProjectionInverse[3].w;

		return viewPos;
	}
#endif

vec3 ScreenToViewSpace(in vec2 coord) {
	vec3 NDCPos = vec3(coord, texture(depthtex0, coord).x) * 2.0 - 1.0;
	#ifdef TAA_ENABLED
		NDCPos.xy -= taaOffset;
	#endif
	vec3 viewPos = projMAD(gbufferProjectionInverse, NDCPos);
	viewPos /= gbufferProjectionInverse[2].w * NDCPos.z + gbufferProjectionInverse[3].w;

	return viewPos;
}

#if defined DISTANT_HORIZONS
	vec3 ScreenToViewSpaceDH(in vec2 coord) {
		vec3 NDCPos = vec3(coord, texture(dhDepthTex0, coord).x) * 2.0 - 1.0;
		#ifdef TAA_ENABLED
			NDCPos.xy -= taaOffset;
		#endif
		vec3 viewPos = projMAD(dhProjectionInverse, NDCPos);
		viewPos /= dhProjectionInverse[2].w * NDCPos.z + dhProjectionInverse[3].w;

		return viewPos;
	}
#endif

//float ScreenToViewSpace(in float depth) {
//    depth = depth * 2.0 - 1.0;
//    return 1.0 / (depth * gbufferProjectionInverse[2][3] + gbufferProjectionInverse[3][3]);
//}

//float ScreenToViewSpaceDH(in float depth) {
//    depth = depth * 2.0 - 1.0;
//    return 1.0 / (depth * dhProjectionInverse[2][3] + dhProjectionInverse[3][3]);
//}

vec3 ViewToScreenSpaceRaw(in vec3 viewPos) {
	vec3 NDCPos = projMAD(gbufferProjection, viewPos) / -viewPos.z;

	return NDCPos * 0.5 + 0.5;
}

vec3 ViewToScreenSpace(in vec3 viewPos) {
	vec3 NDCPos = projMAD(gbufferProjection, viewPos) / -viewPos.z;
	#ifdef TAA_ENABLED
		NDCPos.xy += taaOffset;
	#endif
	return NDCPos * 0.5 + 0.5;
}

#if defined DISTANT_HORIZONS
	vec3 ViewToScreenSpaceDH(in vec3 viewPos) {
		vec3 NDCPos = projMAD(dhProjection, viewPos) / -viewPos.z;
		#ifdef TAA_ENABLED
			NDCPos.xy += taaOffset;
		#endif
		return NDCPos * 0.5 + 0.5;
	}
#endif

vec3 Reproject(in vec3 screenPos) {
	vec3 position = ScreenToViewSpaceRaw(screenPos);
    position = transMAD(gbufferModelViewInverse, position);
	if (screenPos.z > 0.56) position += cameraPosition - previousCameraPosition;
    position = transMAD(gbufferPreviousModelView, position);
	position = projMAD(gbufferPreviousProjection, position) / -position.z;
    return position * 0.5 + 0.5;
}

#if defined DISTANT_HORIZONS
	vec3 ReprojectDH(in vec3 screenPos) {
		vec3 position = ScreenToViewSpaceRawDH(screenPos);
		position = transMAD(gbufferModelViewInverse, position);
		position += cameraPosition - previousCameraPosition;
		position = transMAD(gbufferPreviousModelView, position);
		position = projMAD(dhPreviousProjection, position) / -position.z;
		return position * 0.5 + 0.5;
	}

	float GetDepthLinearDH(in float depth) {
		return (dhNearPlane * dhFarPlane) / (depth * (dhNearPlane - dhFarPlane) + dhFarPlane);
	}

	#define GetDepthDH(texel) texelFetch(dhDepthTex0, texel, 0).x
	#define GetDepthSoildDH(texel) texelFetch(dhDepthTex1, texel, 0).x
#endif

float GetDepthScreen(in float depthLinear) {
	return (far + near) / (far - near) + (2.0 * far * near) / (depthLinear * (far - near));
}

#define GetDepth(texel) texelFetch(depthtex0, texel, 0).x
#define GetDepthSoild(texel) texelFetch(depthtex1, texel, 0).x

float GetDepthFix(in vec2 coord) {
	float depth = texture(depthtex0, coord).x;
	return depth + 0.38 * step(depth, 0.56);
}

float GetDepthFix(in ivec2 texel) {
	float depth = texelFetch(depthtex0, texel, 0).x;
	return depth + 0.38 * step(depth, 0.56);
}

float GetDepthLinear(in ivec2 texel) {
    return (near * far) / (GetDepthSoild(texel) * (near - far) + far);
}

float GetDepthLinear(in float depth) {
    return (near * far) / (depth * (near - far) + far);
}

vec3 GetNormals(in vec2 coord) {
	return DecodeNormal(texture(colortex3, coord).xy);
}

vec3 GetNormals(in ivec2 texel) {
	return DecodeNormal(texelFetch(colortex3, texel, 0).xy);
}

const ivec2 offset3x3[9] = ivec2[9](
    ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1),
    ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0),
    ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1)
);

const ivec2 offset3x3N[8] = ivec2[8](
    ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1),
    ivec2(-1,  0), 				 ivec2(1,  0),
    ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1)
);

const ivec2 offset4x4[16] = ivec2[16](
	ivec2(-2, -2), ivec2(-1, -2), ivec2(1, -2), ivec2(2, -2),
	ivec2(-2, -1), ivec2(-1, -1), ivec2(1, -1), ivec2(2, -1),
	ivec2(-2,  1), ivec2(-1,  1), ivec2(1,  1), ivec2(2,  1), 
	ivec2(-2,  2), ivec2(-1,  2), ivec2(1,  2), ivec2(2,  2)
);
/*
const ivec2 offset5x5[25] = ivec2[25](
	ivec2(-2, -2), ivec2(-1, -2), ivec2(0, -2), ivec2(1, -2), ivec2(2, -2),
	ivec2(-2, -1), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(2, -1),
	ivec2(-2,  0), ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0), ivec2(2,  0), 
	ivec2(-2,  1), ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1), ivec2(2,  1), 
	ivec2(-2,  2), ivec2(-1,  2), ivec2(0,  2), ivec2(1,  2), ivec2(2,  2)
);

const ivec2 offset7x7[49] = ivec2[49](
	ivec2(-3, -3), ivec2(-2, -3), ivec2(-1, -3), ivec2(0, -3), ivec2(1, -3), ivec2(2, -3), ivec2(3, -3),
	ivec2(-3, -2), ivec2(-2, -2), ivec2(-1, -2), ivec2(0, -2), ivec2(1, -2), ivec2(2, -2), ivec2(3, -2),
	ivec2(-3, -1), ivec2(-2, -1), ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1), ivec2(2, -1), ivec2(3, -1),
	ivec2(-3,  0), ivec2(-2,  0), ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0), ivec2(2,  0), ivec2(3,  0),
	ivec2(-3,  1), ivec2(-2,  1), ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1), ivec2(2,  1), ivec2(3,  1),
	ivec2(-3,  2), ivec2(-2,  2), ivec2(-1,  2), ivec2(0,  2), ivec2(1,  2), ivec2(2,  2), ivec2(3,  2),
	ivec2(-3,  3), ivec2(-2,  3), ivec2(-1,  3), ivec2(0,  3), ivec2(1,  3), ivec2(2,  3), ivec2(3,  3)
);
*/