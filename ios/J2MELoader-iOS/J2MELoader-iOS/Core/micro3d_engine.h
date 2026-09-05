#ifndef MICRO3D_ENGINE_H
#define MICRO3D_ENGINE_H

#include "m3g_engine.h"
#include <vector>
#include <string>

class Micro3DAffineTrans {
public:
    int m00 = 4096, m01 = 0, m02 = 0, m03 = 0;
    int m10 = 0, m11 = 4096, m12 = 0, m13 = 0;
    int m20 = 0, m21 = 0, m22 = 4096, m23 = 0;

    Micro3DAffineTrans();
    void setIdentity();
    void rotationX(int angle);
    void rotationY(int angle);
    void rotationZ(int angle);
    void multiply(const Micro3DAffineTrans& other);
    Mat4 toMat4() const;
};

class Micro3DTexture {
public:
    Micro3DTexture(const std::vector<uint32_t>& pixels, int width, int height);
    const uint32_t* getPixels() const { return m_pixels.data(); }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    std::vector<uint32_t> m_pixels;
    int m_width;
    int m_height;
};

class Micro3DFigure {
public:
    Micro3DFigure(const std::vector<uint8_t>& mbacData);
    void setTexture(const Micro3DTexture& tex);
    void setAction(const std::vector<uint8_t>& traData, int actionIndex);
    void setPosture(int frame);
    void draw(LcduiDisplay* target, const Micro3DAffineTrans& trans);

private:
    std::vector<M3GVertex> m_vertices;
    std::vector<uint16_t> m_indices;
    std::vector<uint32_t> m_texturePixels;
    int m_texW = 0;
    int m_texH = 0;
    int m_frame = 0;
    void parseMbac(const uint8_t* data, size_t size);
};

#endif // MICRO3D_ENGINE_H