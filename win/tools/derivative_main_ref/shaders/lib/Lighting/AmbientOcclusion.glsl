
// vec3 uniformSphereSample(vec2 hash) {
// 	hash.x *= TAU; hash.y = 2.0 * hash.y - 1.0;
// 	return vec3(sincos(hash.x) * sqrt(1.0 - hash.y * hash.y), hash.y);
// }

// // https://amietia.com/lambertnotangent.html
// vec3 cosineWeightedHemisphereSample(vec3 vector, vec2 hash) {
// 	vec3 dir = normalize(uniformSphereSample(hash) + vector);
// 	return dot(dir, vector) < 0.0 ? -dir : dir;
// }

float SpiralAO(in vec2 coord, in vec3 viewPos, in vec3 normal, float dither) {
    float rSteps = 1.0 / float(SSAO_SAMPLES);
	float maxSqLen = sqr(viewPos.z) * 0.25;

    vec2 radius = vec2(0.0);
    vec2 rayStep = vec2(0.6 / aspectRatio, 0.6) / max((far - near) * -viewPos.z / far + near, 5.0) * gbufferProjection[1][1];

	const float goldenAngle = TAU / (PHI1 + 1.0);
	const mat2 goldenRotate = mat2(cos(goldenAngle), -sin(goldenAngle), sin(goldenAngle), cos(goldenAngle));

	vec2 rot = sincos(dither * TAU) * rSteps;
    float total = 0.0;

    for (uint i = 0u; i < SSAO_SAMPLES; ++i, rot *= goldenRotate) {
        radius += rayStep;

		// vec3 rayPos = cosineWeightedHemisphereSample(n, RandNext2F()) * radius + viewPos;
		// vec3 diff = ScreenToViewSpace(ViewToScreenSpaceRaw(rayPos).xy) - viewPos;
		vec3 diff = ScreenToViewSpace(coord + rot * radius) - viewPos;
		float diffSqLen = dotSelf(diff);
		if (diffSqLen > 1e-5 && diffSqLen < maxSqLen) {
			float NdotL = saturate(dot(normal, diff * inversesqrt(diffSqLen)));
			total += NdotL * saturate(1.0 - diffSqLen / maxSqLen);
		}

		diff = ScreenToViewSpace(coord - rot * radius) - viewPos;
		diffSqLen = dotSelf(diff);
		if (diffSqLen > 1e-5 && diffSqLen < maxSqLen) {
			float NdotL = saturate(dot(normal, diff * inversesqrt(diffSqLen)));
			total += NdotL * saturate(1.0 - diffSqLen / maxSqLen);
		}
    }

    total = max0(1.0 - total * rSteps * SSAO_STRENGTH);
    return total * sqrt(total);
}

#if defined DISTANT_HORIZONS
	float SpiralAO_DH(in vec2 coord, in vec3 viewPos, in vec3 normal, float dither) {
		float rSteps = 1.0 / float(SSAO_SAMPLES);
		float maxSqLen = sqr(viewPos.z) * 0.25;

		vec2 radius = vec2(0.0);
		vec2 rayStep = vec2(0.6 / aspectRatio, 0.6) / max((far - near) * -viewPos.z / far + near, 5.0) * gbufferProjection[1][1];

		const float goldenAngle = TAU / (PHI1 + 1.0);
		const mat2 goldenRotate = mat2(cos(goldenAngle), -sin(goldenAngle), sin(goldenAngle), cos(goldenAngle));

		vec2 rot = sincos(dither * TAU) * rSteps;
		float total = 0.0;

		for (uint i = 0u; i < SSAO_SAMPLES; ++i, rot *= goldenRotate) {
			radius += rayStep;

			// vec3 rayPos = cosineWeightedHemisphereSample(n, RandNext2F()) * radius + viewPos;
			// vec3 diff = ScreenToViewSpaceDH(ViewToScreenSpaceRaw(rayPos).xy) - viewPos;
			vec3 diff = ScreenToViewSpaceDH(coord + rot * radius) - viewPos;
			float diffSqLen = dotSelf(diff);
			if (diffSqLen > 1e-5 && diffSqLen < maxSqLen) {
				float NdotL = saturate(dot(normal, diff * inversesqrt(diffSqLen)));
				total += NdotL * saturate(1.0 - diffSqLen / maxSqLen);
			}

			diff = ScreenToViewSpaceDH(coord - rot * radius) - viewPos;
			diffSqLen = dotSelf(diff);
			if (diffSqLen > 1e-5 && diffSqLen < maxSqLen) {
				float NdotL = saturate(dot(normal, diff * inversesqrt(diffSqLen)));
				total += NdotL * saturate(1.0 - diffSqLen / maxSqLen);
			}
		}

		total = max0(1.0 - total * rSteps * SSAO_STRENGTH);
		return total * sqrt(total);
	}
#endif