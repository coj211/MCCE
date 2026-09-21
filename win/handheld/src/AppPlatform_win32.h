#ifndef APPPLATFORM_WIN32_H__
#define APPPLATFORM_WIN32_H__

#include "AppPlatform.h"
#include "platform/log.h"
#include "client/renderer/gles.h"
#include "world/level/storage/FolderMethods.h"
#include <png.h>
#include <zlib.h>
#include <io.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

static void png_funcReadFile(png_structp pngPtr, png_bytep data, png_size_t length) {
	((std::istream*)png_get_io_ptr(pngPtr))->read((char*)data, length);
}

class AppPlatform_win32: public AppPlatform
{
public:
    AppPlatform_win32()
    {
    }

	BinaryBlob readAssetFile(const std::string& filename) {
		FILE* fp = fopen(("data/" + filename).c_str(), "r");
		if (!fp)
			return BinaryBlob();

		int size = getRemainingFileSize(fp);

		BinaryBlob blob;
		blob.size = size;
		blob.data = new unsigned char[size];

		fread(blob.data, 1, size, fp);
		fclose(fp);

		return blob;
	}

	// 枚举 data/lang/*.lang（去掉扩展名，如 en_US、zh_CN）。找不到返回空。
	StringVector listLanguageCodes() {
		StringVector codes;
		std::string pattern = "data/lang/*.lang";
		struct _finddata_t fd;
		intptr_t h = _findfirst(pattern.c_str(), &fd);
		if (h == -1)
			return codes;
		do {
			std::string name = fd.name;
			if (name.length() > 5 && name.compare(name.length() - 5, 5, ".lang") == 0)
				codes.push_back(name.substr(0, name.length() - 5));
		} while (_findnext(h, &fd) == 0);
		_findclose(h);
		return codes;
	}

    void saveScreenshot(const std::string& filename, int glWidth, int glHeight) {
        // 用户定制：从当前 GL 帧缓冲读像素，手写 PNG 写入（zlib 压缩，绕开 libpng 写路径的版本坑）
        // GLES 1.1 的 glReadPixels 只支持 GL_RGBA；且以实际 GL 视口为准（窗口逻辑尺寸可能不一致）
        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);
        int vpW = vp[2], vpH = vp[3];
        if (vpW <= 0 || vpH <= 0) { vpW = glWidth; vpH = glHeight; }
        if (vpW <= 0 || vpH <= 0)
            return;
        const size_t rowBytes = (size_t)vpW * 4; // RGBA
        std::vector<unsigned char> pixels(rowBytes * vpH);
        glReadPixels(0, 0, vpW, vpH, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

        // 游戏画面是竖屏(480x854)：直接输出竖图，不旋转。
        // glReadPixels 是标准 GL（原点左下，第 0 行=画面底部）→ PNG 行序翻转使顶部=天空
        const int outW = vpW, outH = vpH;
        const size_t outRow = (size_t)outW * 4;
        std::vector<unsigned char> raw((outRow + 1) * outH);
        for (int y = 0; y < outH; y++) {
            unsigned char* dst = &raw[(size_t)y * (outRow + 1)];
            dst[0] = 0; // filter None
            const unsigned char* s = &pixels[(size_t)(outH - 1 - y) * rowBytes];
            for (int x = 0; x < outW; x++) {
                dst[1 + x * 4 + 0] = s[x * 4 + 0];
                dst[1 + x * 4 + 1] = s[x * 4 + 1];
                dst[1 + x * 4 + 2] = s[x * 4 + 2];
                dst[1 + x * 4 + 3] = 255; // alpha
            }
        }
        uLongf compLen = compressBound((uLong)raw.size());
        std::vector<unsigned char> comp(compLen);
        if (compress2(&comp[0], &compLen, &raw[0], (uLong)raw.size(), 9) != Z_OK) {
            return;
        }

        FILE* fp = fopen(filename.c_str(), "wb");
        if (!fp) return;
        static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        fwrite(sig, 1, 8, fp);

        // 写 PNG chunk：len(4BE) + type(4) + data + crc32(type+data)(4BE)
        struct ChunkWriter {
            FILE* fp;
            void write(const char* type, const unsigned char* data, unsigned len) {
                unsigned char hdr[8];
                hdr[0] = (unsigned char)((len >> 24) & 0xff);
                hdr[1] = (unsigned char)((len >> 16) & 0xff);
                hdr[2] = (unsigned char)((len >> 8) & 0xff);
                hdr[3] = (unsigned char)(len & 0xff);
                memcpy(hdr + 4, type, 4);
                fwrite(hdr, 1, 8, fp);
                uLong crc = crc32(0, (const Bytef*)type, 4);
                if (len) {
                    crc = crc32(crc, data, len);
                    fwrite(data, 1, len, fp);
                }
                unsigned char cb[4];
                cb[0] = (unsigned char)((crc >> 24) & 0xff);
                cb[1] = (unsigned char)((crc >> 16) & 0xff);
                cb[2] = (unsigned char)((crc >> 8) & 0xff);
                cb[3] = (unsigned char)(crc & 0xff);
                fwrite(cb, 1, 4, fp);
            }
        } cw = { fp };

        // IHDR：width, height, bitdepth=8, colortype=6(RGBA), compression=0, filter=0, interlace=0
        unsigned char ihdr[13];
        ihdr[0] = (unsigned char)((outW >> 24) & 0xff);
        ihdr[1] = (unsigned char)((outW >> 16) & 0xff);
        ihdr[2] = (unsigned char)((outW >> 8) & 0xff);
        ihdr[3] = (unsigned char)(outW & 0xff);
        ihdr[4] = (unsigned char)((outH >> 24) & 0xff);
        ihdr[5] = (unsigned char)((outH >> 16) & 0xff);
        ihdr[6] = (unsigned char)((outH >> 8) & 0xff);
        ihdr[7] = (unsigned char)(outH & 0xff);
        ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
        cw.write("IHDR", ihdr, 13);
        cw.write("IDAT", &comp[0], (unsigned)compLen);
        cw.write("IEND", NULL, 0);
        fclose(fp);
    }

    __inline unsigned int rgbToBgr(unsigned int p) {
        return (p & 0xff00ff00) | ((p >> 16) & 0xff) | ((p << 16) & 0xff0000);
    }

    TextureData loadTexture(const std::string& filename_, bool textureFolder)
	{
		TextureData out;

		std::string filename = textureFolder? "data/images/" + filename_
											: filename_;
		std::ifstream source(filename.c_str(), std::ios::binary);

		if (source) {
			png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

			if (!pngPtr)
				return out;

			png_infop infoPtr = png_create_info_struct(pngPtr);

			if (!infoPtr) {
				png_destroy_read_struct(&pngPtr, NULL, NULL);
				return out;
			}

			// Hack to get around the broken libpng for windows
			png_set_read_fn(pngPtr,(voidp)&source, png_funcReadFile);

			png_read_info(pngPtr, infoPtr);

			// 统一输出 RGBA：RGB(3通道) PNG 补 alpha=255，避免写进 4 字节/像素缓冲后纹理错位出彩色条纹
			png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);

			// Set up the texdata properties
			out.w = png_get_image_width(pngPtr, infoPtr);
			out.h = png_get_image_height(pngPtr, infoPtr);

			png_bytep* rowPtrs = new png_bytep[out.h];
			out.data = new unsigned char[4 * out.w * out.h];
			out.memoryHandledExternally = false;

			int rowStrideBytes = 4 * out.w;
			for (int i = 0; i < out.h; i++) {
				rowPtrs[i] = (png_bytep)&out.data[i*rowStrideBytes];
			}
			png_read_image(pngPtr, rowPtrs);

			// Teardown and return
			png_destroy_read_struct(&pngPtr, &infoPtr,(png_infopp)0);
			delete[] (png_bytep)rowPtrs;
			source.close();

			return out;
		}
		else
		{
			LOGI("Couldn't find file: %s\n", filename.c_str());
			return out;
		}
    }

    std::string getDateString(int s) {
        std::stringstream ss;
		ss << s << " s (UTC)";
		return ss.str();
	}

	virtual int checkLicense() {
		static int _z = 0;//20;
		_z--;
		if (_z < 0) return 0;
		//if (_z < 0) return 107;
		return -2;
	}

	virtual int getScreenWidth();
	virtual int getScreenHeight();

	virtual float getPixelsPerMillimeter();

	virtual bool supportsTouchscreen();
	virtual bool hasBuyButtonWhenInvalidLicense();

private:
};

#endif /*APPPLATFORM_WIN32_H__*/
