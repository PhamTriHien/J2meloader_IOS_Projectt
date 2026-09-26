#pragma once

#include "j2me_core.h"
#include "bluetooth_types.h"
#include "bluetooth_uuid.h"
#include "bluetooth_service_record.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace j2me {
namespace bluetooth {

class J2ME_API RemoteDevice {
public:
    RemoteDevice();
    explicit RemoteDevice(const std::string& address, const std::string& friendlyName = "");

    std::string getBluetoothAddress() const;
    std::string getFormattedAddress() const;
    std::string getFriendlyName(bool alwaysAsk = false) const;
    void setFriendlyName(const std::string& name) { friendlyName_ = name; }

    bool isAuthenticated() const { return authenticated_; }
    bool isAuthorized() const { return authorized_; }
    bool isEncrypted() const { return encrypted_; }
    bool isTrustedDevice() const { return trusted_; }

    void setAuthenticated(bool v) { authenticated_ = v; }
    void setAuthorized(bool v) { authorized_ = v; }
    void setEncrypted(bool v) { encrypted_ = v; }
    void setTrusted(bool v) { trusted_ = v; }

    bool operator==(const RemoteDevice& other) const;
    bool operator!=(const RemoteDevice& other) const { return !(*this == other); }

private:
    std::string address_; // 12-char uppercase hex
    std::string friendlyName_;
    bool authenticated_ = false;
    bool authorized_ = false;
    bool encrypted_ = false;
    bool trusted_ = false;
};

class J2ME_API DiscoveryAgent {
public:
    DiscoveryAgent();

    // Device inquiry
    bool startInquiry(int accessCode);
    bool cancelInquiry();
    bool isInquiring() const { return inquiring_; }

    // Remote device registration / discovery
    void addDiscoveredDevice(const RemoteDevice& device);
    void addPreknownDevice(const RemoteDevice& device);
    std::vector<RemoteDevice> retrieveDevices(int option) const;

    // Service search & registration
    void registerRemoteService(const std::string& deviceAddress, const BluetoothServiceRecord& record);
    std::vector<BluetoothServiceRecord> searchServices(
        const std::string& deviceAddress,
        const std::vector<BluetoothUUID>& uuidSet) const;

    // Fast service selection: returns connection URL or empty string if not found
    std::string selectService(const BluetoothUUID& uuid, int security, bool master) const;

private:
    mutable std::mutex mutex_;
    bool inquiring_ = false;
    std::vector<RemoteDevice> discoveredDevices_;
    std::vector<RemoteDevice> preknownDevices_;
    // deviceAddress -> list of ServiceRecords
    std::map<std::string, std::vector<BluetoothServiceRecord>> remoteServices_;
};

class J2ME_API LocalDevice {
public:
    static LocalDevice& getInstance();

    LocalDevice();

    std::string getBluetoothAddress() const { return address_; }
    void setBluetoothAddress(const std::string& addr);

    std::string getFriendlyName() const { return friendlyName_; }
    void setFriendlyName(const std::string& name) { friendlyName_ = name; }

    int getDiscoverable() const { return discoverableMode_; }
    bool setDiscoverable(int mode);

    bool isPowerOn() const { return powerOn_; }
    void setPowerOn(bool on) { powerOn_ = on; }

    std::string getProperty(const std::string& property) const;

    DiscoveryAgent& getDiscoveryAgent() { return agent_; }
    const DiscoveryAgent& getDiscoveryAgent() const { return agent_; }

    // Service records hosted on this local device
    bool registerService(const BluetoothServiceRecord& record);
    bool unregisterService(const BluetoothUUID& uuid);
    bool findService(const BluetoothUUID& uuid, BluetoothServiceRecord& outRecord) const;
    std::vector<BluetoothServiceRecord> getAllLocalServices() const;

private:
    mutable std::mutex mutex_;
    std::string address_ = "001122334455";
    std::string friendlyName_ = "J2ME-Loader Mobile";
    int discoverableMode_ = GIAC;
    bool powerOn_ = true;
    std::map<std::string, std::string> properties_;
    DiscoveryAgent agent_;
    std::map<BluetoothUUID, BluetoothServiceRecord> localServices_;

    void initProperties();
};

} // namespace bluetooth
} // namespace j2me
