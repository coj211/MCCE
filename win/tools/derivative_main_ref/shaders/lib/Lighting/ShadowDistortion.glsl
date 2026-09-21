
float cubeLength(in vec2 v) {
    vec2 t = abs(cube(v));
    return pow(t.x + t.y, 1.0 / 3.0);
}

float quarticLength(in vec2 v) {
	return sqrt2(pow4(v.x) + pow4(v.y));
}

float DistortionFactor(in vec2 shadowClipPos) {
	#if defined DISTANT_HORIZONS && defined DH_SHADOW
		return 1.0;
	#else
    	return quarticLength(shadowClipPos * 1.165) * SHADOW_MAP_BIAS + 1.0 - SHADOW_MAP_BIAS;
	#endif
}

// float DistortionFactor(in vec2 shadowClipPos) {
// 	float a = fastExp(12.0 * shadowProjection[0].x);
// 	float b = exp2(rLOG2) - a;
// 	return log(length(shadowClipPos) * b + a);
// }

vec3 DistortShadowSpace(in vec3 shadowClipPos, in float DistortionFactor) {
	#if defined DISTANT_HORIZONS && defined DH_SHADOW
		return shadowClipPos * vec3(vec2(1.0), 0.05);
	#else
		return shadowClipPos * vec3(vec2(rcp(DistortionFactor)), 0.2);
	#endif
}

vec3 DistortShadowSpace(in vec3 shadowClipPos) {
	float DistortionFactor = DistortionFactor(shadowClipPos.xy);
	#if defined DISTANT_HORIZONS && defined DH_SHADOW
		return shadowClipPos * vec3(vec2(1.0), 0.05);
	#else
		return shadowClipPos * vec3(vec2(rcp(DistortionFactor)), 0.2);
	#endif
}
