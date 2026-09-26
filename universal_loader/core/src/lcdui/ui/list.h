#pragma once

#include "displayable.h"
#include "item.h"
#include <vector>
#include <memory>
#include <string>

namespace universal_loader {
namespace lcdui {

class J2ME_API List : public Displayable {
public:
    static constexpr int EXCLUSIVE = 1;
    static constexpr int MULTIPLE  = 2;
    static constexpr int IMPLICIT  = 3;

    static constexpr int TEXT_WRAP_DEFAULT = 0;
    static constexpr int TEXT_WRAP_ON      = 1;
    static constexpr int TEXT_WRAP_OFF     = 2;

    static const Command SELECT_COMMAND;

    List(std::string title, int listType);
    List(std::string title, int listType, const std::vector<std::string>& stringElements,
         const std::vector<std::shared_ptr<j2me::LcduiImage>>& imageElements);
    ~List() override = default;

    int append(std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart);
    void insert(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart);
    void deleteElement(int elementNum);
    void deleteAll();
    void set(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart);

    int size() const { return static_cast<int>(m_elements.size()); }
    int getListType() const { return m_listType; }

    const std::string& getString(int elementNum) const;
    std::shared_ptr<j2me::LcduiImage> getImage(int elementNum) const;

    int getSelectedIndex() const;
    void setSelectedIndex(int elementNum, bool selected);
    bool isSelected(int elementNum) const;

    int getSelectedFlags(std::vector<bool>& result) const;
    void setSelectedFlags(const std::vector<bool>& flags);

    void setSelectCommand(const Command& cmd) { m_selectCommand = cmd; }
    const Command& getSelectCommand() const { return m_selectCommand; }

    int getFitPolicy() const { return m_fitPolicy; }
    void setFitPolicy(int fitPolicy) { m_fitPolicy = fitPolicy; }

    void paint(j2me::LcduiGraphics* g) override;
    void keyPressed(int keyCode) override;
    void pointerPressed(int x, int y) override;

private:
    int m_listType;
    int m_fitPolicy;
    Command m_selectCommand;
    std::vector<ChoiceElement> m_elements;
    int m_focusedIndex;
    int m_scrollY;
};

} // namespace lcdui
} // namespace universal_loader
