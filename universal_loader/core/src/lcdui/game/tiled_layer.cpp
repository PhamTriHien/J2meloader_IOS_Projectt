#include "tiled_layer.h"
#include <algorithm>
#include <stdexcept>

namespace universal_loader::lcdui::game {

TiledLayer::TiledLayer(int columns, int rows, std::shared_ptr<j2me::LcduiImage> image, int tileWidth, int tileHeight)
    : Layer(columns < 1 || tileWidth < 1 ? 0 : columns * tileWidth,
            rows < 1 || tileHeight < 1 ? 0 : rows * tileHeight) {
    if (!image) {
        throw std::invalid_argument("Image cannot be null in TiledLayer");
    }
    if (columns <= 0 || rows <= 0 || tileWidth <= 0 || tileHeight <= 0) {
        throw std::invalid_argument("Invalid dimensions for TiledLayer");
    }
    if ((image->getWidth() % tileWidth != 0) || (image->getHeight() % tileHeight != 0)) {
        throw std::invalid_argument("Image dimensions must be divisible by tile dimensions");
    }

    m_columns = columns;
    m_rows = rows;
    m_cellMatrix.assign(rows, std::vector<int>(columns, 0));

    int noOfFrames = (image->getWidth() / tileWidth) * (image->getHeight() / tileHeight);
    createStaticSet(image, noOfFrames + 1, tileWidth, tileHeight, true);
}

int TiledLayer::createAnimatedTile(int staticTileIndex) {
    if (staticTileIndex < 0 || staticTileIndex >= m_numberOfTiles) {
        throw std::out_of_range("Static tile index out of bounds");
    }

    if (m_animToStatic.empty()) {
        m_animToStatic.resize(4, 0);
        m_numOfAnimTiles = 1;
    } else if (m_numOfAnimTiles == static_cast<int>(m_animToStatic.size())) {
        m_animToStatic.resize(m_animToStatic.size() * 2, 0);
    }

    m_animToStatic[m_numOfAnimTiles] = staticTileIndex;
    m_numOfAnimTiles++;
    return -(m_numOfAnimTiles - 1);
}

void TiledLayer::setAnimatedTile(int animatedTileIndex, int staticTileIndex) {
    if (staticTileIndex < 0 || staticTileIndex >= m_numberOfTiles) {
        throw std::out_of_range("Static tile index out of bounds");
    }

    int idx = -animatedTileIndex;
    if (m_animToStatic.empty() || idx <= 0 || idx >= m_numOfAnimTiles) {
        throw std::out_of_range("Animated tile index out of bounds");
    }

    m_animToStatic[idx] = staticTileIndex;
}

int TiledLayer::getAnimatedTile(int animatedTileIndex) const {
    int idx = -animatedTileIndex;
    if (m_animToStatic.empty() || idx <= 0 || idx >= m_numOfAnimTiles) {
        throw std::out_of_range("Animated tile index out of bounds");
    }

    return m_animToStatic[idx];
}

void TiledLayer::setCell(int col, int row, int tileIndex) {
    if (col < 0 || col >= m_columns || row < 0 || row >= m_rows) {
        throw std::out_of_range("Cell coordinate out of bounds");
    }

    if (tileIndex > 0) {
        if (tileIndex >= m_numberOfTiles) {
            throw std::out_of_range("Tile index out of bounds");
        }
    } else if (tileIndex < 0) {
        int idx = -tileIndex;
        if (m_animToStatic.empty() || idx >= m_numOfAnimTiles) {
            throw std::out_of_range("Animated tile index out of bounds");
        }
    }

    m_cellMatrix[row][col] = tileIndex;
}

int TiledLayer::getCell(int col, int row) const {
    if (col < 0 || col >= m_columns || row < 0 || row >= m_rows) {
        throw std::out_of_range("Cell coordinate out of bounds");
    }
    return m_cellMatrix[row][col];
}

void TiledLayer::fillCells(int col, int row, int numCols, int numRows, int tileIndex) {
    if (numCols < 0 || numRows < 0) {
        throw std::invalid_argument("numCols and numRows cannot be negative");
    }

    if (col < 0 || col >= m_columns || row < 0 || row >= m_rows ||
        col + numCols > m_columns || row + numRows > m_rows) {
        throw std::out_of_range("Range out of bounds");
    }

    if (tileIndex > 0) {
        if (tileIndex >= m_numberOfTiles) {
            throw std::out_of_range("Tile index out of bounds");
        }
    } else if (tileIndex < 0) {
        int idx = -tileIndex;
        if (m_animToStatic.empty() || idx >= m_numOfAnimTiles) {
            throw std::out_of_range("Animated tile index out of bounds");
        }
    }

    for (int r = row; r < row + numRows; ++r) {
        for (int c = col; c < col + numCols; ++c) {
            m_cellMatrix[r][c] = tileIndex;
        }
    }
}

void TiledLayer::setStaticTileSet(std::shared_ptr<j2me::LcduiImage> image, int tileWidth, int tileHeight) {
    if (!image || tileWidth < 1 || tileHeight < 1 ||
        ((image->getWidth() % tileWidth) != 0) ||
        ((image->getHeight() % tileHeight) != 0)) {
        throw std::invalid_argument("Invalid tile dimensions or image");
    }

    setWidthImpl(m_columns * tileWidth);
    setHeightImpl(m_rows * tileHeight);

    int noOfFrames = (image->getWidth() / tileWidth) * (image->getHeight() / tileHeight);
    if (noOfFrames >= (m_numberOfTiles - 1)) {
        createStaticSet(image, noOfFrames + 1, tileWidth, tileHeight, true);
    } else {
        createStaticSet(image, noOfFrames + 1, tileWidth, tileHeight, false);
    }
}

void TiledLayer::paint(j2me::LcduiGraphics* g) {
    if (!g || !m_visible || !m_sourceImage || m_cellWidth <= 0 || m_cellHeight <= 0) {
        return;
    }

    int startColumn = 0;
    int endColumn = m_columns;
    int startRow = 0;
    int endRow = m_rows;

    int clipX = g->getClipX();
    int clipY = g->getClipY();
    int clipW = g->getClipWidth();
    int clipH = g->getClipHeight();

    // calculate the number of columns left of the clip
    int number = (clipX - m_x) / m_cellWidth;
    if (number > 0) {
        startColumn = std::min(number, m_columns);
    }

    // calculate the number of columns right of the clip
    int endX = m_x + (m_columns * m_cellWidth);
    int endClipX = clipX + clipW;
    number = (endX - endClipX) / m_cellWidth;
    if (number > 0) {
        endColumn = std::max(0, endColumn - number);
    }

    // calculate the number of rows above the clip
    number = (clipY - m_y) / m_cellHeight;
    if (number > 0) {
        startRow = std::min(number, m_rows);
    }

    // calculate the number of rows below the clip
    int endY = m_y + (m_rows * m_cellHeight);
    int endClipY = clipY + clipH;
    number = (endY - endClipY) / m_cellHeight;
    if (number > 0) {
        endRow = std::max(0, endRow - number);
    }

    // paint all visible cells
    int ty = m_y + (startRow * m_cellHeight);
    for (int row = startRow; row < endRow; ++row, ty += m_cellHeight) {
        int tx = m_x + (startColumn * m_cellWidth);
        for (int col = startColumn; col < endColumn; ++col, tx += m_cellWidth) {
            int tileIndex = m_cellMatrix[row][col];
            if (tileIndex == 0) {
                continue; // transparent tile
            } else if (tileIndex < 0) {
                tileIndex = getAnimatedTile(tileIndex);
            }

            if (tileIndex > 0 && tileIndex < m_numberOfTiles) {
                g->drawRegion(m_sourceImage.get(),
                              m_tileSetX[tileIndex],
                              m_tileSetY[tileIndex],
                              m_cellWidth,
                              m_cellHeight,
                              j2me::TRANS_NONE,
                              tx,
                              ty,
                              j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);
            }
        }
    }
}

void TiledLayer::createStaticSet(std::shared_ptr<j2me::LcduiImage> image, int noOfFrames, int tileWidth, int tileHeight, bool maintainIndices) {
    m_cellWidth = tileWidth;
    m_cellHeight = tileHeight;
    m_sourceImage = image;

    int imageW = image->getWidth();
    int imageH = image->getHeight();

    m_numberOfTiles = noOfFrames;
    m_tileSetX.assign(m_numberOfTiles, 0);
    m_tileSetY.assign(m_numberOfTiles, 0);

    if (!maintainIndices) {
        for (int r = 0; r < m_rows; ++r) {
            for (int c = 0; c < m_columns; ++c) {
                m_cellMatrix[r][c] = 0;
            }
        }
        m_animToStatic.clear();
        m_numOfAnimTiles = 1;
    }

    int currentTile = 1;
    for (int locY = 0; locY < imageH; locY += tileHeight) {
        for (int locX = 0; locX < imageW; locX += tileWidth) {
            if (currentTile < m_numberOfTiles) {
                m_tileSetX[currentTile] = locX;
                m_tileSetY[currentTile] = locY;
                currentTile++;
            }
        }
    }
}

} // namespace universal_loader::lcdui::game
