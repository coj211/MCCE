
float textureSmooth(in vec2 coord) {
	coord += 0.5f;

	vec2 whole = floor(coord);
	vec2 part  = curve(coord - whole);

	coord = whole + part - 0.5f;

	return texture(noisetex, coord * rcp(256.0)).x;
}

float WaterHeight(in vec2 p) {
    float wavesTime = frameTimeCounter * 1.2 * WATER_WAVE_SPEED;
	p.y *= 0.8;
	// p -= wavesTime * 1e-2;

    float wave = 0.0;
	wave += textureSmooth((p + vec2(0.0, p.x - wavesTime)) * 0.8);
	wave += textureSmooth((p - vec2(-wavesTime, p.x)) * 1.6) * 0.5;
	wave += textureSmooth((p + vec2(wavesTime * 0.6, p.x - wavesTime)) * 2.4) * 0.2;
	wave += textureSmooth((p - vec2(wavesTime * 0.6, p.x - wavesTime)) * 3.6) * 0.1;

    // vec2 wavesTime = frameTimeCounter * vec2(1.5, 0.8) * WATER_WAVE_SPEED;
	// // p -= wavesTime * 1e-2;

	// const mat2 rotation = mat2(cos(2.4), -sin(2.4), sin(2.4), cos(2.4));

	// p = rotation * p + wavesTime;
	// float wave = textureSmooth(p * vec2(0.5, 0.2));	
	// p = rotation * p + wavesTime;
	// wave += textureSmooth(p * vec2(1.0, 0.4)) * 0.5;
	// p = rotation * p + wavesTime;
	// wave += textureSmooth(p * vec2(1.6, 0.8)) * 0.3;
	// p = rotation * p + wavesTime;
	// wave += textureSmooth(p * vec2(2.0, 2.4)) * 0.2;

	#if defined DISTANT_HORIZONS
		return wave / (0.8 + dot(abs(dFdx(p) + dFdy(p)), vec2(2e2 / dhFarPlane)));
	#else
		return wave / (0.8 + dot(abs(dFdx(p) + dFdy(p)), vec2(80.0 / far)));
	#endif
}

vec3 GetWavesNormal(in vec2 position) {
	float wavesCenter = WaterHeight(position);
	float wavesLeft   = WaterHeight(position + vec2(0.04, 0.0));
	float wavesUp     = WaterHeight(position + vec2(0.0, 0.04));

	vec2 wavesNormal = vec2(wavesCenter - wavesLeft, wavesCenter - wavesUp);

	return normalize(vec3(wavesNormal * WATER_WAVE_HEIGHT, 0.5));
}
