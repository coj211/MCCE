
uniform vec3 cameraPosition;
uniform mat4 gbufferModelViewInverse;
uniform vec2 taaOffset;

uniform mat4 dhProjection;

uniform mat4 gbufferModelView;

out vec4 tint;
// out vec2 texcoord;
out vec3 minecraftPos;
out vec4 viewPos;

out vec2 lightmap;

flat out mat3 tbnMatrix;

flat out uint materialIDs;

#include "/lib/Head/Common.inc"

void main() {
	tint = gl_Color;
	// texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;

	viewPos = gl_ModelViewMatrix * gl_Vertex;
	gl_Position = dhProjection * viewPos;

	#ifdef TAA_ENABLED
		gl_Position.xy += taaOffset * gl_Position.w;
	#endif

	minecraftPos = transMAD(gbufferModelViewInverse, viewPos.xyz) + cameraPosition;

    tbnMatrix[2] = normalize(gl_NormalMatrix * gl_Normal);
    tbnMatrix[0] = normalize(gbufferModelView[0].xyz);
    tbnMatrix[1] = normalize(gbufferModelView[2].xyz);

	lightmap = mat2(gl_TextureMatrix[1]) * gl_MultiTexCoord1.xy + gl_TextureMatrix[1][3].xy;
	lightmap = saturate((lightmap - 0.03125) * 1.06667);

	materialIDs = dhMaterialId == DH_BLOCK_WATER ? 17u : 16u;
}