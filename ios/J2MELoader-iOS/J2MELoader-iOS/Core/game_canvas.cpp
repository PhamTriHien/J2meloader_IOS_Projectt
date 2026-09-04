#include "game_canvas.h"
#include <algorithm>
#include <cmath>

Sprite::Sprite(const std::vector<uint32_t>& imagePixels, int imgWidth, int imgHeight, int frameWidth, int frameHeight)
    : m_imagePixels(imagePixels), m_imageWidth(imgWidth), m_imageHeight(imgHeight),
      m_frameWidth(frameWidth), m_frameHeight(frameHeight) {
    m_width = frameWidth;
    m_height = frameHeight;
    m_numFrames = (imgWidth / frameWidth) * (imgHeight / frameHeight);
    m_sequence.resize(m_numFrames);
    for (int i = 0; i < m_numFrames; ++i) m_sequence[i] = i;
    m_collW = frameWidth;
    m_collH = frameHeight;
}

void Sprite::setFrame(int sequenceIndex) {
    if (sequenceIndex >= 0 && sequenceIndex < (int)m_sequence.size()) {
        m_currentFrameIndex = sequenceIndex;
    }
}

void Sprite::setFrameSequence(const std::vector<int>& sequence) {
    m_sequence = sequence;
    m_currentFrameIndex = 0;
}

void Sprite::nextFrame() {
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_sequence.size();
}

void Sprite::prevFrame() {
    m_currentFrameIndex = (m_currentFrameIndex - 1 + (int)m_sequence.size()) % m_sequence.size();
}

void Sprite::setTransform(SpriteTransform transform) {
    m_transform = transform;
    if (transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
        transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270) {
        m_width = m_frameHeight;
        m_height = m_frameWidth;
    } else {
        m_width = m_frameWidth;
        m_height = m_frameHeight;
    }
}

void Sprite::defineReferencePixel(int x, int y) {
    m_refX = x;
    m_refY = y;
}

void Sprite::setRefPixelPosition(int x, int y) {
    m_x = x - m_refX;
    m_y = y - m_refY;
}

int Sprite::getRefPixelX() const { return m_x + m_refX; }
int Sprite::getRefPixelY() const { return m_y + m_refY; }

void Sprite::defineCollisionRectangle(int x, int y, int width, int height) {
    m_collX = x;
    m_collY = y;
    m_collW = width;
    m_collH = height;
    m_customCollision = true;
}

uint32_t Sprite::getPixel(int frame, int localX, int localY, SpriteTransform transform) const {
    if (localX < 0 || localX >= m_width || localY < 0 || localY >= m_height) return 0;

    int srcX = localX, srcY = localY;
    switch (transform) {
    case TRANS_NONE: break;
    case TRANS_MIRROR: srcX = m_frameWidth - 1 - localX; break;
    case TRANS_ROT180: srcX = m_frameWidth - 1 - localX; srcY = m_frameHeight - 1 - localY; break;
    case TRANS_ROT90: srcX = localY; srcY = m_frameWidth - 1 - localX; break;
    case TRANS_ROT270: srcX = m_frameHeight - 1 - localY; srcY = localX; break;
    case TRANS_MIRROR_ROT90: srcX = localY; srcY = localX; break;
    case TRANS_MIRROR_ROT180: srcX = localX; srcY = m_frameHeight - 1 - localY; break;
    case TRANS_MIRROR_ROT270: srcX = m_frameHeight - 1 - localY; srcY = m_frameWidth - 1 - localX; break;
    }

    int framesPerRow = m_imageWidth / m_frameWidth;
    int fx = (frame % framesPerRow) * m_frameWidth;
    int fy = (frame / framesPerRow) * m_frameHeight;

    int gx = fx + srcX;
    int gy = fy + srcY;
    if (gx < 0 || gx >= m_imageWidth || gy < 0 || gy >= m_imageHeight) return 0;
    return m_imagePixels[gy * m_imageWidth + gx];
}

bool Sprite::collidesWith(int x, int y, int width, int height) const {
    int ax1 = m_x + (m_customCollision ? m_collX : 0);
    int ay1 = m_y + (m_customCollision ? m_collY : 0);
    int ax2 = ax1 + (m_customCollision ? m_collW : m_width);
    int ay2 = ay1 + (m_customCollision ? m_collH : m_height);

    int bx1 = x, by1 = y, bx2 = x + width, by2 = y + height;
    return !(ax2 <= bx1 || ax1 >= bx2 || ay2 <= by1 || ay1 >= by2);
}

bool Sprite::collidesWith(const Sprite& other, bool pixelLevel) const {
    if (!collidesWith(other.m_x, other.m_y, other.m_width, other.m_height)) return false;
    if (!pixelLevel) return true;

    int intersectX1 = std::max(m_x, other.m_x);
    int intersectY1 = std::max(m_y, other.m_y);
    int intersectX2 = std::min(m_x + m_width, other.m_x + other.m_width);
    int intersectY2 = std::min(m_y + m_height, other.m_y + other.m_height);

    int frameA = m_sequence[m_currentFrameIndex];
    int frameB = other.m_sequence[other.m_currentFrameIndex];

    for (int cy = intersectY1; cy < intersectY2; ++cy) {
        for (int cx = intersectX1; cx < intersectX2; ++cx) {
            uint32_t pA = getPixel(frameA, cx - m_x, cy - m_y, m_transform);
            uint32_t pB = other.getPixel(frameB, cx - other.m_x, cy - other.m_y, other.m_transform);

            // Both pixels must be non-transparent (Alpha > 128)
            if (((pA >> 24) & 0xFF) > 128 && ((pB >> 24) & 0xFF) > 128) {
                return true;
            }
        }
    }
    return false;
}

void Sprite::paint(LcduiDisplay* display) {
    if (!m_visible || m_sequence.empty()) return;
    int frame = m_sequence[m_currentFrameIndex];

    for (int r = 0; r < m_height; ++r) {
        for (int c = 0; c < m_width; ++c) {
            uint32_t pixel = getPixel(frame, c, r, m_transform);
            if (((pixel >> 24) & 0xFF) > 0) {
                display->drawRGB((const int32_t*)&pixel, 0, 1, m_x + c, m_y + r, 1, 1, true);
            }
        }
    }
}

TiledLayer::TiledLayer(int columns, int rows, const std::vector<uint32_t>& tileImage, int imgW, int imgH, int tileW, int tileH)
    : m_cols(columns), m_rows(rows), m_tileWidth(tileW), m_tileHeight(tileH),
      m_tileImage(tileImage), m_imgWidth(imgW), m_imgHeight(imgH) {
    m_width = columns * tileW;
    m_height = rows * tileH;
    m_cells.resize(columns * rows, 0);
    m_numStaticTiles = (imgW / tileW) * (imgH / tileH);
}

void TiledLayer::setCell(int col, int row, int tileIndex) {
    if (col >= 0 && col < m_cols && row >= 0 && row < m_rows) {
        m_cells[row * m_cols + col] = tileIndex;
    }
}

int TiledLayer::getCell(int col, int row) const {
    if (col >= 0 && col < m_cols && row >= 0 && row < m_rows) {
        return m_cells[row * m_cols + col];
    }
    return 0;
}

void TiledLayer::fillCells(int col, int row, int numCols, int numRows, int tileIndex) {
    for (int r = row; r < row + numRows && r < m_rows; ++r) {
        for (int c = col; c < col + numCols && c < m_cols; ++c) {
            m_cells[r * m_cols + c] = tileIndex;
        }
    }
}

int TiledLayer::createAnimatedTile(int staticTileIndex) {
    m_animatedTiles.push_back(staticTileIndex);
    return -(int)m_animatedTiles.size(); // Animated tiles have negative IDs: -1, -2, ...
}

void TiledLayer::setAnimatedTile(int animatedTileIndex, int staticTileIndex) {
    int idx = (-animatedTileIndex) - 1;
    if (idx >= 0 && idx < (int)m_animatedTiles.size()) {
        m_animatedTiles[idx] = staticTileIndex;
    }
}

int TiledLayer::getAnimatedTile(int animatedTileIndex) const {
    int idx = (-animatedTileIndex) - 1;
    if (idx >= 0 && idx < (int)m_animatedTiles.size()) {
        return m_animatedTiles[idx];
    }
    return 0;
}

void TiledLayer::paint(LcduiDisplay* display) {
    if (!m_visible) return;
    int tilesPerRow = m_imgWidth / m_tileWidth;

    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            int tile = m_cells[r * m_cols + c];
            if (tile < 0) { // Animated tile
                tile = getAnimatedTile(tile);
            }
            if (tile == 0) continue; // 0 is transparent / empty

            int tileIdx = tile - 1;
            int tx = (tileIdx % tilesPerRow) * m_tileWidth;
            int ty = (tileIdx / tilesPerRow) * m_tileHeight;

            int destX = m_x + c * m_tileWidth;
            int destY = m_y + r * m_tileHeight;

            for (int tr = 0; tr < m_tileHeight; ++tr) {
                int srcOffset = (ty + tr) * m_imgWidth + tx;
                display->drawRGB((const int32_t*)&m_tileImage[srcOffset], 0, m_imgWidth, destX, destY + tr, m_tileWidth, 1, true);
            }
        }
    }
}

LayerManager::LayerManager() {}

void LayerManager::append(std::shared_ptr<Layer> layer) {
    m_layers.push_back(layer);
}

void LayerManager::insert(std::shared_ptr<Layer> layer, int index) {
    if (index >= 0 && index <= (int)m_layers.size()) {
        m_layers.insert(m_layers.begin() + index, layer);
    }
}

void LayerManager::remove(std::shared_ptr<Layer> layer) {
    auto it = std::find(m_layers.begin(), m_layers.end(), layer);
    if (it != m_layers.end()) m_layers.erase(it);
}

void LayerManager::setViewWindow(int x, int y, int width, int height) {
    m_viewX = x;
    m_viewY = y;
    m_viewW = width;
    m_viewH = height;
}

void LayerManager::paint(LcduiDisplay* display, int x, int y) {
    display->setClip(x, y, m_viewW, m_viewH);
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        if ((*it)->isVisible()) {
            int origX = (*it)->getX();
            int origY = (*it)->getY();
            (*it)->setPosition(origX - m_viewX + x, origY - m_viewY + y);
            (*it)->paint(display);
            (*it)->setPosition(origX, origY);
        }
    }
    display->resetClip();
}