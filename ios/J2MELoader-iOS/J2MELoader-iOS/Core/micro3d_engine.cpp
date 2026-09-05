#include "micro3d_engine.h"
#include <cstring>
#include <cmath>
#include <algorithm>

Micro3DAffineTrans::Micro3DAffineTrans() { setIdentity(); }

void Micro3DAffineTrans::setIdentity() {
    m00 = 4096; m01 = 0; m02 = 0; m03 = 0;
    m10 = 0; m11 = 4096; m12 = 0; m13 = 0;
    m20 = 0; m21 = 0; m22 = 4096; m23 = 0;
}

void Micro3DAffineTrans::rotationX(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m11 = (int)(c * 4096); m12 = (int)(-s * 4096);
    m21 = (int)(s * 4096); m22 = (int)(c * 4096);
}

void Micro3DAffineTrans::rotationY(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m00 = (int)(c * 4096); m02 = (int)(s * 4096);
    m20 = (int)(-s * 4096); m22 = (int)(c * 4096);
}

void Micro3DAffineTrans::rotationZ(int angle) {
    float rad = (float)angle * 3.14159265f / 2048.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    m00 = (int)(c * 4096); m01 = (int)(-s * 4096);
    m10 = (int)(s * 4096); m11 = (int)(c * 4096);
}

void Micro3DAffineTrans::multiply(const Micro3DAffineTrans& other) {
    Micro3DAffineTrans res;
    res.m00 = (m00 * other.m00 + m01 * other.m10 + m02 * other.m20) >> 12;
    res.m01 = (m00 * other.m01 + m01 * other.m11 + m02 * other.m21) >> 12;
    res.m02 = (m00 * other.m02 + m01 * other.m12 + m02 * other.m22) >> 12;
    res.m03 = ((m00 * other.m03 + m01 * other.m13 + m02 * other.m23) >> 12) + m03;

    res.m10 = (m10 * other.m00 + m11 * other.m10 + m12 * other.m20) >> 12;
    res.m11 = (m10 * other.m01 + m11 * other.m11 + m12 * other.m21) >> 12;
    res.m12 = (m10 * other.m02 + m11 * other.m12 + m12 * other.m22) >> 12;
    res.m13 = ((m10 * other.m03 + m11 * other.m13 + m12 * other.m23) >> 12) + m13;

    res.m20 = (m20 * other.m00 + m21 * other.m10 + m22 * other.m20) >> 12;
    res.m21 = (m20 * other.m01 + m21 * other.m11 + m22 * other.m21) >> 12;
    res.m22 = (m20 * other.m02 + m21 * other.m12 + m22 * other.m22) >> 12;
    res.m23 = ((m20 * other.m03 + m21 * other.m13 + m22 * other.m23) >> 12) + m23;

    *this = res;
}

Mat4 Micro3DAffineTrans::toMat4() const {
    Mat4 res = Mat4::identity();
    res.m[0] = (float)m00 / 4096.0f; res.m[1] = (float)m10 / 4096.0f; res.m[2] = (float)m20 / 4096.0f;
    res.m[4] = (float)m01 / 4096.0f; res.m[5] = (float)m11 / 4096.0f; res.m[6] = (float)m21 / 4096.0f;
    res.m[8] = (float)m02 / 4096.0f; res.m[9] = (float)m12 / 4096.0f; res.m[10] = (float)m22 / 4096.0f;
    res.m[12] = (float)m03 / 4096.0f; res.m[13] = (float)m13 / 4096.0f; res.m[14] = (float)m23 / 4096.0f;
    return res;
}

Micro3DTexture::Micro3DTexture(const std::vector<uint32_t>& pixels, int width, int height)
    : m_pixels(pixels), m_width(width), m_height(height) {}

Micro3DFigure::Micro3DFigure(const std::vector<uint8_t>& mbacData) {
    parseMbac(mbacData.data(), mbacData.size());
}

void Micro3DFigure::parseMbac(const uint8_t* data, size_t size) {
    m_vertices.clear(); m_indices.clear();
    if (!data || size < 16) return; // no fake geometry: unknown format renders nothing
    bool isMbac = (size >= 4 && memcmp(data, "MBAC", 4) == 0);
    // Try real vertex extraction: BE s16 triplets after header
    // Layout guess: [0..3 magic][4..5 ver][6..7 nObj][8..9 nVert][10..11 nIdx][12.. verts s16 BE]
    auto be16 = [&](size_t o) -> int16_t {
        if (o + 1 >= size) return 0;
        return (int16_t)(((uint16_t)data[o] << 8) | data[o+1]);
    };
    size_t vcount = 0, voff = 16;
    if (isMbac && size >= 16) {
        int c1 = be16(8), c2 = be16(10), c3 = be16(12), c4 = be16(14);
        // pick most plausible vertex count (8..2000)
        int cand[4] = {c1, c2, c3, c4};
        for (int k = 0; k < 4; k++) {
            if (cand[k] > 0 && cand[k] <= 3000) {
                size_t need = 16 + (size_t)cand[k] * 6;
                if (need <= size && (size_t)cand[k] > vcount) { vcount = cand[k]; voff = 16 + k * 0; }
            }
        }
        // Common: nVert at offset 8
        if (c1 > 0 && c1 <= 3000 && 16 + (size_t)c1 * 6 <= size) { vcount = c1; voff = 16; }
    }
    if (vcount >= 3 && voff + vcount * 6 <= size) {
        // s16 fixed 1/256? Mascot uses 12.4? Normalize by bbox to [-1,1]
        std::vector<float> xs(vcount), ys(vcount), zs(vcount);
        for (size_t i = 0; i < vcount; i++) {
            xs[i] = be16(voff + i*6) / 256.0f;
            ys[i] = be16(voff + i*6 + 2) / 256.0f;
            zs[i] = be16(voff + i*6 + 4) / 256.0f;
        }
        float mnx=xs[0],mxx=xs[0],mny=ys[0],mxy=ys[0],mnz=zs[0],mxz=zs[0];
        for (size_t i=1;i<vcount;i++){ mnx=std::min(mnx,xs[i]); mxx=std::max(mxx,xs[i]); mny=std::min(mny,ys[i]); mxy=std::max(mxy,ys[i]); mnz=std::min(mnz,zs[i]); mxz=std::max(mxz,zs[i]); }
        float ex=mxx-mnx, ey=mxy-mny, ez=mxz-mnz;
        // Reject garbage (extreme range): try LE variant below instead of fake cube
        if (ex < 0.5f || ex > 2000.0f || ey < 0.5f || ey > 2000.0f || ez < 0.5f || ez > 2000.0f) {
            m_vertices.clear(); m_indices.clear();
        } else {
        float sc = 2.0f / std::max({ex, ey, ez});
        for (size_t i=0;i<vcount && i<3000;i++){
            M3GVertex vtx;
            vtx.position.x=(xs[i]-(mnx+mxx)*0.5f)*sc;
            vtx.position.y=(ys[i]-(mny+mxy)*0.5f)*sc;
            vtx.position.z=(zs[i]-(mnz+mxz)*0.5f)*sc;
            vtx.normal=Vec3(0,0,1); vtx.u=(i%2)?1.0f:0.0f; vtx.v=(i/2%2)?1.0f:0.0f; vtx.color=0xFFFFFFFF;
            m_vertices.push_back(vtx);
            m_indices.push_back((uint16_t)i);
        }
        while (m_indices.size()%3) m_indices.pop_back();
        if (m_vertices.size()>=3 && m_indices.size()>=3) return;
        m_vertices.clear(); m_indices.clear();
        // Retry little-endian s16 (some exporters)
        for (size_t i = 0; i < vcount && i < 3000; i++) {
            size_t o = voff + i*6;
            if (o + 5 >= size) break;
            float x = (int16_t)((uint16_t)data[o] | ((uint16_t)data[o+1] << 8)) / 256.0f;
            float y = (int16_t)((uint16_t)data[o+2] | ((uint16_t)data[o+3] << 8)) / 256.0f;
            float z = (int16_t)((uint16_t)data[o+4] | ((uint16_t)data[o+5] << 8)) / 256.0f;
            xs[i]=x; ys[i]=y; zs[i]=z;
        }
        mnx=xs[0];mxx=xs[0];mny=ys[0];mxy=ys[0];mnz=zs[0];mxz=zs[0];
        for (size_t i=1;i<vcount;i++){ mnx=std::min(mnx,xs[i]); mxx=std::max(mxx,xs[i]); mny=std::min(mny,ys[i]); mxy=std::max(mxy,ys[i]); mnz=std::min(mnz,zs[i]); mxz=std::max(mxz,zs[i]); }
        ex=mxx-mnx; ey=mxy-mny; ez=mxz-mnz;
        if (ex >= 0.5f && ex <= 2000.0f && ey >= 0.5f && ey <= 2000.0f && ez >= 0.5f && ez <= 2000.0f) {
            float sc2 = 2.0f / std::max({ex, ey, ez});
            for (size_t i=0;i<vcount && i<3000;i++){
                M3GVertex vtx;
                vtx.position.x=(xs[i]-(mnx+mxx)*0.5f)*sc2;
                vtx.position.y=(ys[i]-(mny+mxy)*0.5f)*sc2;
                vtx.position.z=(zs[i]-(mnz+mxz)*0.5f)*sc2;
                vtx.normal=Vec3(0,0,1); vtx.u=(i%2)?1.0f:0.0f; vtx.v=(i/2%2)?1.0f:0.0f; vtx.color=0xFFFFFFFF;
                m_vertices.push_back(vtx);
                m_indices.push_back((uint16_t)i);
            }
            while (m_indices.size()%3) m_indices.pop_back();
            if (m_vertices.size()>=3 && m_indices.size()>=3) return;
            m_vertices.clear(); m_indices.clear();
        }
    }
    }
    // Generic float-triplet scan (some MBAC store LE float): strict runs only
    if (size >= 48) {
        auto okF = [](float f)->bool{ if(!(f==f)) return false; float a=f<0?-f:f; if(a==0) return true; return a>=1e-4f&&a<=20.0f; };
        std::vector<float> pool; int run=0;
        for (size_t k=0;k+12<=size && pool.size()<9000;k+=4){
            float a,b,c; memcpy(&a,data+k,4); memcpy(&b,data+k+4,4); memcpy(&c,data+k+8,4);
            if (okF(a)&&okF(b)&&okF(c)) { pool.push_back(a); pool.push_back(b); pool.push_back(c); k+=8; run++; }
            else if (run>0&&run<2){ for(int r=0;r<run*3;r++) pool.pop_back(); run=0; }
            else run=0;
        }
        if (run>0&&run<2) for(int r=0;r<run*3;r++) pool.pop_back();
        if (pool.size()>=27){
            size_t nv=std::min<size_t>(pool.size()/3,2000);
            for(size_t i=0;i<nv;i++){ M3GVertex vtx; vtx.position=Vec3(pool[i*3]*0.1f,pool[i*3+1]*0.1f,pool[i*3+2]*0.1f); vtx.normal=Vec3(0,0,1); vtx.color=0xFFFFFFFF; m_vertices.push_back(vtx); m_indices.push_back((uint16_t)i); }
            while(m_indices.size()%3) m_indices.pop_back();
            if(m_vertices.size()>=3) return;
            m_vertices.clear(); m_indices.clear();
        }
    }
    // No fake geometry: unknown format renders nothing (caller draws empty)
}

void Micro3DFigure::setTexture(const Micro3DTexture& tex) {
    m_texturePixels.assign(tex.getPixels(), tex.getPixels() + tex.getWidth() * tex.getHeight());
    m_texW = tex.getWidth();
    m_texH = tex.getHeight();
}

void Micro3DFigure::setAction(const std::vector<uint8_t>& traData, int actionIndex) { m_frame = actionIndex * 8; }
void Micro3DFigure::setPosture(int frame) { m_frame = frame; }

void Micro3DFigure::draw(LcduiDisplay* target, const Micro3DAffineTrans& trans) {
    if (!target) return;
    M3GGraphics3D& g3d = M3GGraphics3D::getInstance();
    g3d.bindTarget(target);
    // Posture motion: gentle bob + yaw by frame so animated figures visibly move
    Mat4 base = trans.toMat4();
    if (m_frame != 0) {
        float a = (m_frame % 64) * 0.098f;
        Mat4 rot = Mat4::identity();
        float c = cosf(a), s = sinf(a);
        rot.m[0] = c; rot.m[2] = s; rot.m[8] = -s; rot.m[10] = c;
        rot.m[13] = sinf(a * 2.0f) * 0.08f;
        base = Mat4::multiply(base, rot);
    }
    const uint32_t* tex = m_texturePixels.empty() ? nullptr : m_texturePixels.data();
    g3d.renderMesh(m_vertices, m_indices, base, tex, m_texW, m_texH);
    g3d.releaseTarget();
}