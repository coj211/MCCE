
uniform vec3 cameraPosition;
uniform mat4 gbufferModelViewInverse;
uniform vec2 taaOffset;

#ifndef MC_GL_VENDOR_INTEL
	#define attribute in
#endif

attribute vec4 mc_Entity;
attribute vec4 at_tangent;

out vec4 tint;
out vec2 texcoord;
out vec3 minecraftPos;
out vec4 viewPos;

out vec2 lightmap;

flat out mat3 tbnMatrix;

flat out uint materialIDs;

#include "/Settings.glsl"

#define PHYSICS_OCEAN_SUPPORT

#ifdef PHYSICS_OCEAN
	#define PHYSICS_VERTEX
	#include "/lib/Water/PhysicsOceans.glsl"
#endif

void main() {
	tint = gl_Color;
	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	#ifdef PHYSICS_OCEAN
		// basic texture to determine how shallow/far away from the shore the water is
		physics_localWaviness = texelFetch(physics_waviness, ivec2(gl_Vertex.xz) - physics_textureOffset, 0).r;
		// transform gl_Vertex (since it is the raw mesh, i.e. not transformed yet)
		vec4 finalPosition = vec4(gl_Vertex.x, gl_Vertex.y + physics_waveHeight(gl_Vertex.xz, PHYSICS_ITERATIONS_OFFSET, physics_localWaviness, physics_gameTime), gl_Vertex.z, gl_Vertex.w);
		// pass this to the fragment shader to fetch the texture there for per fragment normals
		physics_localPosition = finalPosition.xyz;
		viewPos = gl_ModelViewMatrix * finalPosition;
	#else
		viewPos = gl_ModelViewMatrix * gl_Vertex;
	#endif
	gl_Position = gl_ProjectionMatrix * viewPos;

	minecraftPos = mat3(gbufferModelViewInverse) * viewPos.xyz + gbufferModelViewInverse[3].xyz + cameraPosition;

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
    tbnMatrix[0] = normalize(gl_NormalMatrix * at_tangent.xyz);
    tbnMatrix[1] = cross(tbnMatrix[0], tbnMatrix[2]) * sign(at_tangent.w);

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	lightmap = clamp(gl_MultiTexCoord1.xy * (1.0 / 240.0), 0.0, 1.0);

	materialIDs = max(uint(mc_Entity.x - 1e4), 16u);
}