#ifndef UNIVERSAL_LOADER_PIM_LIST_H
#define UNIVERSAL_LOADER_PIM_LIST_H

#include "pim_types.h"
#include "pim_item.h"
#include "contact.h"
#include "event.h"
#include "todo.h"
#include <string>
#include <vector>
#include <memory>

namespace universal_loader {
namespace pim {

class J2ME_API PIMList {
public:
    PIMList(const std::string& name, int type, int mode);
    virtual ~PIMList() = default;

    const std::string& getName() const { return m_name; }
    int getType() const { return m_type; }
    int getMode() const { return m_mode; }
    bool isOpen() const { return m_open; }
    void close() { m_open = false; }

    bool isModified() const;

    const std::vector<std::string>& getCategories() const { return m_categories; }
    void addCategory(const std::string& category);
    void deleteCategory(const std::string& category, bool deleteItems);
    bool isCategory(const std::string& category) const;
    int maxCategories() const { return 10; }

    virtual int maxValues(int field) const;
    virtual int getFieldDataType(int field) const;
    virtual std::string getFieldLabel(int field) const;
    virtual std::vector<int> getSupportedFields() const;
    virtual std::vector<int> getSupportedAttributes(int field) const;
    virtual std::vector<int> getSupportedArrayElements(int stringArrayField) const;

    std::vector<std::shared_ptr<PIMItem>> items() const;
    std::vector<std::shared_ptr<PIMItem>> items(const std::shared_ptr<PIMItem>& matchingItem) const;
    std::vector<std::shared_ptr<PIMItem>> items(const std::string& matchingValue) const;
    std::vector<std::shared_ptr<PIMItem>> itemsByCategory(const std::string& category) const;

    void addItem(const std::shared_ptr<PIMItem>& item);
    void removeItem(const std::shared_ptr<PIMItem>& item);

    void saveToFile(const std::string& filePath) const;
    void loadFromFile(const std::string& filePath);

protected:
    std::string m_name;
    int m_type{PIM_CONTACT_LIST};
    int m_mode{PIM_READ_WRITE};
    bool m_open{true};
    std::vector<std::string> m_categories;
    std::vector<std::shared_ptr<PIMItem>> m_items;
};

class J2ME_API ContactList : public PIMList {
public:
    ContactList(const std::string& name, int mode);
    ~ContactList() override = default;

    int getFieldDataType(int field) const override;
    std::string getFieldLabel(int field) const override;
    std::vector<int> getSupportedFields() const override;
    std::vector<int> getSupportedAttributes(int field) const override;
    std::vector<int> getSupportedArrayElements(int stringArrayField) const override;

    std::shared_ptr<Contact> createContact();
    std::shared_ptr<Contact> importContact(const std::shared_ptr<Contact>& contact);
    void removeContact(const std::shared_ptr<Contact>& contact);
};

class J2ME_API EventList : public PIMList {
public:
    EventList(const std::string& name, int mode);
    ~EventList() override = default;

    int getFieldDataType(int field) const override;
    std::string getFieldLabel(int field) const override;
    std::vector<int> getSupportedFields() const override;
    std::vector<int> getSupportedAttributes(int field) const override;

    std::shared_ptr<Event> createEvent();
    std::shared_ptr<Event> importEvent(const std::shared_ptr<Event>& event);
    void removeEvent(const std::shared_ptr<Event>& event);

    std::vector<std::shared_ptr<Event>> items(int searchType, int64_t startDate, int64_t endDate, bool initialEventOnly) const;
};

class J2ME_API ToDoList : public PIMList {
public:
    ToDoList(const std::string& name, int mode);
    ~ToDoList() override = default;

    int getFieldDataType(int field) const override;
    std::string getFieldLabel(int field) const override;
    std::vector<int> getSupportedFields() const override;
    std::vector<int> getSupportedAttributes(int field) const override;

    std::shared_ptr<ToDo> createToDo();
    std::shared_ptr<ToDo> importToDo(const std::shared_ptr<ToDo>& todo);
    void removeToDo(const std::shared_ptr<ToDo>& todo);

    std::vector<std::shared_ptr<ToDo>> items(int field, int64_t startDate, int64_t endDate) const;
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PIM_LIST_H
