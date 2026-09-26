#ifndef J2ME_GAME_TILED_LAYER_H
#define J2ME_GAME_TILED_LAYER_H

#include "layer.h"
#include <vector>
#include <memory>

namespace universal_loader::lcdui::game {

class J2ME_API TiledLayer : public Layer {
public:
    TiledLayer(int columns, int rows, std::shared_ptr<j2me::LcduiImage> image, int tileWidth, int tileHeight);
    ~TiledLayer() override = default;

    int createAnimatedTile(int staticTileIndex);
    void setAnimatedTile(int animatedTileIndex, int staticTileIndex);
    int getAnimatedTile(int animatedTileIndex) const;

    void setCell(int col, int row, int tileIndex);
    int getCell(int col, int row) const;
    void fillCells(int col, int row, int numCols, int numRows, int tileIndex);

    int getCellWidth() const { return m_cellWidth; }
    int getCellHeight() const { return m_cellHeight; }
    int getColumns() const { return m_columns; }
    int getRows() const { return m_rows; }

    void setStaticTileSet(std::shared_ptr<j2me::LcduiImage> image, int tileWidth, int tileHeight);

    void paint(j2me::LcduiGraphics* g) override;

private:
    int m_cellWidth{0};
    int m_cellHeight{0};
    int m_columns{0};
    int m_rows{0};

    std::vector<std::vector<int>> m_cellMatrix;
    std::shared_ptr<j2me::LcduiImage> m_sourceImage;

    int m_numberOfTiles{0};
    std::vector<int> m_tileSetX;
    std::vector<int> m_tileSetY;

    std::vector<int> m_animToStatic;
    int m_numOfAnimTiles{1};

    void createStaticSet(std::shared_ptr<j2me::LcduiImage> image, int noOfFrames, int tileWidth, int tileHeight, bool maintainIndices);
};

} // namespace universal_loader::lcdui::game

#endif // J2ME_GAME_TILED_LAYER_H
