
uniform vec2 taaOffset;

out vec2 texcoord;

#include "/Settings.glsl"

void main() {
	gl_Position = ftransform();

    #ifdef TAA_ENABLED
        gl_Position.xy += taaOffset * gl_Position.w;
    #endif

	texcoord = mat2(gl_TextureMatrix[0]) * gl_MultiTexCoord0.xy + gl_TextureMatrix[0][3].xy;
}