#ifndef UNIVERSAL_LOADER_PIM_ITEM_H
#define UNIVERSAL_LOADER_PIM_ITEM_H

#include "pim_types.h"
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace universal_loader {
namespace pim {

class PIMList;

struct PIMValue {
    int attributes{ATTR_NONE};
    std::string stringVal;
    int intVal{0};
    int64_t dateVal{0};
    bool boolVal{false};
    std::vector<uint8_t> binaryVal;
    std::vector<std::string> stringArrayVal;
};

struct PIMFieldData {
    int fieldId{0};
    int dataType{PIM_TYPE_STRING};
    std::vector<PIMValue> values;
};

class J2ME_API PIMItem {
public:
    explicit PIMItem(PIMList* list = nullptr);
    virtual ~PIMItem() = default;

    PIMList* getPIMList() const { return m_list; }
    void setPIMList(PIMList* list) { m_list = list; }

    virtual int getType() const = 0;

    void commit();
    bool isModified() const { return m_modified; }
    void setModified(bool mod) { m_modified = mod; }

    std::vector<int> getFields() const;
    virtual int getDataType(int field) const;

    int countValues(int field) const;
    int getAttributes(int field, int index) const;
    void removeValue(int field, int index);

    // Getters
    std::string getString(int field, int index) const;
    int getInt(int field, int index) const;
    int64_t getDate(int field, int index) const;
    bool getBoolean(int field, int index) const;
    std::vector<uint8_t> getBinary(int field, int index) const;
    std::vector<std::string> getStringArray(int field, int index) const;

    // Setters
    void setString(int field, int index, int attributes, const std::string& value);
    void setInt(int field, int index, int attributes, int value);
    void setDate(int field, int index, int attributes, int64_t value);
    void setBoolean(int field, int index, int attributes, bool value);
    void setBinary(int field, int index, int attributes, const std::vector<uint8_t>& value, int offset, int length);
    void setStringArray(int field, int index, int attributes, const std::vector<std::string>& value);

    // Adders
    void addString(int field, int attributes, const std::string& value);
    void addInt(int field, int attributes, int value);
    void addDate(int field, int attributes, int64_t value);
    void addBoolean(int field, int attributes, bool value);
    void addBinary(int field, int attributes, const std::vector<uint8_t>& value, int offset, int length);
    void addStringArray(int field, int attributes, const std::vector<std::string>& value);

    // Categories
    const std::vector<std::string>& getCategories() const { return m_categories; }
    void addToCategory(const std::string& category);
    void removeFromCategory(const std::string& category);
    int maxCategories() const { return 10; }

    // UID
    const std::string& getUid() const { return m_uid; }
    void setUid(const std::string& uid) { m_uid = uid; }

    // Serialization
    std::string toJson() const;
    void fromJson(const std::string& json);

    const std::map<int, PIMFieldData>& getFieldMap() const { return m_fields; }

protected:
    PIMList* m_list{nullptr};
    bool m_modified{false};
    std::string m_uid;
    std::vector<std::string> m_categories;
    std::map<int, PIMFieldData> m_fields;

    PIMValue* getOrCreateValue(int field, int index);
    const PIMValue* getValue(int field, int index) const;
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PIM_ITEM_H
