// Minimal declaration header for stb_vorbis (compiled separately as C in
// stb_vorbis.c, which handles extern "C" itself via __cplusplus).
#ifndef MOD_STB_VORBIS_H
#define MOD_STB_VORBIS_H

#ifdef __cplusplus
extern "C" {
#endif

// Decodes a whole ogg/vorbis stream from memory to interleaved 16-bit PCM.
// Returns number of frames (samples per channel), or <= 0 on failure.
// *output is malloc'd - the caller must free() it.
extern int stb_vorbis_decode_memory(const unsigned char *mem, int len, int *channels, int *sample_rate, short **output);

#ifdef __cplusplus
}
#endif

#endif // MOD_STB_VORBIS_H
