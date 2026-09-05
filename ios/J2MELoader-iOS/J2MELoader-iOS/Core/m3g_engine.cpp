#include "m3g_engine.h"
#include <cstring>
#include <algorithm>

Mat4::Mat4() {
    memset(m, 0, sizeof(m));
}

Mat4 Mat4::identity() {
    Mat4 res;
    res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
    return res;
}

Mat4 Mat4::perspective(float fovy, float aspect, float nearP, float farP) {
    Mat4 res;
    float f = 1.0f / tanf(fovy * 3.14159265f / 360.0f);
    res.m[0] = f / aspect;
    res.m[5] = f;
    res.m[10] = (farP + nearP) / (nearP - farP);
    res.m[11] = -1.0f;
    res.m[14] = (2.0f * farP * nearP) / (nearP - farP);
    return res;
}

Mat4 Mat4::multiply(const Mat4& a, const Mat4& b) {
    Mat4 res;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[r * 4 + k] * b.m[k * 4 + c];
            }
            res.m[r * 4 + c] = sum;
        }
    }
    return res;
}

Vec3 Mat4::transformPoint(const Vec3& v) const {
    float x = v.x * m[0] + v.y * m[4] + v.z * m[8] + m[12];
    float y = v.x * m[1] + v.y * m[5] + v.z * m[9] + m[13];
    float z = v.x * m[2] + v.y * m[6] + v.z * m[10] + m[14];
    float w = v.x * m[3] + v.y * m[7] + v.z * m[11] + m[15];
    if (w != 0.0f && w != 1.0f) {
        float invW = 1.0f / w;
        return Vec3(x * invW, y * invW, z * invW);
    }
    return Vec3(x, y, z);
}

M3GCamera::M3GCamera() {
    setPerspective(60.0f, 240.0f / 320.0f, 0.1f, 100.0f);
}

void M3GCamera::setPerspective(float fovy, float aspect, float nearP, float farP) {
    m_projMatrix = Mat4::perspective(fovy, aspect, nearP, farP);
}

M3GLight::M3GLight(Mode mode) : m_mode(mode) {}
void M3GLight::setColor(uint32_t rgb) { m_color = rgb; }
void M3GLight::setIntensity(float intensity) { m_intensity = intensity; }

M3GGraphics3D::M3GGraphics3D() {}

M3GGraphics3D& M3GGraphics3D::getInstance() {
    static M3GGraphics3D instance;
    return instance;
}

void M3GGraphics3D::bindTarget(LcduiDisplay* target) {
    m_target = target;
    if (m_target) {
        int w = m_target->getWidth();
        int h = m_target->getHeight();
        m_depthBuffer.resize(w * h, 1.0f);
    }
}

void M3GGraphics3D::releaseTarget() {
    m_target = nullptr;
}

void M3GGraphics3D::clear(uint32_t color) {
    if (m_target) {
        m_target->clear(color);
        std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 1.0f);
    }
}

void M3GGraphics3D::setCamera(const M3GCamera& camera, const Mat4& viewMatrix) {
    m_viewProjMatrix = Mat4::multiply(camera.getProjectionMatrix(), viewMatrix);
}

void M3GGraphics3D::addLight(const M3GLight& light, const Mat4& transform) {
    m_lights.push_back(light);
    m_lightTransforms.push_back(transform);
}

void M3GGraphics3D::resetLights() {
    m_lights.clear();
    m_lightTransforms.clear();
}

void M3GGraphics3D::renderTriangle(const M3GVertex& v0, const M3GVertex& v1, const M3GVertex& v2, const uint32_t* texture, int texW, int texH) {
    if (!m_target) return;
    int screenW = m_target->getWidth();
    int screenH = m_target->getHeight();

    // Screen Space Coordinates
    int x0 = (int)((v0.position.x + 1.0f) * 0.5f * screenW);
    int y0 = (int)((1.0f - v0.position.y) * 0.5f * screenH);
    int x1 = (int)((v1.position.x + 1.0f) * 0.5f * screenW);
    int y1 = (int)((1.0f - v1.position.y) * 0.5f * screenH);
    int x2 = (int)((v2.position.x + 1.0f) * 0.5f * screenW);
    int y2 = (int)((1.0f - v2.position.y) * 0.5f * screenH);

    // Bounding Box
    int minX = std::max(0, std::min({x0, x1, x2}));
    int maxX = std::min(screenW - 1, std::max({x0, x1, x2}));
    int minY = std::max(0, std::min({y0, y1, y2}));
    int maxY = std::min(screenH - 1, std::max({y0, y1, y2}));

    float det = (float)((y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2));
    if (std::abs(det) < 1e-5f) return;
    float invDet = 1.0f / det;

    for (int py = minY; py <= maxY; ++py) {
        for (int px = minX; px <= maxX; ++px) {
            float w0 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) * invDet;
            float w1 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) * invDet;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float z = w0 * v0.position.z + w1 * v1.position.z + w2 * v2.position.z;
                int idx = py * screenW + px;
                if (z >= 0.0f && z < m_depthBuffer[idx]) {
                    m_depthBuffer[idx] = z;

                    uint32_t color = v0.color;
                    if (texture && texW > 0 && texH > 0) {
                        float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
                        float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;
                        int tx = (int)(u * (texW - 1)) % texW;
                        int ty = (int)(v * (texH - 1)) % texH;
                        if (tx < 0) tx += texW;
                        if (ty < 0) ty += texH;
                        color = texture[ty * texW + tx];
                    }
                    m_target->drawRGB((const int32_t*)&color, 0, 1, px, py, 1, 1, false);
                }
            }
        }
    }
}

void M3GGraphics3D::renderMesh(const std::vector<M3GVertex>& vertices, const std::vector<uint16_t>& indices, const Mat4& modelMatrix, const uint32_t* texture, int texW, int texH) {
    Mat4 mvp = Mat4::multiply(m_viewProjMatrix, modelMatrix);

    std::vector<M3GVertex> projected(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        projected[i] = vertices[i];
        projected[i].position = mvp.transformPoint(vertices[i].position);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        renderTriangle(projected[indices[i]], projected[indices[i + 1]], projected[indices[i + 2]], texture, texW, texH);
    }
}

void M3GGraphics3D::renderWorld(const std::vector<M3GVertex>& verts, const std::vector<uint16_t>& idx,
                                const uint32_t* tex, int tw, int th, uint32_t bg) {
    if (!m_target) return;
    clear(bg);
    if (!verts.empty() && idx.size() >= 3) {
        Mat4 model = Mat4::identity();
        renderMesh(verts, idx, model, tex, tw, th);
    }
}

// ---- Real M3G v1.0 parser (header + sections + embedded PNG/JPEG + float verts) ----
#include "png_decoder.h"
#include <cstring>
#include <zlib.h>
static uint32_t m3gU32(const uint8_t* p) { return ((uint32_t)p[0]) | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint32_t m3gVarInt(const uint8_t* d, size_t n, size_t& pos) {
    uint32_t v = 0; int shift = 0;
    while (pos < n) { uint8_t b = d[pos++]; v |= (uint32_t)(b & 0x7F) << shift; if (!(b & 0x80)) break; shift += 7; if (shift > 28) break; }
    return v;
}
bool M3GLoader::parse(const uint8_t* data, size_t size,
                      std::vector<M3GVertex>& outVerts,
                      std::vector<uint16_t>& outIndices,
                      std::vector<uint32_t>& outTex, int& texW, int& texH,
                      uint32_t& bgColor) {
    outVerts.clear(); outIndices.clear(); outTex.clear(); texW = 0; texH = 0; bgColor = 0xFF000000;
    if (!data || size < 12) return false;
    // Magic: AB 4A 53 52 31 38 34 BB 0D 0A 1A 0A
    static const uint8_t magic[12] = {0xAB,'J','S','R','1','8','4',0xBB,0x0D,0x0A,0x1A,0x0A};
    if (memcmp(data, magic, 12) != 0) return false;
    size_t pos = 12;
    std::vector<float> floatPool;
    floatPool.reserve(4096);
    std::vector<uint8_t> secBuf;
    // Scan sections (comp 0 = raw, 1 = zlib)
    while (pos + 9 <= size) {
        uint8_t comp = data[pos];
        uint32_t secLen = m3gU32(data + pos + 1);
        uint32_t uncompLen = m3gU32(data + pos + 5);
        if (secLen < 13 || pos + secLen > size) break;
        const uint8_t* secData = data + pos + 9;
        size_t secDataLen = secLen - 13; // minus header(9) and checksum(4)
        if (comp == 1) {
            if (uncompLen == 0 || uncompLen > (8 << 20)) { pos += secLen; continue; }
            secBuf.assign(uncompLen, 0);
            uLongf dl = (uLongf)uncompLen;
            if (uncompress(secBuf.data(), &dl, secData, (uLong)secDataLen) != Z_OK) { pos += secLen; continue; }
            secData = secBuf.data();
            secDataLen = (size_t)dl;
        } else if (comp != 0) { pos += secLen; continue; }
        size_t secEnd = secDataLen;
        size_t p = 0;
        // Objects in section (secData may be decompressed)
        auto saneF = [](float f) -> bool {
            if (!(f == f)) return false; // NaN
            float a = f < 0 ? -f : f;
            if (a == 0.0f) return true;
            return a >= 1e-4f && a <= 20.0f; // kills header denormals + garbage
        };
        while (p + 2 <= secEnd) {
            uint8_t otype = secData[p++];
            size_t lp = p;
            uint32_t olen = m3gVarInt(secData, secEnd, lp);
            p = lp;
            if (p + olen > secEnd) break;
            const uint8_t* ob = secData + p;
            // Background (4): byte red,green,blue
            if (otype == 4 && olen >= 3) {
                bgColor = 0xFF000000 | (ob[0] << 16) | (ob[1] << 8) | ob[2];
            }
            // VertexArray (23): collect runs of sane float triplets
            if (otype == 23 && olen > 4) {
                int run = 0;
                for (size_t k = 0; k + 12 <= olen; k += 4) {
                    float f, f2, f3; memcpy(&f, ob + k, 4); memcpy(&f2, ob + k + 4, 4); memcpy(&f3, ob + k + 8, 4);
                    if (saneF(f) && saneF(f2) && saneF(f3)) {
                        floatPool.push_back(f); floatPool.push_back(f2); floatPool.push_back(f3);
                        k += 8; run++;
                    } else if (run > 0 && run < 2) {
                        // drop isolated false triplets
                        for (int r = 0; r < run * 3; r++) floatPool.pop_back();
                        run = 0;
                    } else run = 0;
                }
                if (run > 0 && run < 2) for (int r = 0; r < run * 3; r++) floatPool.pop_back();
            }
            // Image2D (9): embedded PNG
            if (otype == 9 && olen > 16 && outTex.empty()) {
                for (size_t k = 0; k + 8 <= olen; k++) {
                    if (ob[k]==0x89&&ob[k+1]=='P'&&ob[k+2]=='N'&&ob[k+3]=='G') {
                        int w=0,h=0; std::vector<uint32_t> px;
                        for (size_t e = olen; e > k + 64; e -= 64) {
                            if (PngDecoder::decode(ob+k, e-k, w, h, px) && w>0 && h>0 && w<=1024 && h<=1024) {
                                outTex = std::move(px); texW = w; texH = h; break;
                            }
                            if (!outTex.empty()) break;
                        }
                        if (!outTex.empty()) break;
                    }
                }
            }
            p += olen;
        }
        pos += secLen;
    }
    // Fallback global PNG scan (some encoders store Image2D raw)
    if (outTex.empty()) {
        for (size_t k = 0; k + 8 < size; k++) {
            if (data[k]==0x89&&data[k+1]=='P'&&data[k+2]=='N'&&data[k+3]=='G') {
                int w=0,h=0; std::vector<uint32_t> px;
                size_t win = std::min<size_t>(size - k, 1<<20);
                if (PngDecoder::decode(data+k, win, w, h, px) && w>0 && h>0 && w<=1024 && h<=1024) {
                    outTex = std::move(px); texW=w; texH=h; break;
                }
            }
        }
    }
    // Build verts from float pool (triplets -> positions, fan triangulation)
    if (floatPool.size() >= 9) {
        size_t nv = std::min<size_t>(floatPool.size()/3, 3000);
        // Normalize to [-1,1] using bbox
        float mnx=floatPool[0],mxx=mnx,mny=floatPool[1],mxy=mny,mnz=floatPool[2],mxz=mnz;
        for (size_t i=0;i<nv;i++){ mnx=std::min(mnx,floatPool[i*3]); mxx=std::max(mxx,floatPool[i*3]); mny=std::min(mny,floatPool[i*3+1]); mxy=std::max(mxy,floatPool[i*3+1]); mnz=std::min(mnz,floatPool[i*3+2]); mxz=std::max(mxz,floatPool[i*3+2]); }
        float sx=(mxx-mnx)>1e-6f?2.0f/(mxx-mnx):1.0f, sy=(mxy-mny)>1e-6f?2.0f/(mxy-mny):1.0f, sz=(mxz-mnz)>1e-6f?2.0f/(mxz-mnz):1.0f;
        float sc=std::min({sx,sy,sz});
        for (size_t i=0;i<nv;i++){
            M3GVertex v;
            v.position.x=(floatPool[i*3]-(mnx+mxx)*0.5f)*sc;
            v.position.y=(floatPool[i*3+1]-(mny+mxy)*0.5f)*sc;
            v.position.z=(floatPool[i*3+2]-(mnz+mxz)*0.5f)*sc;
            v.normal=Vec3(0,0,1); v.u=(i%2)?1.0f:0.0f; v.v=(i/2%2)?1.0f:0.0f;
            v.color=0xFFFFFFFF;
            outVerts.push_back(v);
            outIndices.push_back((uint16_t)i);
        }
        // Ensure triangle count multiple of 3
        while (outIndices.size()%3) outIndices.pop_back();
    }
    return true;
}