#ifndef NET_MINECRAFT_CLIENT_RENDERER__Tesselator_H__
#define NET_MINECRAFT_CLIENT_RENDERER__Tesselator_H__

//package net.minecraft.client.renderer;

#include <map>
#include <vector>
#include <cstdint>
#include "RenderChunk.h"
#include "gles.h"
#include "VertecDecl.h"

extern const int VertexSizeBytes;

typedef VertexDeclPTC VERTEX;
typedef std::map<GLuint, GLsizei> IntGLMap;


class LevelRenderer; // forward declare for friend
struct MeshBuffer;   // 0.8.1 背包渲染（Tesselator::end() 返回）

class Tesselator
{
    static const int MAX_MEMORY_USE = 16 * 1024 * 1024;
    static const int MAX_FLOATS = MAX_MEMORY_USE / 4 / 2;

	Tesselator(int size);
	friend class LevelRenderer;  // allow LevelRenderer to construct _meshTesselator

public:
	static const int ACCESS_DYNAMIC = 1;
	static const int ACCESS_STATIC = 2;

	static Tesselator instance;

	~Tesselator();

	void init();
    void clear();

    void begin();
    void begin(int mode);
	void draw();
	RenderChunk end(bool useMine, int bufferId);
	// 0.8.1 背包渲染：从当前顶点数据构建 MeshBuffer（Touch::InventoryPane 批量渲染用）
	MeshBuffer end();

#if defined(MACOS) || defined(LINUX) || defined(WIN32) || defined(ANDROID)
	// CPU-only tessellation: stores vertex data into outData / outVertexCount
	// without issuing any GL calls. Safe to call from a background thread.
	void endToCPU(std::vector<uint8_t>& outData, int& outVertexCount);
	// 拷贝当前已 tessellate 的顶点(不改状态,可在 end() 前调用;供植被 sway 基网格)
	void snapshotTo(std::vector<uint8_t>& outData, int& outVertexCount);
#endif

	void color(int c);
	void color(int c, int alpha);
    void color(float r, float g, float b);
    void color(float r, float g, float b, float a);
    void color(int r, int g, int b);
    void color(int r, int g, int b, int a);
    void color(char r, char g, char b);
	void colorABGR( int c );

	// 当前已 tessellate 的顶点数(begin 后累计;供调用方按 tile 差分记录
	// 每段顶点的属性,如 sway 幅度)。
	int getVertexCount() const { return vertices; }

	void normal(float x, float y, float z);
	void voidBeginAndEndCalls(bool doVoid);
	
	void tex(float u, float v);
    
	void vertex(float x, float y, float z);
	void vertexUV(float x, float y, float z, float u, float v);
	
	void scale2d(float x, float y);
	void resetScale();
	// 0.8.1 背包渲染：3D 方块图标的缩放/倾斜（renderGuiItemInChunk 用）
	void scale3d(float x, float y, float z);
	void tilt();
	void resetTilt();

    void noColor();
	void enableColor();
private:
	void setAccessMode(int mode);
public:

    void offset(float xo, float yo, float zo);
	void offset(const Vec3& v);
    void addOffset(float x, float y, float z);
	void addOffset(const Vec3& v);

	int getVboCount();

	int getColor();

	__inline void beginOverride() {
		begin();
		voidBeginAndEndCalls(true);
	}
	__inline void endOverrideAndDraw() {
		voidBeginAndEndCalls(false);
		draw();
	}
	__inline bool isOverridden() {
		return _voidBeginEnd;
	}
	__inline RenderChunk endOverride(int bufferId) {
		voidBeginAndEndCalls(false);
		return end(true, bufferId);
	}

private:
	Tesselator(const Tesselator& rhs) {}
	Tesselator& operator=(const Tesselator& rhs) { return *this; }
	VERTEX* _varray;

	int vertices;

	float xo, yo, zo;
	float u, v;
	unsigned int _color;
	int _normal;
	float _sx, _sy, _sz;
	bool _tilted;

	bool hasColor;
	bool hasTexture;
	bool hasNormal;
	bool _noColor;
	bool _voidBeginEnd;

	int p;
	int count;

	bool tesselating;

	bool vboMode;
	int vboCounts;
	int vboId;
	GLuint* vboIds;

	int size;
	int totalSize;
	int maxVertices;

	int mode;
	int accessMode;

	IntGLMap map;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__Tesselator_H__*/
