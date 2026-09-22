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
    computeTransformedBounds(m_transform);
}

void Sprite::computeTransformedBounds(SpriteTransform transform) {
    int cX = m_customCollision ? m_collX : 0;
    int cY = m_customCollision ? m_collY : 0;
    int cW = m_customCollision ? m_collW : m_frameWidth;
    int cH = m_customCollision ? m_collH : m_frameHeight;

    switch (transform) {
    case TRANS_NONE:
        m_tCollX = cX;
        m_tCollY = cY;
        m_tCollW = cW;
        m_tCollH = cH;
        break;
    case TRANS_MIRROR:
        m_tCollX = m_frameWidth - (cX + cW);
        m_tCollY = cY;
        m_tCollW = cW;
        m_tCollH = cH;
        break;
    case TRANS_MIRROR_ROT180:
        m_tCollY = m_frameHeight - (cY + cH);
        m_tCollX = cX;
        m_tCollW = cW;
        m_tCollH = cH;
        break;
    case TRANS_ROT90:
        m_tCollX = m_frameHeight - (cH + cY);
        m_tCollY = cX;
        m_tCollW = cH;
        m_tCollH = cW;
        break;
    case TRANS_ROT180:
        m_tCollX = m_frameWidth - (cW + cX);
        m_tCollY = m_frameHeight - (cH + cY);
        m_tCollW = cW;
        m_tCollH = cH;
        break;
    case TRANS_ROT270:
        m_tCollX = cY;
        m_tCollY = m_frameWidth - (cW + cX);
        m_tCollW = cH;
        m_tCollH = cW;
        break;
    case TRANS_MIRROR_ROT90:
        m_tCollX = m_frameHeight - (cH + cY);
        m_tCollY = m_frameWidth - (cW + cX);
        m_tCollW = cH;
        m_tCollH = cW;
        break;
    case TRANS_MIRROR_ROT270:
        m_tCollY = cX;
        m_tCollX = cY;
        m_tCollW = cH;
        m_tCollH = cW;
        break;
    }
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
    if (!m_sequence.empty()) {
        m_currentFrameIndex = (m_currentFrameIndex + 1) % m_sequence.size();
    }
}

void Sprite::prevFrame() {
    if (!m_sequence.empty()) {
        m_currentFrameIndex = (m_currentFrameIndex - 1 + (int)m_sequence.size()) % m_sequence.size();
    }
}

int Sprite::getTransformedPtX(int x, int y, SpriteTransform transform) const {
    switch (transform) {
    case TRANS_NONE: return x;
    case TRANS_MIRROR: return m_frameWidth - x - 1;
    case TRANS_MIRROR_ROT180: return x;
    case TRANS_ROT90: return m_frameHeight - y - 1;
    case TRANS_ROT180: return m_frameWidth - x - 1;
    case TRANS_ROT270: return y;
    case TRANS_MIRROR_ROT90: return m_frameHeight - y - 1;
    case TRANS_MIRROR_ROT270: return y;
    default: return x;
    }
}

int Sprite::getTransformedPtY(int x, int y, SpriteTransform transform) const {
    switch (transform) {
    case TRANS_NONE: return y;
    case TRANS_MIRROR: return y;
    case TRANS_MIRROR_ROT180: return m_frameHeight - y - 1;
    case TRANS_ROT90: return x;
    case TRANS_ROT180: return m_frameHeight - y - 1;
    case TRANS_ROT270: return m_frameWidth - x - 1;
    case TRANS_MIRROR_ROT90: return m_frameWidth - x - 1;
    case TRANS_MIRROR_ROT270: return x;
    default: return y;
    }
}

void Sprite::setTransform(SpriteTransform transform) {
    int oldRefX = getTransformedPtX(m_refX, m_refY, m_transform);
    int oldRefY = getTransformedPtY(m_refX, m_refY, m_transform);

    m_transform = transform;
    if (transform == TRANS_ROT90 || transform == TRANS_ROT270 ||
        transform == TRANS_MIRROR_ROT90 || transform == TRANS_MIRROR_ROT270) {
        m_width = m_frameHeight;
        m_height = m_frameWidth;
    } else {
        m_width = m_frameWidth;
        m_height = m_frameHeight;
    }

    computeTransformedBounds(m_transform);

    int newRefX = getTransformedPtX(m_refX, m_refY, m_transform);
    int newRefY = getTransformedPtY(m_refX, m_refY, m_transform);
    m_x += (oldRefX - newRefX);
    m_y += (oldRefY - newRefY);
}

void Sprite::defineReferencePixel(int x, int y) {
    m_refX = x;
    m_refY = y;
}

void Sprite::setRefPixelPosition(int x, int y) {
    m_x = x - getTransformedPtX(m_refX, m_refY, m_transform);
    m_y = y - getTransformedPtY(m_refX, m_refY, m_transform);
}

int Sprite::getRefPixelX() const {
    return m_x + getTransformedPtX(m_refX, m_refY, m_transform);
}

int Sprite::getRefPixelY() const {
    return m_y + getTransformedPtY(m_refX, m_refY, m_transform);
}

void Sprite::defineCollisionRectangle(int x, int y, int width, int height) {
    m_collX = x;
    m_collY = y;
    m_collW = width;
    m_collH = height;
    m_customCollision = true;
    computeTransformedBounds(m_transform);
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
    case TRANS_MIRROR_ROT90: srcX = m_frameHeight - 1 - localY; srcY = m_frameWidth - 1 - localX; break;
    case TRANS_MIRROR_ROT270: srcX = localY; srcY = localX; break;
    case TRANS_MIRROR_ROT180: srcX = localX; srcY = m_frameHeight - 1 - localY; break;
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
    if (!m_visible) return false;
    int ax1 = m_x + m_tCollX;
    int ay1 = m_y + m_tCollY;
    int ax2 = ax1 + m_tCollW;
    int ay2 = ay1 + m_tCollH;

    int bx1 = x, by1 = y, bx2 = x + width, by2 = y + height;
    return !(ax2 <= bx1 || ax1 >= bx2 || ay2 <= by1 || ay1 >= by2);
}

bool Sprite::collidesWith(const Sprite& other, bool pixelLevel) const {
    if (!m_visible || !other.m_visible) return false;

    int otherLeft = other.m_x + other.m_tCollX;
    int otherTop = other.m_y + other.m_tCollY;
    int otherRight = otherLeft + other.m_tCollW;
    int otherBottom = otherTop + other.m_tCollH;

    int left = m_x + m_tCollX;
    int top = m_y + m_tCollY;
    int right = left + m_tCollW;
    int bottom = top + m_tCollH;

    if (otherRight <= left || otherLeft >= right || otherBottom <= top || otherTop >= bottom) {
        return false;
    }

    if (!pixelLevel) return true;

    if (m_tCollX < 0) left = m_x;
    if (m_tCollY < 0) top = m_y;
    if (m_tCollX + m_tCollW > m_width) right = m_x + m_width;
    if (m_tCollY + m_tCollH > m_height) bottom = m_y + m_height;

    if (other.m_tCollX < 0) otherLeft = other.m_x;
    if (other.m_tCollY < 0) otherTop = other.m_y;
    if (other.m_tCollX + other.m_tCollW > other.m_width) otherRight = other.m_x + other.m_width;
    if (other.m_tCollY + other.m_tCollH > other.m_height) otherBottom = other.m_y + other.m_height;

    int intersectX1 = std::max(left, otherLeft);
    int intersectY1 = std::max(top, otherTop);
    int intersectX2 = std::min(right, otherRight);
    int intersectY2 = std::min(bottom, otherBottom);

    if (intersectX1 >= intersectX2 || intersectY1 >= intersectY2) return false;

    int frameA = (m_currentFrameIndex >= 0 && m_currentFrameIndex < (int)m_sequence.size()) ? m_sequence[m_currentFrameIndex] : 0;
    int frameB = (other.m_currentFrameIndex >= 0 && other.m_currentFrameIndex < (int)other.m_sequence.size()) ? other.m_sequence[other.m_currentFrameIndex] : 0;

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

bool Sprite::collidesWith(const TiledLayer& t, bool pixelLevel) const {
    if (!m_visible || !t.isVisible()) return false;

    int tLx1 = t.getX();
    int tLy1 = t.getY();
    int tLx2 = tLx1 + t.getWidth();
    int tLy2 = tLy1 + t.getHeight();

    int tW = t.getCellWidth();
    int tH = t.getCellHeight();
    if (tW <= 0 || tH <= 0) return false;

    int sx1 = m_x + m_tCollX;
    int sy1 = m_y + m_tCollY;
    int sx2 = sx1 + m_tCollW;
    int sy2 = sy1 + m_tCollH;

    if (sx2 <= tLx1 || sx1 >= tLx2 || sy2 <= tLy1 || sy1 >= tLy2) return false;

    int tNumCols = t.getColumns();
    int tNumRows = t.getRows();

    int startCol = (sx1 <= tLx1) ? 0 : (sx1 - tLx1) / tW;
    int startRow = (sy1 <= tLy1) ? 0 : (sy1 - tLy1) / tH;
    int endCol = (sx2 < tLx2) ? ((sx2 - 1 - tLx1) / tW) : tNumCols - 1;
    int endRow = (sy2 < tLy2) ? ((sy2 - 1 - tLy1) / tH) : tNumRows - 1;

    if (startCol < 0) startCol = 0;
    if (startRow < 0) startRow = 0;
    if (endCol >= tNumCols) endCol = tNumCols - 1;
    if (endRow >= tNumRows) endRow = tNumRows - 1;

    if (!pixelLevel) {
        for (int r = startRow; r <= endRow; ++r) {
            for (int c = startCol; c <= endCol; ++c) {
                if (t.getCell(c, r) != 0) return true;
            }
        }
        return false;
    }

    int frame = (m_currentFrameIndex >= 0 && m_currentFrameIndex < (int)m_sequence.size()) ? m_sequence[m_currentFrameIndex] : 0;
    for (int r = startRow; r <= endRow; ++r) {
        for (int c = startCol; c <= endCol; ++c) {
            int tile = t.getCell(c, r);
            if (tile == 0) continue;
            if (tile < 0) tile = t.getAnimatedTile(tile);
            if (tile <= 0) continue;

            int cellX1 = tLx1 + c * tW;
            int cellY1 = tLy1 + r * tH;
            int cellX2 = cellX1 + tW;
            int cellY2 = cellY1 + tH;

            int ix1 = std::max(sx1, cellX1);
            int iy1 = std::max(sy1, cellY1);
            int ix2 = std::min(sx2, cellX2);
            int iy2 = std::min(sy2, cellY2);

            for (int cy = iy1; cy < iy2; ++cy) {
                for (int cx = ix1; cx < ix2; ++cx) {
                    uint32_t pA = getPixel(frame, cx - m_x, cy - m_y, m_transform);
                    if (((pA >> 24) & 0xFF) > 128) {
                        uint32_t pB = t.getPixel(tile, cx - cellX1, cy - cellY1);
                        if (((pB >> 24) & 0xFF) > 128) return true;
                    }
                }
            }
        }
    }
    return false;
}

bool Sprite::collidesWith(const std::vector<uint32_t>& imgPixels, int imgW, int imgH, int imgX, int imgY, bool pixelLevel) const {
    if (!m_visible || imgPixels.empty() || imgW <= 0 || imgH <= 0) return false;

    int otherLeft = imgX;
    int otherTop = imgY;
    int otherRight = imgX + imgW;
    int otherBottom = imgY + imgH;

    int left = m_x + m_tCollX;
    int top = m_y + m_tCollY;
    int right = left + m_tCollW;
    int bottom = top + m_tCollH;

    if (otherRight <= left || otherLeft >= right || otherBottom <= top || otherTop >= bottom) return false;

    if (!pixelLevel) return true;

    if (m_tCollX < 0) left = m_x;
    if (m_tCollY < 0) top = m_y;
    if (m_tCollX + m_tCollW > m_width) right = m_x + m_width;
    if (m_tCollY + m_tCollH > m_height) bottom = m_y + m_height;

    int intersectX1 = std::max(left, otherLeft);
    int intersectY1 = std::max(top, otherTop);
    int intersectX2 = std::min(right, otherRight);
    int intersectY2 = std::min(bottom, otherBottom);

    if (intersectX1 >= intersectX2 || intersectY1 >= intersectY2) return false;

    int frame = (m_currentFrameIndex >= 0 && m_currentFrameIndex < (int)m_sequence.size()) ? m_sequence[m_currentFrameIndex] : 0;
    for (int cy = intersectY1; cy < intersectY2; ++cy) {
        for (int cx = intersectX1; cx < intersectX2; ++cx) {
            uint32_t pA = getPixel(frame, cx - m_x, cy - m_y, m_transform);
            if (((pA >> 24) & 0xFF) > 128) {
                int ix = cx - imgX;
                int iy = cy - imgY;
                if (ix >= 0 && ix < imgW && iy >= 0 && iy < imgH) {
                    uint32_t pB = imgPixels[(size_t)iy * imgW + ix];
                    if (((pB >> 24) & 0xFF) > 128) return true;
                }
            }
        }
    }
    return false;
}

void Sprite::paint(LcduiDisplay* display) {
    if (!m_visible || m_sequence.empty() || m_imagePixels.empty() || !display) return;
    int frame = m_sequence[m_currentFrameIndex];
    int framesPerRow = m_imageWidth / m_frameWidth;
    if (framesPerRow <= 0) framesPerRow = 1;
    int fx = (frame % framesPerRow) * m_frameWidth;
    int fy = (frame / framesPerRow) * m_frameHeight;

    display->drawRegion(m_imagePixels.data(), m_imageWidth, m_imageHeight, fx, fy, m_frameWidth, m_frameHeight, m_transform, m_x, m_y, 0 | 16);
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

void TiledLayer::setStaticTileSet(const std::vector<uint32_t>& tileImage, int imgW, int imgH, int tileW, int tileH) {
    if (tileW < 1 || tileH < 1 || imgW <= 0 || imgH <= 0 || (imgW % tileW) != 0 || (imgH % tileH) != 0) return;
    m_tileWidth = tileW;
    m_tileHeight = tileH;
    m_tileImage = tileImage;
    m_imgWidth = imgW;
    m_imgHeight = imgH;
    m_width = m_cols * tileW;
    m_height = m_rows * tileH;

    int noOfFrames = (imgW / tileW) * (imgH / tileH);
    if (noOfFrames < (m_numStaticTiles - 1)) {
        std::fill(m_cells.begin(), m_cells.end(), 0);
        m_animatedTiles.clear();
    }
    m_numStaticTiles = noOfFrames + 1;
}

uint32_t TiledLayer::getPixel(int tileIdx, int localX, int localY) const {
    if (tileIdx <= 0 || m_tileImage.empty() || m_tileWidth <= 0 || m_tileHeight <= 0) return 0;
    if (localX < 0 || localX >= m_tileWidth || localY < 0 || localY >= m_tileHeight) return 0;
    int tilesPerRow = m_imgWidth / m_tileWidth;
    if (tilesPerRow <= 0) return 0;

    int idx = tileIdx - 1;
    int tx = (idx % tilesPerRow) * m_tileWidth + localX;
    int ty = (idx / tilesPerRow) * m_tileHeight + localY;
    if (tx < 0 || tx >= m_imgWidth || ty < 0 || ty >= m_imgHeight) return 0;
    return m_tileImage[(size_t)ty * m_imgWidth + tx];
}

void TiledLayer::paint(LcduiDisplay* display) {
    if (!m_visible || m_tileImage.empty() || m_tileWidth <= 0 || m_tileHeight <= 0 || !display) return;
    int tilesPerRow = m_imgWidth / m_tileWidth;
    if (tilesPerRow <= 0) return;

    // Upstream TiledLayer.java:208-240: calculate startColumn, endColumn, startRow, endRow from clip
    int startColumn = 0;
    int endColumn = m_cols;
    int startRow = 0;
    int endRow = m_rows;

    int clipX = display->getClipX();
    int clipY = display->getClipY();
    int clipW = display->getClipWidth();
    int clipH = display->getClipHeight();

    int number = (clipX - m_x) / m_tileWidth;
    if (number > 0) startColumn = number;

    int endX = m_x + (m_cols * m_tileWidth);
    int endClipX = clipX + clipW;
    number = (endX - endClipX) / m_tileWidth;
    if (number > 0) endColumn -= number;

    number = (clipY - m_y) / m_tileHeight;
    if (number > 0) startRow = number;

    int endY = m_y + (m_rows * m_tileHeight);
    int endClipY = clipY + clipH;
    number = (endY - endClipY) / m_tileHeight;
    if (number > 0) endRow -= number;

    if (startColumn < 0) startColumn = 0;
    if (startRow < 0) startRow = 0;
    if (endColumn > m_cols) endColumn = m_cols;
    if (endRow > m_rows) endRow = m_rows;
    if (startColumn >= endColumn || startRow >= endRow) return;

    for (int r = startRow; r < endRow; ++r) {
        int ty = m_y + r * m_tileHeight;
        for (int c = startColumn; c < endColumn; ++c) {
            int tile = m_cells[r * m_cols + c];
            if (tile < 0) {
                tile = getAnimatedTile(tile);
            }
            if (tile <= 0) continue;

            int tileIdx = tile - 1;
            int tx = (tileIdx % tilesPerRow) * m_tileWidth;
            int ty_src = (tileIdx / tilesPerRow) * m_tileHeight;
            int destX = m_x + c * m_tileWidth;

            display->drawRegion(m_tileImage.data(), m_imgWidth, m_imgHeight, tx, ty_src, m_tileWidth, m_tileHeight, 0, destX, ty, 0 | 16);
        }
    }
}

LayerManager::LayerManager() {}

void LayerManager::append(std::shared_ptr<Layer> layer) {
    if (!layer) return;
    remove(layer);
    m_layers.push_back(layer);
}

void LayerManager::insert(std::shared_ptr<Layer> layer, int index) {
    if (!layer) return;
    remove(layer);
    if (index < 0) index = 0;
    if (index > (int)m_layers.size()) index = (int)m_layers.size();
    m_layers.insert(m_layers.begin() + index, layer);
}

void LayerManager::remove(std::shared_ptr<Layer> layer) {
    if (!layer) return;
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
    if (!display) return;
    int oldX = display->getClipX();
    int oldY = display->getClipY();
    int oldW = display->getClipWidth();
    int oldH = display->getClipHeight();

    display->translate(x - m_viewX, y - m_viewY);
    display->clipRect(m_viewX, m_viewY, m_viewW, m_viewH);

    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        if ((*it)->isVisible()) {
            (*it)->paint(display);
        }
    }

    display->translate(-x + m_viewX, -y + m_viewY);
    display->setClip(oldX, oldY, oldW, oldH);
}