#ifndef UNIVERSAL_LOADER_PIM_MANAGER_H
#define UNIVERSAL_LOADER_PIM_MANAGER_H

#include "pim_types.h"
#include "pim_list.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>

namespace universal_loader {
namespace pim {

class J2ME_API PIMManager {
public:
    static PIMManager& getInstance();

    void init(const std::string& sandboxDir);
    void reset();

    std::vector<std::string> listPIMLists(int pimListType);
    std::shared_ptr<PIMList> openPIMList(int pimListType, int mode, const std::string& name = "");

    std::vector<std::string> getSupportedSerialFormats(int pimListType);
    std::vector<std::shared_ptr<PIMItem>> fromSerialFormat(int pimListType, const std::string& data, const std::string& encoding);
    std::string toSerialFormat(const std::shared_ptr<PIMItem>& item, const std::string& dataFormat, const std::string& encoding);

    void saveAll();

    const std::string& getSandboxDir() const { return m_sandboxDir; }

private:
    PIMManager();
    ~PIMManager() = default;

    std::string m_sandboxDir;
    std::mutex m_mutex;
    std::map<std::string, std::shared_ptr<ContactList>> m_contactLists;
    std::map<std::string, std::shared_ptr<EventList>> m_eventLists;
    std::map<std::string, std::shared_ptr<ToDoList>> m_todoLists;

    void ensureDefaults();
    std::string getListFilePath(int pimListType, const std::string& name) const;
};

} // namespace pim
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_PIM_MANAGER_H
