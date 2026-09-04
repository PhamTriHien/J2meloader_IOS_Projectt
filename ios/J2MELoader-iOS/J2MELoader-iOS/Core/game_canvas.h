#ifndef GAME_CANVAS_H
#define GAME_CANVAS_H

#include "lcdui_display.h"
#include <vector>
#include <memory>
#include <cstdint>

enum SpriteTransform {
    TRANS_NONE = 0,
    TRANS_MIRROR_ROT180 = 1,
    TRANS_MIRROR = 2,
    TRANS_ROT180 = 3,
    TRANS_MIRROR_ROT270 = 4,
    TRANS_ROT90 = 5,
    TRANS_ROT270 = 6,
    TRANS_MIRROR_ROT90 = 7
};

class Layer {
public:
    virtual ~Layer() = default;
    virtual void paint(LcduiDisplay* display) = 0;
    
    void setPosition(int x, int y) { m_x = x; m_y = y; }
    void move(int dx, int dy) { m_x += dx; m_y += dy; }
    void setVisible(bool visible) { m_visible = visible; }
    
    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isVisible() const { return m_visible; }

protected:
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_visible = true;
};

class Sprite : public Layer {
public:
    Sprite(const std::vector<uint32_t>& imagePixels, int imgWidth, int imgHeight, int frameWidth, int frameHeight);
    
    void setFrame(int sequenceIndex);
    void setFrameSequence(const std::vector<int>& sequence);
    void nextFrame();
    void prevFrame();
    int getFrame() const { return m_currentFrameIndex; }
    int getRawFrameCount() const { return m_numFrames; }

    void setTransform(SpriteTransform transform);
    void defineReferencePixel(int x, int y);
    void setRefPixelPosition(int x, int y);
    int getRefPixelX() const;
    int getRefPixelY() const;

    void defineCollisionRectangle(int x, int y, int width, int height);
    bool collidesWith(const Sprite& other, bool pixelLevel) const;
    bool collidesWith(int x, int y, int width, int height) const;

    void paint(LcduiDisplay* display) override;

private:
    std::vector<uint32_t> m_imagePixels;
    int m_imageWidth;
    int m_imageHeight;
    int m_frameWidth;
    int m_frameHeight;
    int m_numFrames;

    std::vector<int> m_sequence;
    int m_currentFrameIndex = 0;
    SpriteTransform m_transform = TRANS_NONE;

    int m_refX = 0;
    int m_refY = 0;

    int m_collX = 0;
    int m_collY = 0;
    int m_collW = 0;
    int m_collH = 0;
    bool m_customCollision = false;

    uint32_t getPixel(int frame, int localX, int localY, SpriteTransform transform) const;
};

class TiledLayer : public Layer {
public:
    TiledLayer(int columns, int rows, const std::vector<uint32_t>& tileImage, int imgW, int imgH, int tileW, int tileH);

    void setCell(int col, int row, int tileIndex);
    int getCell(int col, int row) const;
    void fillCells(int col, int row, int numCols, int numRows, int tileIndex);

    int createAnimatedTile(int staticTileIndex);
    void setAnimatedTile(int animatedTileIndex, int staticTileIndex);
    int getAnimatedTile(int animatedTileIndex) const;

    void paint(LcduiDisplay* display) override;

private:
    int m_cols;
    int m_rows;
    int m_tileWidth;
    int m_tileHeight;
    std::vector<int> m_cells;
    std::vector<uint32_t> m_tileImage;
    int m_imgWidth;
    int m_imgHeight;
    int m_numStaticTiles;
    std::vector<int> m_animatedTiles;
};

class LayerManager {
public:
    LayerManager();
    void append(std::shared_ptr<Layer> layer);
    void insert(std::shared_ptr<Layer> layer, int index);
    void remove(std::shared_ptr<Layer> layer);
    void setViewWindow(int x, int y, int width, int height);
    void paint(LcduiDisplay* display, int x, int y);

private:
    std::vector<std::shared_ptr<Layer>> m_layers;
    int m_viewX = 0;
    int m_viewY = 0;
    int m_viewW = 240;
    int m_viewH = 320;
};

#endif // GAME_CANVAS_H