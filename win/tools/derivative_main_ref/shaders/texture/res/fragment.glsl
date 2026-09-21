#version 430 core

layout(location = 0) out vec4 Output;

const float PI = 3.14159265359;
in vec2 texCoord;

uniform float seed2;


uint ihash1D(uint q)
{
    // hash by Hugo Elias, Integer Hash - I, 2017
    q = q * 747796405u + 2891336453u;
    q = (q << 13u) ^ q;
    return q * (q * q * 15731u + 789221u) + 1376312589u;
}

uvec2 ihash1D(uvec2 q)
{
    // hash by Hugo Elias, Integer Hash - I, 2017
    q = q * 747796405u + 2891336453u;
    q = (q << 13u) ^ q;
    return q * (q * q * 15731u + 789221u) + 1376312589u;
}

uvec4 ihash1D(uvec4 q)
{
    // hash by Hugo Elias, Integer Hash - I, 2017
    q = q * 747796405u + 2891336453u;
    q = (q << 13u) ^ q;
    return q * (q * q * 15731u + 789221u) + 1376312589u;
}

// generates a random number for each of the 4 cell corners
vec4 multiHash2D(vec4 cell)
{
    uvec4 i = uvec4(cell);
    uvec4 hash = ihash1D(ihash1D(i.xzxz) + i.yyww);
    return vec4(hash) * (1.0 / float(0xffffffffu));
}

// generates 2 random numbers for the coordinate
vec2 multiHash2D(vec2 x)
{
    uvec2 q = uvec2(x);
    uint h0 = ihash1D(ihash1D(q.x) + q.y);
    uint h1 = h0 * 1933247u + ~h0 ^ 230123u;
    return vec2(h0, h1)  * (1.0 / float(0xffffffffu));
}

// generates 2 random numbers for each of the 4 cell corners
void multiHash2D(vec4 cell, out vec4 hashX, out vec4 hashY)
{
    uvec4 i = uvec4(cell);
    uvec4 hash0 = ihash1D(ihash1D(i.xzxz) + i.yyww);
    uvec4 hash1 = ihash1D(hash0 ^ 1933247u);
    hashX = vec4(hash0) * (1.0 / float(0xffffffffu));
    hashY = vec4(hash1) * (1.0 / float(0xffffffffu));
}

// generates 2 random numbers for each of the four 2D coordinates
void multiHash2D(vec4 coords0, vec4 coords1, out vec4 hashX, out vec4 hashY)
{
    uvec4 hash0 = ihash1D(ihash1D(uvec4(coords0.xz, coords1.xz)) + uvec4(coords0.yw, coords1.yw));
    uvec4 hash1 = hash0 * 1933247u + ~hash0 ^ 230123u;
    hashX = vec4(hash0) * (1.0 / float(0xffffffffu));
    hashY = vec4(hash1) * (1.0 / float(0xffffffffu));
}

float perlinNoise(vec2 pos, vec2 scale, float seed)
{
    // based on Modifications to Classic Perlin Noise by Brian Sharpe: https://archive.is/cJtlS
    pos *= scale;
    vec4 i = floor(pos).xyxy + vec2(0.0, 1.0).xxyy;
    vec4 f = (pos.xyxy - i.xyxy) - vec2(0.0, 1.0).xxyy;
    i = mod(i, scale.xyxy) + seed;

    // grid gradients
    vec4 gradientX, gradientY;
    multiHash2D(i, gradientX, gradientY);
    gradientX -= 0.49999;
    gradientY -= 0.49999;

    // perlin surflet
    vec4 gradients = inversesqrt(gradientX * gradientX + gradientY * gradientY) * (gradientX * f.xzxz + gradientY * f.yyww);
    // normalize: 1.0 / 0.75^3
    gradients *= 2.3703703703703703703703703703704;
    vec4 lengthSq = f * f;
    lengthSq = lengthSq.xzxz + lengthSq.yyww;
    vec4 xSq = 1.0 - min(vec4(1.0), lengthSq);
    xSq = xSq * xSq * xSq;
    return dot(xSq, gradients);
}

vec2 cellularNoise(vec2 pos, vec2 scale, float jitter, float seed)
{
    pos *= scale;
    vec2 i = floor(pos);
    vec2 f = pos - i;

    const vec3 offset = vec3(-1.0, 0.0, 1.0);
    vec4 cells = mod(i.xyxy + offset.xxzz, scale.xyxy) + seed;
    i = mod(i, scale) + seed;
    vec4 dx0, dy0, dx1, dy1;
    multiHash2D(vec4(cells.xy, vec2(i.x, cells.y)), vec4(cells.zyx, i.y), dx0, dy0);
    multiHash2D(vec4(cells.zwz, i.y), vec4(cells.xw, vec2(i.x, cells.w)), dx1, dy1);

    dx0 = offset.xyzx + dx0 * jitter - f.xxxx; // -1 0 1 -1
    dy0 = offset.xxxy + dy0 * jitter - f.yyyy; // -1 -1 -1 0
    dx1 = offset.zzxy + dx1 * jitter - f.xxxx; // 1 1 -1 0
    dy1 = offset.zyzz + dy1 * jitter - f.yyyy; // 1 0 1 1
    vec4 d0 = dx0 * dx0 + dy0 * dy0;
    vec4 d1 = dx1 * dx1 + dy1 * dy1;

    vec2 centerPos = multiHash2D(i) * jitter - f; // 0 0

    vec4 F = min(d0, d1);
    // shuffle into F the 4 lowest values
    F = min(F, max(d0, d1).wzyx);
    // shuffle into F the 2 lowest values
    F.xy = min(min(F.xy, F.zw), max(F.xy, F.zw).yx);
    // add the last value
    F.zw = vec2(dot(centerPos, centerPos), 1e+5);
    // shuffle into F the final 2 lowest values
    F.xy = min(min(F.xy, F.zw), max(F.xy, F.zw).yx);

    vec2 f12 = vec2(min(F.x, F.y), max(F.x, F.y));
    // normalize: 0.75^2 * 2.0  == 1.125
    return sqrt(f12) * (1.0 / 1.125);
}

float fbm_perlin(vec2 pos, float amplitude, float frequency, uint octaves, float seed) {
    float sum = 0.0;
    for (uint i = 0u; i < octaves; i++) {
        sum += amplitude * perlinNoise(pos, vec2(frequency), seed);
        amplitude *= 0.5;
        frequency *= 2.0;
        pos *= 0.5;
    }
    return sum * 0.5 + 0.5;
}

float fbm_worley(vec2 pos, float amplitude, float frequency, uint octaves, float seed) {
    float sum = 0.0;
    for (uint i = 0u; i < octaves; i++) {
        sum += amplitude * (1.0 - cellularNoise(pos, vec2(frequency), 1.0, seed).x);
        amplitude *= 0.5;
        frequency *= 2.0;
        pos *= 0.5;
    }
    return sum;
}

#define pi PI
//http://www.jcgt.org/published/0009/03/02/paper.pdf
uvec3 hash33UintPcg(uvec3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    //v += v.yzx * v.zxy; //swizzled notation is not exactly the same because components depend on each other, but works too

    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    //v += v.yzx * v.zxy;
    return v;
}

vec3 hash3i3f(ivec3 seed)
{
    uvec3 hash_uvec3 = hash33UintPcg(uvec3(seed));
    return vec3(hash_uvec3) * (1.0f / float(~0u));
}

//these hashes create a random unit vector in floatN from an intN seed
vec2 hashis(ivec2 seed)
{
    float ang = hash3i3f(seed.xyy).x * 2.0f * pi;
    return vec2(cos(ang), sin(ang));
}

vec3 hashis(ivec3 seed)
{
    vec3 h = hash3i3f(seed.xyz);
    float ang = h.x * 2.0f * pi;
    float z = h.z * 2.0f - 1.0f;
    float m = sqrt(clamp(1.0f - z * z, 0.0f, 1.0f));
    return vec3(cos(ang) * m, sin(ang) * m, z);
    return normalize(hash3i3f(seed) - vec3(0.5));
}

vec4 hashis(ivec4 seed)
{
    uvec3 seed3 = hash33UintPcg(uvec3(hash33UintPcg(uvec3(seed.xyz)).xy, seed.w));
    vec2 xy = hash3i3f(ivec3(seed3)).xy;
    vec2 zw = hash3i3f(ivec3(seed3) + ivec3(1)).xy;
    return normalize(vec4(xy, zw) - vec4(0.5));
}

vec3 hashif3(ivec2 seed)
{
    return hash3i3f(seed.xyy);
}

vec3 hashif3(ivec3 seed)
{
    return hash3i3f(seed);
}

vec3 hashif3(ivec4 seed)
{
    uvec3 seed3 = hash33UintPcg(uvec3(hash33UintPcg(uvec3(seed.xyz)).xy, seed.w));
    return hash3i3f(ivec3(seed3));
}

//For a given component number i, ComponentOrder3(pos)[i] returns how many p[j] are greater than p[i]
//For example, for vector (0.4, 0.5, 0.3), g = (1, 0, 2)
vec2 ComponentOrder(vec2 pos)
{
    float c = step(pos.y, pos.x);
    return vec2(1.0 - c, c);
}
vec3 ComponentOrder(vec3 pos)
{
    vec3 res = vec3(0.0, ComponentOrder(pos.yz));
    vec2 c = step(pos.yz, pos.xx);
    res += vec3(2.0 - dot(c, vec2(1.0)), c);
    return res;
}
vec4 ComponentOrder(vec4 pos)
{
    vec4 res = vec4(0.0, ComponentOrder(pos.yzw));
    vec3 c = step(pos.yzw, pos.xxx);
    res += vec4(3.0 - dot(c, vec3(1.0)), c);
    return res;
}

//This is mostly based on the wiki article
//https://en.wikipedia.org/wiki/Simplex_noise
float SimplexF(int n)
{
    return (sqrt(float(n) + 1.0) - 1.0) / float(n);
}

float SimplexG(int n)
{
    return (1.0 - 1.0 / sqrt(float(n) + 1.0)) / float(n);
}

//Distance between two hypertetrahedron vertices of the skewed lattice. Exact for N=2, inexact for N>2
float SimplexGridStep(int n)
{
    //return length(vecN(1.0, 0.0, 0.0...) - vecN(SimplexG(n)))
    float G = SimplexG(n);
    return sqrt((1.0 - G) * (1.0 - G) + G * G * float(n - 1));
}

//Escribed radius for a hypertetrahedron with a unit edge. Exact.
//Derived by analyzing analytical solutions for N=1,2,3 and 4, seems like a pretty clear progression. Have not tested for N>4
float GetEscribedSimplexRadius(int n)
{
    return sqrt(float(n) / (2.0f * (float(n) + 1.0f)));
}

//This is found empirically and seems to produce noise in the 0..1 range for the original simplex noise weights
float SimplexGradNormalFactor(int n)
{
    return 3.0f * sqrt(float(n));
}

//poor man's templates
#define SIMPLEX_GRID_DEFINITIONS(N)\
    struct SimplexVerticesN                                                \
    {                                                                      \
        ivecN pos[N];                                                      \
    };                                                                     \
    /*for a given n generates                */                            \
    /*res =    (0,  0,  ..  0,  1,  1, .., 1)*/                            \
    /*          0   1   .. n-1  n  n+1 .., N */                            \
    vecN ShiftedOnesN(float n)                                             \
    {                                                                      \
      return step(vecN(n), step_vecN);                                     \
    }                                                                      \
    SimplexVerticesN SchlafliOrthosceme(vecN pos)                          \
    {                                                                      \
        /*From https://en.wikipedia.org/wiki/Simplex_noise*/               \
        /*For example, the point (0.4, 0.5, 0.3) would lie inside the simplex with vertices (0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 1, 1).*/\
        /*The yi' coordinate is the largest, so it is added first. It is then followed by the xi' coordinate, and finally zi'.*/\
                                                                           \
        /*These can be arranged into a matrix of rows:*/                   \
        /*(0, 1, 0)*/                                                      \
        /*(1, 1, 0)*/                                                      \
        /*(1, 1, 1).*/                                                     \
                                                                           \
        /*Each column has a bunch of zeroes at first and then remaining ones. The number of zeroes is equal to its position in sorted order g[i]*/\
                                                                           \
        vecN g = ComponentOrder(pos);                                      \
                                                                           \
        matN p_T;                                                          \
        for(int i = 0; i < N; i++)                                         \
            p_T[i] = ShiftedOnesN(g[i]);                                   \
                                                                           \
        matN p = transpose(p_T);                                           \
        SimplexVerticesN vertices;                                         \
                                                                           \
        for(int i = 0; i < N; i++)                                         \
            vertices.pos[i] = ivecN(p[i]);                                 \
        return vertices;                                                   \
    }                                                                      \
    float VertexWeight(vecN delta, float r2)                               \
    {                                                                      \
        float s = max(r2 - dot(delta, delta), 0.0f);                       \
        return s * s * s * s / (r2 * r2 * r2 * r2);                        \
    }                                                                      \
    vecN SkewLattice(vecN pos)                                             \
    {                                                                      \
        return pos + dot(vecN(1.0f), pos) * SimplexF(N);                   \
    }                                                                      \
    vecN UnskewLattice(vecN skewed_pos)                                    \
    {                                                                      \
        return skewed_pos - dot(vecN(1.0f), skewed_pos) * SimplexG(N);     \
    }                                                                      \
    struct SimplexNodesN                                                   \
    {                                                                      \
        ivecN indices[N+1];                                                \
        vecN pos[N+1];                                                     \
        float weights[N+1];                                                \
    };                                                                     \
    /*rel_verts and rel_pos are relative to verts[0]*/                     \
    vecN GetBarycentricWeights(matN rel_verts, vecN rel_pos)               \
    {                                                                      \
        return inverse(rel_verts) * rel_pos;                               \
    }                                                                      \
    float GetRadialWeight(vecN delta)                                      \
    {                                                                      \
        /*return max(0.0f, 1.0f - length(delta) / (1.5f * GetEscribedSimplexRadius(N) * SimplexGridStep(N)));*/\
        return smoothstep(0.0f, 1.0f, max(0.0f, 1.0f - length(delta) / SimplexGridStep(N)));     \
    }                                                                      \
    SimplexNodesN GetSimplexNodes(vecN pos)                                \
    {                                                                      \
        SimplexNodesN nodes;                                               \
                                                                           \
        vecN skewed_pos = SkewLattice(pos);                                \
        ivecN base = ivecN(floor(skewed_pos));                             \
                                                                           \
        SimplexVerticesN ortho = SchlafliOrthosceme(skewed_pos - vecN(base));\
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            nodes.indices[i] = base + ((i==0) ? ivecN(0) : ortho.pos[i - 1]);\
            nodes.pos[i] = UnskewLattice(vecN(nodes.indices[i]));          \
        }                                                                  \
        matN barycentric_verts;                                            \
        for(int i = 0; i < N; i++)                                         \
            barycentric_verts[i] = nodes.pos[i + 1] - nodes.pos[0];        \
        vecN weightsn = GetBarycentricWeights(barycentric_verts, pos - nodes.pos[0]);\
        for(int i = 0; i < N; i++)                                         \
            nodes.weights[i + 1] = weightsn[i];                            \
        nodes.weights[0] = 1.0f - dot(vecN(1), weightsn);                  \
        float total = 1e-5f;                                               \
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            nodes.weights[i] = nodes.weights[i] * GetRadialWeight(pos - nodes.pos[i]);       \
            total += nodes.weights[i];                                     \
        }                                                                  \
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            nodes.weights[i] /= total;                                     \
        }                                                                  \
                                                                           \
        return nodes;                                                      \
    }                                                                      \
    SimplexNodesN GetSimplexNodesOriginal(vecN pos, float r2)              \
    {                                                                      \
        SimplexNodesN nodes;                                               \
                                                                           \
        vecN skewed_pos = SkewLattice(pos);                                \
        ivecN base = ivecN(floor(skewed_pos));                             \
                                                                           \
        SimplexVerticesN ortho = SchlafliOrthosceme(skewed_pos - vecN(base));\
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            nodes.indices[i] = base + ((i==0) ? ivecN(0) : ortho.pos[i - 1]);\
            nodes.pos[i] = UnskewLattice(vecN(nodes.indices[i]));          \
            nodes.weights[i] = VertexWeight(nodes.pos[i] - pos, r2);       \
        }                                                                  \
        return nodes;                                                      \
    }                                                                      \
    float GetGradSimplexNoiseOriginal(vecN pos, float r2)                  \
    {                                                                      \
        SimplexNodesN nodes = GetSimplexNodesOriginal(pos, r2);            \
        float res = 0.0f;                                                  \
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            vecN delta = pos - nodes.pos[i];                               \
            vecN node_grad = hashis(nodes.indices[i]);                     \
            /*no explanation for why normal_factor is this value*/         \
            /*determined emplirically, seems to work*/                     \
            res += dot(delta, node_grad) * nodes.weights[i] * SimplexGradNormalFactor(N);\
        }                                                                  \
        return res * 0.5f + 0.5f;                                          \
    }                                                                      \
    float GetGradSimplexNoise(vecN pos)                                    \
    {                                                                      \
        SimplexNodesN nodes = GetSimplexNodes(pos);                        \
        float res = 0.0f;                                                  \
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            vecN delta = pos - nodes.pos[i];                               \
            vecN node_grad = hashis(nodes.indices[i]);                     \
            float maxVal = GetEscribedSimplexRadius(N) * SimplexGridStep(N);\
            res += dot(delta, node_grad) * nodes.weights[i] / maxVal;      \
        }                                                                  \
        return res * 0.5f + 0.5f;                                          \
    }                                                                      \
    vec3 GetValueSimplexNoise(vecN pos)                                    \
    {                                                                      \
        SimplexNodesN nodes = GetSimplexNodes(pos);                        \
        vec3 res = vec3(0.0f);                                             \
        for(int i = 0; i < N+1; i++)                                       \
        {                                                                  \
            vec3 node_val = hashif3(nodes.indices[i]);                     \
            res += node_val * nodes.weights[i];                            \
        }                                                                  \
        return res;                                                        \
    }





#define vecN vec2
#define matN mat2
#define ivecN ivec2
#define SimplexVerticesN SimplexVertices2
#define step_vecN vec2(0.5, 1.5)
#define ShiftedOnesN ShiftedOnes2
#define SimplexNodesN SimplexNodes2
SIMPLEX_GRID_DEFINITIONS(2)
#undef vecN
#undef matN
#undef ivecN
#undef SimplexVerticesN
#undef step_vecN
#undef ShiftedOnesN
#undef SimplexNodesN

#define vecN vec3
#define matN mat3
#define ivecN ivec3
#define SimplexVerticesN SimplexVertices3
#define step_vecN vec3(0.5, 1.5, 2.5)
#define ShiftedOnesN ShiftedOnes3
#define SimplexNodesN SimplexNodes3
SIMPLEX_GRID_DEFINITIONS(3)
#undef vecN
#undef matN
#undef ivecN
#undef SimplexVerticesN
#undef step_vecN
#undef ShiftedOnesN
#undef SimplexNodesN


#define vecN vec4
#define matN mat4
#define ivecN ivec4
#define SimplexVerticesN SimplexVertices4
#define step_vecN vec4(0.5, 1.5, 2.5, 3.5)
#define ShiftedOnesN ShiftedOnes4
#define SimplexNodesN SimplexNodes4
SIMPLEX_GRID_DEFINITIONS(4)
#undef vecN
#undef matN
#undef ivecN
#undef SimplexVerticesN
#undef step_vecN
#undef ShiftedOnesN
#undef SimplexNodesN

// uint pcg(uint seed) {
//     seed = seed * 747796405u + 2891336453u;
//     seed = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
//     return (seed >> 22u) ^ seed;
// }

uvec3 pcg3d(uvec3 seed) {
    seed = seed * 1664525u + 1013904223u;
    seed.x += seed.y*seed.z; seed.y += seed.z*seed.x; seed.z += seed.x*seed.y;
    seed ^= seed >> 16u;
    seed.x += seed.y*seed.z; seed.y += seed.z*seed.x; seed.z += seed.x*seed.y;
    return seed;
}

uvec2 pcg2d(uvec2 seed) {
    return pcg3d(uvec3(seed, 97u)).xy;
}

uvec4 pcg4d(uvec4 seed) {
    seed = seed * 1664525u + 1013904223u;
    seed.x += seed.y*seed.w; seed.y += seed.z*seed.x; seed.z += seed.x*seed.y; seed.w += seed.y*seed.z;
    seed ^= seed >> 16u;
    seed.x += seed.y*seed.w; seed.y += seed.z*seed.x; seed.z += seed.x*seed.y; seed.w += seed.y*seed.z;
    return seed;
}

// float hash(float x) {
//     return pcg(uint(x + 97)) / float(0x7fffffffu);
// }

// vec2 hash22(vec2 x) {
//     return vec2(pcg2d(uvec2(x + 97)) ^ pcg2d(uvec2(fract(x) * float(0xffffffffu)))) * (1.0 / float(0xffffffffu));
// }

// vec3 hash33(vec3 x) {
//     return vec3(pcg3d(uvec3(x + 97)) / float(0xffffffffu));
// }

// vec4 hash44(vec4 x) {
//     return vec4(pcg4d(uvec4(x + 97)) ^ pcg4d(uvec4(fract(x) * float(0xffffffffu)))) * (1.0 / float(0xffffffffu));
// }

vec2 hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx+33.33);
    return fract((p3.xx+p3.yz)*p3.zy);

}

// Hash by David_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

vec3 hash33(vec3 p) {
	uvec3 q = uvec3(ivec3(p)) * UI3;
	q = (q.x ^ q.y ^ q.z)*UI3;
	return -1. + 2. * vec3(q) * UIF;
}

vec4 hash44(vec4 p4) {
	p4 = fract(p4  * vec4(.1031, .1030, .0973, .1099));
    p4 += dot(p4, p4.wzxy+33.33);
    return fract((p4.xxyz+p4.yzzw)*p4.zywx);
}

float perlinNoise(vec4 p) {
    vec4 i = floor(p);
    vec4 f = fract(p);

    vec4 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    vec4 g0000 = hash44(i + vec4(0.0, 0.0, 0.0, 0.0));
    vec4 g0001 = hash44(i + vec4(0.0, 0.0, 0.0, 1.0));
    vec4 g0010 = hash44(i + vec4(0.0, 0.0, 1.0, 0.0));
    vec4 g0011 = hash44(i + vec4(0.0, 0.0, 1.0, 1.0));
    vec4 g0100 = hash44(i + vec4(0.0, 1.0, 0.0, 0.0));
    vec4 g0101 = hash44(i + vec4(0.0, 1.0, 0.0, 1.0));
    vec4 g0110 = hash44(i + vec4(0.0, 1.0, 1.0, 0.0));
    vec4 g0111 = hash44(i + vec4(0.0, 1.0, 1.0, 1.0));
    vec4 g1000 = hash44(i + vec4(1.0, 0.0, 0.0, 0.0));
    vec4 g1001 = hash44(i + vec4(1.0, 0.0, 0.0, 1.0));
    vec4 g1010 = hash44(i + vec4(1.0, 0.0, 1.0, 0.0));
    vec4 g1011 = hash44(i + vec4(1.0, 0.0, 1.0, 1.0));
    vec4 g1100 = hash44(i + vec4(1.0, 1.0, 0.0, 0.0));
    vec4 g1101 = hash44(i + vec4(1.0, 1.0, 0.0, 1.0));
    vec4 g1110 = hash44(i + vec4(1.0, 1.0, 1.0, 0.0));
    vec4 g1111 = hash44(i + vec4(1.0, 1.0, 1.0, 1.0));

    g0000 = normalize(g0000 - 0.5);
    g0001 = normalize(g0001 - 0.5);
    g0010 = normalize(g0010 - 0.5);
    g0011 = normalize(g0011 - 0.5);
    g0100 = normalize(g0100 - 0.5);
    g0101 = normalize(g0101 - 0.5);
    g0110 = normalize(g0110 - 0.5);
    g0111 = normalize(g0111 - 0.5);
    g1000 = normalize(g1000 - 0.5);
    g1001 = normalize(g1001 - 0.5);
    g1010 = normalize(g1010 - 0.5);
    g1011 = normalize(g1011 - 0.5);
    g1100 = normalize(g1100 - 0.5);
    g1101 = normalize(g1101 - 0.5);
    g1110 = normalize(g1110 - 0.5);
    g1111 = normalize(g1111 - 0.5);
    // g0000 = g0000 - 0.5;
    // g0001 = g0001 - 0.5;
    // g0010 = g0010 - 0.5;
    // g0011 = g0011 - 0.5;
    // g0100 = g0100 - 0.5;
    // g0101 = g0101 - 0.5;
    // g0110 = g0110 - 0.5;
    // g0111 = g0111 - 0.5;
    // g1000 = g1000 - 0.5;
    // g1001 = g1001 - 0.5;
    // g1010 = g1010 - 0.5;
    // g1011 = g1011 - 0.5;
    // g1100 = g1100 - 0.5;
    // g1101 = g1101 - 0.5;
    // g1110 = g1110 - 0.5;

    float x0000 = dot(g0000, f - vec4(0.0, 0.0, 0.0, 0.0));
    float x0001 = dot(g0001, f - vec4(0.0, 0.0, 0.0, 1.0));
    float x0010 = dot(g0010, f - vec4(0.0, 0.0, 1.0, 0.0));
    float x0011 = dot(g0011, f - vec4(0.0, 0.0, 1.0, 1.0));
    float x0100 = dot(g0100, f - vec4(0.0, 1.0, 0.0, 0.0));
    float x0101 = dot(g0101, f - vec4(0.0, 1.0, 0.0, 1.0));
    float x0110 = dot(g0110, f - vec4(0.0, 1.0, 1.0, 0.0));
    float x0111 = dot(g0111, f - vec4(0.0, 1.0, 1.0, 1.0));
    float x1000 = dot(g1000, f - vec4(1.0, 0.0, 0.0, 0.0));
    float x1001 = dot(g1001, f - vec4(1.0, 0.0, 0.0, 1.0));
    float x1010 = dot(g1010, f - vec4(1.0, 0.0, 1.0, 0.0));
    float x1011 = dot(g1011, f - vec4(1.0, 0.0, 1.0, 1.0));
    float x1100 = dot(g1100, f - vec4(1.0, 1.0, 0.0, 0.0));
    float x1101 = dot(g1101, f - vec4(1.0, 1.0, 0.0, 1.0));
    float x1110 = dot(g1110, f - vec4(1.0, 1.0, 1.0, 0.0));
    float x1111 = dot(g1111, f - vec4(1.0, 1.0, 1.0, 1.0));

    return mix(
        mix(
            mix(
                mix(
                    x0000,
                    x1000,
                    u.x
                ),
                mix(
                    x0100,
                    x1100,
                    u.x
                ),
                u.y
            ),
            mix(
                mix(
                    x0010,
                    x1010,
                    u.x
                ),
                mix(
                    x0110,
                    x1110,
                    u.x
                ),
                u.y
            ),
            u.z
        ),
        mix(
            mix(
                mix(
                    x0001,
                    x1001,
                    u.x
                ),
                mix(
                    x0101,
                    x1101,
                    u.x
                ),
                u.y
            ),
            mix(
                mix(
                    x0011,
                    x1011,
                    u.x
                ),
                mix(
                    x0111,
                    x1111,
                    u.x
                ),
                u.y
            ),
            u.z
        ),
        u.w
    );
}


float fbmPerlin(vec4 p, float freq, int octaves, float lacunarity, float gain) {
    float sum = 0.0;
    float amp = 0.5;
    for (int i = 0; i < octaves; i++) {
        sum += perlinNoise(p * freq) * amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum * 0.5 + 0.5;
}

float seamlessPerlin(vec2 p, float freq) {
    float nx = cos(p.x * 2.0 * PI) / (2.0 * PI);
    float ny = cos(p.y * 2.0 * PI) / (2.0 * PI);
    float nz = sin(p.x * 2.0 * PI) / (2.0 * PI);
    float nw = sin(p.y * 2.0 * PI) / (2.0 * PI);
    return fbmPerlin(vec4(nx, ny, nz, nw), freq, 8, 2.0, 0.5);
}

float fbmSimplex(vec4 p, float freq, int octaves, float lacunarity, float gain) {
    float sum = 0.0;
    float amp = 0.5;
    for (int i = 0; i < octaves; i++) {
        sum += GetGradSimplexNoise(p * freq) * amp;
        freq *= lacunarity;
        amp *= gain;
    }
    return sum;
}

float seamlessSimplex(vec2 p, float freq) {
    float nx = cos(p.x * 2.0 * PI) / (2.0 * PI);
    float ny = cos(p.y * 2.0 * PI) / (2.0 * PI);
    float nz = sin(p.x * 2.0 * PI) / (2.0 * PI);
    float nw = sin(p.y * 2.0 * PI) / (2.0 * PI);
    return fbmSimplex(vec4(nx, ny, nz, nw), freq, 8, 2.0, 0.5);
}

float worleyNoise(vec2 u, float freq) {
    vec3 uv = vec3(u, 0.0);
    vec3 id = floor(uv);
    vec3 p = fract(uv);

    float minDist = 10000.;
    for (float x = -1.; x <= 1.; ++x)
    {
        for(float y = -1.; y <= 1.; ++y)
        {
            for(float z = -1.; z <= 1.; ++z)
            {
                vec3 offset = vec3(x, y, z);
            	vec3 h = hash33(mod(id + offset, vec3(freq))) * .5 + .5;
    			h += offset;
            	vec3 d = p - h;
           		minDist = min(minDist, dot(d, d));
            }
        }
    }

    // inverted worley noise
    return 1. - minDist;
}

float worleyFbm(vec2 p, float freq) {
    return worleyNoise(p * freq, freq) * 0.625
         + worleyNoise(p * freq * 2.0, freq * 2.0) * 0.25
         + worleyNoise(p * freq * 4.0, freq * 4.0) * 0.125;
}

float remap(float value, float low1, float high1, float low2, float high2) {
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

void main() {
    float perlin = seamlessSimplex(texCoord, 12.0);
    float worley = worleyFbm(texCoord, 16.0);
    float perlinWorley = remap(perlin, 0.0, 1.0, worley * 0.8 - 0.3, 0.8);
    // Output = vec4(noise, noise, noise, 1.0);
    // float perlin = seamlessPerlin(texCoord, 128.0);
    // float perlin = seamlessSimplex(texCoord, 16.0);
    // Output = vec4(perlin, perlin, perlin, 1.0);

    float perlin1 = fbm_perlin(texCoord, 0.4, 192.0, 6u, seed2);
    float perlin2 = fbm_perlin(fract(texCoord + 97.0 / 256.0), 0.4, 192.0, 6u, seed2);
    // float perlinW = fbm_perlin(texCoord, 0.4, 22.0, 5u, seed2);
    // float worley = fbm_worley(texCoord, 0.7, 12.0, 6u, seed2);
    // float perlinWorley = remap(perlinW, 0.0, 1.0, worley * 0.5, 0.5) * 1.15;
    Output = vec4(perlin1, perlin2, perlinWorley, 1.0);
}
