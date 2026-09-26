#ifndef J2ME_GAME_SPRITE_LAYER_H
#define J2ME_GAME_SPRITE_LAYER_H

#include "layer.h"
#include "../lcdui_graphics.h"
#include <memory>

namespace universal_loader::lcdui::game {

class J2ME_API SpriteLayer : public Layer {
public:
    SpriteLayer(std::shared_ptr<j2me::LcduiImage> image, int frameWidth, int frameHeight)
        : Layer(frameWidth, frameHeight),
          m_sprite(std::make_shared<j2me::SpriteEngine>(image, frameWidth, frameHeight)) {}

    ~SpriteLayer() override = default;

    void setPosition(int x, int y) override {
        Layer::setPosition(x, y);
        m_sprite->setPosition(x, y);
    }

    void move(int dx, int dy) override {
        Layer::move(dx, dy);
        m_sprite->setPosition(m_x, m_y);
    }

    void setFrame(int sequenceIndex) { m_sprite->setFrame(sequenceIndex); }
    int getFrame() const { return m_sprite->getFrame(); }
    void setFrameSequence(const std::vector<int>& sequence) { m_sprite->setFrameSequence(sequence); }
    void nextFrame() { m_sprite->nextFrame(); }
    void prevFrame() { m_sprite->prevFrame(); }

    void setTransform(int transform) { m_sprite->setTransform(transform); }
    int getTransform() const { return m_sprite->getTransform(); }

    void paint(j2me::LcduiGraphics* g) override {
        if (m_visible && g) {
            m_sprite->paint(g);
        }
    }

    std::shared_ptr<j2me::SpriteEngine> getEngine() const { return m_sprite; }

    bool collidesWith(const SpriteLayer& other, bool pixelLevel) const {
        return m_sprite->collidesWith(*other.m_sprite, pixelLevel);
    }

private:
    std::shared_ptr<j2me::SpriteEngine> m_sprite;
};

} // namespace universal_loader::lcdui::game

#endif // J2ME_GAME_SPRITE_LAYER_H
