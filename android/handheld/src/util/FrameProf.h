#ifndef NET_UTIL__FrameProf_H__
#define NET_UTIL__FrameProf_H__

// Lightweight per-frame section profiler (diagnostics only).
//
// Why: to speed the game up we need to know where the frame time actually
// goes (culling vs. chunk rebuild vs. each terrain layer vs. entities/GUI)
// instead of guessing. This is deliberately NOT a logging facility:
//   - it only collects while the F3 debug overlay is open (FrameProf::on()),
//     so during normal play every call is a single bool read;
//   - it writes ONE small file (perf_dump.txt next to the exe) every 120
//     frames while F3 is open, i.e. no per-frame disk I/O;
//   - all timers are QueryPerformanceCounter deltas.
//
// Usage:
//   double t0 = FrameProf::now();
//   ...work...
//   FrameProf::add(FrameProf::SEC_CULL, t0, FrameProf::now());
// and once per frame: FrameProf::endFrame(updateMs);

#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

namespace FrameProf {

enum Section {
	SEC_CULL = 0,
	SEC_DIRTY,
	SEC_DIRTY_SORT,
	SEC_DIRTY_REBUILD,
	SEC_MESH_PREFETCH,
	SEC_MESH_TESS,
	SEC_MESH_UPLOAD,
	SEC_LAYER0,
	SEC_LAYER1,
	SEC_LAYER2,
	SEC_LAYER3,
	SEC_ENTITIES,
	SEC_PARTICLES,
	SEC_HAND,
	SEC_GUI,
	SEC_TICK,
	SEC_UPDATELIGHTS,
	SEC_SETUP,          // chunk rebuild: Region/TileRenderer setup
	SEC_CHUNK_ALL,      // chunk rebuild: whole rebuildImpl()
	SEC_COUNT
};

inline const char* sectionName(int i) {
	// NOTE: must line up 1:1 with the enum above. A previous mismatch made the
	// "dirtyMesh" row print the whole rebuild loop instead of the mesh step.
	static const char* n[SEC_COUNT] = {
		"cull", "rebuildImpl", "dirtySort", "dirtyLoop", "meshPrefetch", "meshTess", "meshUpload", "terrainLayer0", "terrainLayer1", "terrainLayer2",
		"terrainLayer3", "entities", "particles", "hand", "gui", "tick", "updateLights", "meshSetup", "chunkRebuild"
	};
	return (i >= 0 && i < SEC_COUNT) ? n[i] : "?";
}

inline double*  accBuf()     { static double a[SEC_COUNT] = {0}; return a; }
inline double&  totalAcc()   { static double t = 0.0; return t; }
inline double&  updateAcc()  { static double t = 0.0; return t; }
inline int&     frameCount() { static int f = 0; return f; }
inline bool&    on()         { static bool b = false; return b; }
inline int&     visibleChunks() { static int v = 0; return v; }
// Chunk-rebuild diagnostics: how many chunks were rebuilt this frame and how
// long the dirty queue is (a persistently long queue = constant rebuilding).
inline int&     rebuiltAcc()  { static int r = 0; return r; }
inline int&     dirtyQueue()  { static int d = 0; return d; }
inline int&     dirtyMarks()  { static int m = 0; return m; }   // chunks marked dirty
inline int&     dirtyCalls()  { static int c = 0; return c; }   // updateDirtyChunks calls/frame
inline int&     genCount()    { static int g = 0; return g; }   // chunk load/generate triggers/frame

inline double qpcFreq() {
#ifdef _WIN32
	static double f = 0.0;
	if (f == 0.0) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = (double)q.QuadPart; }
	return f;
#else
	// 非 Win32（Android/Linux）：clock_gettime 直接给纳秒，频率按 1e9 计。
	return 1e9;
#endif
}
inline double now() {
#ifdef _WIN32
	LARGE_INTEGER t; QueryPerformanceCounter(&t); return (double)t.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
}

// Record one section's duration (t0/t1 from now()).
inline void add(int sec, double t0, double t1) {
	if (!on()) return;
	accBuf()[sec] += (t1 - t0) * 1000.0 / qpcFreq();
}

// Add an already-computed duration in milliseconds (for hot inner loops where
// calling now() twice per item would itself distort the measurement).
inline void addMs(int sec, double ms) {
	if (!on()) return;
	accBuf()[sec] += ms;
}

// Called once per frame with the total update() time.
inline void endFrame(double updateMs, double frameMs) {
	if (!on()) return;
	updateAcc() += updateMs;
	totalAcc()  += frameMs;
	if (++frameCount() < 120) return;

	int n = frameCount();
	FILE* f = fopen("perf_dump.txt", "w");
	if (f) {
		fprintf(f, "[frame] averages over %d frames (F3 open)\n", n);
		fprintf(f, "frame_ms=%.2f  update_ms=%.2f  visible_chunks=%d  rebuilt_per_frame=%.1f  dirty_queue=%d  dirty_marks_per_frame=%.1f  dirty_calls_per_frame=%.2f  newchunk_per_frame=%.1f\n",
			totalAcc() / n, updateAcc() / n, visibleChunks(), (double)rebuiltAcc() / n, dirtyQueue(), (double)dirtyMarks() / n, (double)dirtyCalls() / n, (double)genCount() / n);
		for (int i = 0; i < SEC_COUNT; ++i)
			fprintf(f, "  %-14s %.2f ms\n", sectionName(i), accBuf()[i] / n);
		fclose(f);
	}
	for (int i = 0; i < SEC_COUNT; ++i) accBuf()[i] = 0.0;
	updateAcc() = 0.0;
	totalAcc() = 0.0;
	rebuiltAcc() = 0;
	dirtyMarks() = 0;
	dirtyCalls() = 0;
	genCount() = 0;
	frameCount() = 0;
}

} // namespace FrameProf

#endif /*NET_UTIL__FrameProf_H__*/
