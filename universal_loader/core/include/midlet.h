#pragma once

#include "j2me_core.h"
#include "app_descriptor.h"
#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace universal_loader {
namespace midlet {

/**
 * @brief MIDlet lifecycle states matching upstream J2ME MidletThread constants.
 */
enum class MIDletState {
    UNINITIALIZED = 0,
    ACTIVE        = 1, // Started / Running
    PAUSED        = 2, // Backgrounded
    DESTROYED     = 3  // Terminated
};

/**
 * @brief Exception thrown when MIDlet cannot fulfill a state change request.
 * Matches javax.microedition.midlet.MIDletStateChangeException.
 */
class MIDletStateChangeException : public std::runtime_error {
public:
    explicit MIDletStateChangeException(const std::string& message)
        : std::runtime_error(message) {}
};

class MidletLifecycleManager;

/**
 * @brief Abstract base class for all J2ME applications.
 * Directly matches javax.microedition.midlet.MIDlet from upstream J2ME-Loader.
 */
class J2ME_API MIDlet {
    friend class MidletLifecycleManager;

private:
    MIDletState m_state{MIDletState::UNINITIALIZED};
    MidletLifecycleManager* m_manager{nullptr};

public:
    MIDlet() = default;
    virtual ~MIDlet() = default;

    // Core Lifecycle Callbacks implemented by the MIDlet
    virtual void startApp() = 0;
    virtual void pauseApp() = 0;
    virtual void destroyApp(bool unconditional) = 0;

    // Platform and State Signals invoked by the MIDlet
    void notifyPaused();
    void notifyDestroyed();
    void resumeRequest();
    bool platformRequest(const std::string& url);

    std::string getAppProperty(const std::string& key) const;
    int checkPermission(const std::string& permission) const;

    MIDletState getState() const { return m_state; }
};

/**
 * @brief Manages the execution lifecycle, background/resume state transitions,
 * and descriptor properties of the active MIDlet.
 */
class J2ME_API MidletLifecycleManager {
public:
    using PlatformRequestHandler = std::function<bool(const std::string& url)>;
    using StateChangeListener = std::function<void(MIDletState oldState, MIDletState newState)>;

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<MIDlet> m_activeMidlet;
    std::shared_ptr<AppDescriptor> m_descriptor;
    MIDletState m_state{MIDletState::UNINITIALIZED};
    PlatformRequestHandler m_platformHandler{nullptr};
    StateChangeListener m_stateListener{nullptr};

    MidletLifecycleManager() = default;

public:
    static MidletLifecycleManager& instance();

    void setPlatformRequestHandler(PlatformRequestHandler handler);
    void setStateChangeListener(StateChangeListener listener);

    bool setMidlet(std::shared_ptr<MIDlet> midlet, std::shared_ptr<AppDescriptor> descriptor = nullptr);
    std::shared_ptr<MIDlet> getActiveMidlet() const;
    std::shared_ptr<AppDescriptor> getDescriptor() const;
    MIDletState getState() const;

    // Lifecycle Commands
    void startApp();
    void pauseApp();
    void resumeApp();
    void destroyApp(bool unconditional = true);

    // Signals from MIDlet
    void onMidletNotifyPaused();
    void onMidletNotifyDestroyed();
    void onMidletResumeRequest();
    bool onMidletPlatformRequest(const std::string& url);
    std::string getProperty(const std::string& key) const;
};

} // namespace midlet
} // namespace universal_loader
