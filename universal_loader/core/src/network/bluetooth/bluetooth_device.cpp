#include "bluetooth_device.h"
#include <algorithm>
#include <cctype>

namespace j2me {
namespace bluetooth {

// Helper to clean MAC address into 12 uppercase hex characters
static std::string normalizeAddress(const std::string& addr) {
    std::string clean;
    for (char c : addr) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    if (clean.length() < 12) {
        clean.append(12 - clean.length(), '0');
    } else if (clean.length() > 12) {
        clean = clean.substr(0, 12);
    }
    return clean;
}

// RemoteDevice
RemoteDevice::RemoteDevice()
    : address_("000000000000"), friendlyName_("Unknown Device") {}

RemoteDevice::RemoteDevice(const std::string& address, const std::string& friendlyName)
    : address_(normalizeAddress(address)), friendlyName_(friendlyName) {
    if (friendlyName_.empty()) {
        friendlyName_ = "Device " + address_;
    }
}

std::string RemoteDevice::getBluetoothAddress() const {
    return address_;
}

std::string RemoteDevice::getFormattedAddress() const {
    std::string formatted;
    for (size_t i = 0; i < address_.length(); ++i) {
        if (i > 0 && i % 2 == 0) formatted.push_back(':');
        formatted.push_back(address_[i]);
    }
    return formatted;
}

std::string RemoteDevice::getFriendlyName(bool /*alwaysAsk*/) const {
    return friendlyName_;
}

bool RemoteDevice::operator==(const RemoteDevice& other) const {
    return address_ == other.address_;
}

// DiscoveryAgent
DiscoveryAgent::DiscoveryAgent() = default;

bool DiscoveryAgent::startInquiry(int accessCode) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((accessCode != LIAC) && (accessCode != GIAC) && ((accessCode < 0x9E8B00) || (accessCode > 0x9E8B3F))) {
        return false;
    }
    inquiring_ = true;
    return true;
}

bool DiscoveryAgent::cancelInquiry() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!inquiring_) return false;
    inquiring_ = false;
    return true;
}

void DiscoveryAgent::addDiscoveredDevice(const RemoteDevice& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& d : discoveredDevices_) {
        if (d == device) {
            d = device;
            return;
        }
    }
    discoveredDevices_.push_back(device);
}

void DiscoveryAgent::addPreknownDevice(const RemoteDevice& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& d : preknownDevices_) {
        if (d == device) {
            d = device;
            return;
        }
    }
    preknownDevices_.push_back(device);
}

std::vector<RemoteDevice> DiscoveryAgent::retrieveDevices(int option) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (option == DEVICE_CACHED) {
        return discoveredDevices_;
    } else if (option == DEVICE_PREKNOWN) {
        return preknownDevices_;
    }
    return {};
}

void DiscoveryAgent::registerRemoteService(const std::string& deviceAddress, const BluetoothServiceRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string addr = normalizeAddress(deviceAddress);
    remoteServices_[addr].push_back(record);
}

std::vector<BluetoothServiceRecord> DiscoveryAgent::searchServices(
    const std::string& deviceAddress,
    const std::vector<BluetoothUUID>& uuidSet) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string addr = normalizeAddress(deviceAddress);
    std::vector<BluetoothServiceRecord> results;

    auto it = remoteServices_.find(addr);
    if (it == remoteServices_.end()) {
        return results;
    }

    for (const auto& rec : it->second) {
        if (uuidSet.empty()) {
            results.push_back(rec);
            continue;
        }
        for (const auto& targetUuid : uuidSet) {
            if (rec.getUUID() == targetUuid) {
                results.push_back(rec);
                break;
            }
        }
    }
    return results;
}

std::string DiscoveryAgent::selectService(const BluetoothUUID& uuid, int security, bool master) const {
    std::lock_guard<std::mutex> lock(mutex_);
    // Search in all remote devices
    for (const auto& kv : remoteServices_) {
        for (const auto& rec : kv.second) {
            if (rec.getUUID() == uuid) {
                return rec.getConnectionURL(security, master);
            }
        }
    }
    return "";
}

// LocalDevice
LocalDevice& LocalDevice::getInstance() {
    static LocalDevice instance;
    return instance;
}

LocalDevice::LocalDevice() {
    initProperties();
}

void LocalDevice::initProperties() {
    properties_["bluetooth.api.version"] = "1.1";
    properties_["bluetooth.master.switch"] = "true";
    properties_["bluetooth.sd.attr.retrievable.max"] = "256";
    properties_["bluetooth.connected.devices.max"] = "7";
    properties_["bluetooth.l2cap.receiveMTU.max"] = "672";
    properties_["bluetooth.sd.trans.max"] = "1";
    properties_["bluetooth.connected.inquiry.scan"] = "true";
    properties_["bluetooth.connected.page.scan"] = "true";
    properties_["bluetooth.connected.inquiry"] = "true";
    properties_["bluetooth.connected.page"] = "true";
}

void LocalDevice::setBluetoothAddress(const std::string& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    address_ = normalizeAddress(addr);
}

bool LocalDevice::setDiscoverable(int mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((mode != NOT_DISCOVERABLE) && (mode != GIAC) && (mode != LIAC) &&
        (mode < 0x9E8B00 || mode > 0x9E8B3F)) {
        return false;
    }
    discoverableMode_ = mode;
    return true;
}

std::string LocalDevice::getProperty(const std::string& property) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = properties_.find(property);
    if (it != properties_.end()) {
        return it->second;
    }
    return "";
}

bool LocalDevice::registerService(const BluetoothServiceRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    localServices_.insert_or_assign(record.getUUID(), record);
    return true;
}

bool LocalDevice::unregisterService(const BluetoothUUID& uuid) {
    std::lock_guard<std::mutex> lock(mutex_);
    return localServices_.erase(uuid) > 0;
}

bool LocalDevice::findService(const BluetoothUUID& uuid, BluetoothServiceRecord& outRecord) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = localServices_.find(uuid);
    if (it != localServices_.end()) {
        outRecord = it->second;
        return true;
    }
    return false;
}

std::vector<BluetoothServiceRecord> LocalDevice::getAllLocalServices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BluetoothServiceRecord> res;
    res.reserve(localServices_.size());
    for (const auto& kv : localServices_) {
        res.push_back(kv.second);
    }
    return res;
}

} // namespace bluetooth
} // namespace j2me
