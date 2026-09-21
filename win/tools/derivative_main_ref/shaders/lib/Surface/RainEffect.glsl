
vec2 GetRainAnimationTex(in sampler2D tex, in vec2 pos) {
	pos *= 0.6;
	pos.x = (pos.x + floor(fract(frameTimeCounter) * 64.0)) * rcp(64.0);

	vec2 normal = texture(tex, pos).rg * 2.0 - 1.0;
	return normal;
}

vec2 GetRainNormal(in vec3 position) {
	vec2 normal = GetRainAnimationTex(colortex7, position.xz);

	float lod = dot(abs(fwidth(position)), vec3(5.0));
	normal /= 1.0 + lod;

	return normal * 0.75;
}

float GetRainWetness(in vec2 position) {
	//vec3 p = position * vec3(0.7f, 0.2f, 0.7f);
	position -= frameTimeCounter * vec2(0.01, 0.006);
	position *= 0.01;

	//float n = Get3DNoise(p).y;
	//n += Get3DNoise(p * 0.5f).x * 2.0;
	//n += Get3DNoise(p * 0.25f).x * 4.0;
	float n = texture(noisetex, position).z;
	n += texture(noisetex, position * 0.6).x * 2.0;
	n += texture(noisetex, position * 0.2).y * 3.0;

	return saturate(n * 0.18) * wetnessCustom;
}
