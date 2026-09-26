#include "midlet.h"

namespace universal_loader {
namespace midlet {

// =========================================================================
// MIDlet Implementation
// =========================================================================
void MIDlet::notifyPaused() {
    MidletLifecycleManager::instance().onMidletNotifyPaused();
}

void MIDlet::notifyDestroyed() {
    MidletLifecycleManager::instance().onMidletNotifyDestroyed();
}

void MIDlet::resumeRequest() {
    MidletLifecycleManager::instance().onMidletResumeRequest();
}

bool MIDlet::platformRequest(const std::string& url) {
    return MidletLifecycleManager::instance().onMidletPlatformRequest(url);
}

std::string MIDlet::getAppProperty(const std::string& key) const {
    return MidletLifecycleManager::instance().getProperty(key);
}

int MIDlet::checkPermission(const std::string& /*permission*/) const {
    // Matches upstream MIDlet.java: return 1 (Permission Granted)
    return 1;
}

// =========================================================================
// MidletLifecycleManager Implementation
// =========================================================================
MidletLifecycleManager& MidletLifecycleManager::instance() {
    static MidletLifecycleManager s_instance;
    return s_instance;
}

void MidletLifecycleManager::setPlatformRequestHandler(PlatformRequestHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_platformHandler = handler;
}

void MidletLifecycleManager::setStateChangeListener(StateChangeListener listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stateListener = listener;
}

bool MidletLifecycleManager::setMidlet(std::shared_ptr<MIDlet> midlet, std::shared_ptr<AppDescriptor> descriptor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeMidlet = midlet;
    m_descriptor = descriptor;
    m_state = MIDletState::UNINITIALIZED;

    if (m_activeMidlet) {
        m_activeMidlet->m_manager = this;
        m_activeMidlet->m_state = MIDletState::UNINITIALIZED;
    }
    return true;
}

std::shared_ptr<MIDlet> MidletLifecycleManager::getActiveMidlet() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeMidlet;
}

std::shared_ptr<AppDescriptor> MidletLifecycleManager::getDescriptor() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_descriptor;
}

MIDletState MidletLifecycleManager::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void MidletLifecycleManager::startApp() {
    std::shared_ptr<MIDlet> midlet;
    StateChangeListener listener;
    MIDletState oldState;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_activeMidlet) return;
        midlet = m_activeMidlet;
        listener = m_stateListener;
        oldState = m_state;
    }

    try {
        midlet->startApp();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = MIDletState::ACTIVE;
            midlet->m_state = MIDletState::ACTIVE;
        }
        if (listener) {
            listener(oldState, MIDletState::ACTIVE);
        }
    } catch (...) {
        throw;
    }
}

void MidletLifecycleManager::pauseApp() {
    std::shared_ptr<MIDlet> midlet;
    StateChangeListener listener;
    MIDletState oldState;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_activeMidlet || m_state != MIDletState::ACTIVE) return;
        midlet = m_activeMidlet;
        listener = m_stateListener;
        oldState = m_state;
    }

    midlet->pauseApp();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = MIDletState::PAUSED;
        midlet->m_state = MIDletState::PAUSED;
    }
    if (listener) {
        listener(oldState, MIDletState::PAUSED);
    }
}

void MidletLifecycleManager::resumeApp() {
    MIDletState state = getState();
    if (state == MIDletState::PAUSED) {
        startApp();
    }
}

void MidletLifecycleManager::destroyApp(bool unconditional) {
    std::shared_ptr<MIDlet> midlet;
    StateChangeListener listener;
    MIDletState oldState;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_activeMidlet || m_state == MIDletState::DESTROYED) return;
        midlet = m_activeMidlet;
        listener = m_stateListener;
        oldState = m_state;
    }

    try {
        midlet->destroyApp(unconditional);
    } catch (...) {
        if (!unconditional) {
            throw;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = MIDletState::DESTROYED;
        midlet->m_state = MIDletState::DESTROYED;
    }
    if (listener) {
        listener(oldState, MIDletState::DESTROYED);
    }
}

void MidletLifecycleManager::onMidletNotifyPaused() {
    StateChangeListener listener;
    MIDletState oldState;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oldState = m_state;
        m_state = MIDletState::PAUSED;
        if (m_activeMidlet) {
            m_activeMidlet->m_state = MIDletState::PAUSED;
        }
        listener = m_stateListener;
    }
    if (listener) {
        listener(oldState, MIDletState::PAUSED);
    }
}

void MidletLifecycleManager::onMidletNotifyDestroyed() {
    StateChangeListener listener;
    MIDletState oldState;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oldState = m_state;
        m_state = MIDletState::DESTROYED;
        if (m_activeMidlet) {
            m_activeMidlet->m_state = MIDletState::DESTROYED;
        }
        listener = m_stateListener;
    }
    if (listener) {
        listener(oldState, MIDletState::DESTROYED);
    }
}

void MidletLifecycleManager::onMidletResumeRequest() {
    resumeApp();
}

bool MidletLifecycleManager::onMidletPlatformRequest(const std::string& url) {
    PlatformRequestHandler handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handler = m_platformHandler;
    }
    if (handler) {
        return handler(url);
    }
    // Default fallback: accept standard URL schemes
    return (url.rfind("http://", 0) == 0 ||
            url.rfind("https://", 0) == 0 ||
            url.rfind("tel:", 0) == 0 ||
            url.rfind("sms:", 0) == 0);
}

std::string MidletLifecycleManager::getProperty(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_descriptor) {
        return m_descriptor->get(key);
    }
    return "";
}

} // namespace midlet
} // namespace universal_loader
