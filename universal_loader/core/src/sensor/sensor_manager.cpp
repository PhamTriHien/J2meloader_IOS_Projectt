#include "sensor_manager.h"
#include <sstream>
#include <algorithm>

namespace universal_loader {
namespace sensor {

SensorManager& SensorManager::instance() {
    static SensorManager s_instance;
    return s_instance;
}

SensorManager::SensorManager() {
    initDefaultSensors();
}

void SensorManager::initDefaultSensors() {
    m_sensors.clear();

    // 1. Accelerometer (3 channels: axis_x, axis_y, axis_z)
    {
        std::vector<MeasurementRange> accRanges = {
            MeasurementRange(-9800.0, 9800.0, 10.0)
        };
        std::vector<ChannelInfo> accChannels = {
            ChannelInfo("axis_x", CHANNEL_TYPE_INT, Unit("m/s^2"), 1000, 0.05f, accRanges),
            ChannelInfo("axis_y", CHANNEL_TYPE_INT, Unit("m/s^2"), 1000, 0.05f, accRanges),
            ChannelInfo("axis_z", CHANNEL_TYPE_INT, Unit("m/s^2"), 1000, 0.05f, accRanges)
        };
        SensorInfo accInfo(accChannels, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_USER,
                           "3-Axis Accelerometer sensor", "default", "acceleration", 1024);
        accInfo.setProperty("vendor", "Standard");
        accInfo.setProperty("version", "1.0");
        m_sensors.push_back(std::move(accInfo));
    }

    // 2. Ambient Light (1 channel: luminance)
    {
        std::vector<MeasurementRange> lightRanges = {
            MeasurementRange(0.0, 100000.0, 1.0)
        };
        std::vector<ChannelInfo> lightChannels = {
            ChannelInfo("luminance", CHANNEL_TYPE_DOUBLE, Unit("lux"), 1, 1.0f, lightRanges)
        };
        SensorInfo lightInfo(lightChannels, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_AMBIENT,
                             "Ambient Light sensor", "default", "ambient_light", 512);
        lightInfo.setProperty("vendor", "Standard");
        lightInfo.setProperty("version", "1.0");
        m_sensors.push_back(std::move(lightInfo));
    }

    // 3. Magnetic Field / Magnetometer (3 channels: axis_x, axis_y, axis_z)
    {
        std::vector<MeasurementRange> magRanges = {
            MeasurementRange(-2000.0, 2000.0, 1.0)
        };
        std::vector<ChannelInfo> magChannels = {
            ChannelInfo("axis_x", CHANNEL_TYPE_DOUBLE, Unit("uT"), 100, 0.5f, magRanges),
            ChannelInfo("axis_y", CHANNEL_TYPE_DOUBLE, Unit("uT"), 100, 0.5f, magRanges),
            ChannelInfo("axis_z", CHANNEL_TYPE_DOUBLE, Unit("uT"), 100, 0.5f, magRanges)
        };
        SensorInfo magInfo(magChannels, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_AMBIENT,
                            "3-Axis Magnetometer sensor", "default", "magnetic_field", 512);
        magInfo.setProperty("vendor", "Standard");
        magInfo.setProperty("version", "1.0");
        m_sensors.push_back(std::move(magInfo));
    }

    // 4. Orientation (3 channels: azimuth, pitch, roll)
    {
        std::vector<MeasurementRange> oriRanges = {
            MeasurementRange(0.0, 360.0, 1.0)
        };
        std::vector<ChannelInfo> oriChannels = {
            ChannelInfo("azimuth", CHANNEL_TYPE_DOUBLE, Unit("deg"), 10, 1.0f, oriRanges),
            ChannelInfo("pitch", CHANNEL_TYPE_DOUBLE, Unit("deg"), 10, 1.0f, oriRanges),
            ChannelInfo("roll", CHANNEL_TYPE_DOUBLE, Unit("deg"), 10, 1.0f, oriRanges)
        };
        SensorInfo oriInfo(oriChannels, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_DEVICE,
                            "Device Orientation sensor", "default", "orientation", 512);
        oriInfo.setProperty("vendor", "Standard");
        oriInfo.setProperty("version", "1.0");
        m_sensors.push_back(std::move(oriInfo));
    }

    // 5. Temperature (1 channel: temperature)
    {
        std::vector<MeasurementRange> tempRanges = {
            MeasurementRange(-40.0, 125.0, 0.1)
        };
        std::vector<ChannelInfo> tempChannels = {
            ChannelInfo("temperature", CHANNEL_TYPE_DOUBLE, Unit("Cel"), 10, 0.1f, tempRanges)
        };
        SensorInfo tempInfo(tempChannels, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_AMBIENT,
                             "Ambient Temperature sensor", "default", "temperature", 256);
        tempInfo.setProperty("vendor", "Standard");
        tempInfo.setProperty("version", "1.0");
        m_sensors.push_back(std::move(tempInfo));
    }
}

std::vector<SensorInfo> SensorManager::getAllSensors() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sensors;
}

std::vector<SensorInfo> SensorManager::findSensors(const std::string& quantity, const std::string& contextType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (quantity.empty() && contextType.empty()) {
        return m_sensors;
    }

    std::vector<SensorInfo> result;
    for (const auto& s : m_sensors) {
        bool matchQty = quantity.empty() || (s.getQuantity() == quantity);
        bool matchCtx = contextType.empty() || (s.getContextType() == contextType);
        if (matchQty && matchCtx) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<SensorInfo> SensorManager::findSensorsByUrl(const std::string& url) {
    if (url.rfind("sensor:", 0) != 0) {
        throw std::invalid_argument("URL must begin with sensor: scheme");
    }

    std::string path = url.substr(7); // strip "sensor:"
    std::string quantity;
    std::string contextType;
    std::string model;

    // Parse parameters delimited by ';' or '?'
    std::stringstream ss(path);
    std::string token;
    bool first = true;

    while (std::getline(ss, token, ';')) {
        if (first) {
            // Check for '?' in quantity
            size_t qmark = token.find('?');
            if (qmark != std::string::npos) {
                quantity = token.substr(0, qmark);
            } else {
                quantity = token;
            }
            first = false;
        } else {
            size_t eq = token.find('=');
            if (eq != std::string::npos) {
                std::string key = token.substr(0, eq);
                std::string val = token.substr(eq + 1);
                if (key == "contextType") {
                    contextType = val;
                } else if (key == "model") {
                    model = val;
                }
            }
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SensorInfo> result;
    for (const auto& s : m_sensors) {
        bool matchQty = quantity.empty() || (s.getQuantity() == quantity);
        bool matchCtx = contextType.empty() || (s.getContextType() == contextType);
        bool matchMod = model.empty() || (s.getModel() == model);
        if (matchQty && matchCtx && matchMod) {
            result.push_back(s);
        }
    }
    return result;
}

std::shared_ptr<SensorConnection> SensorManager::openSensor(const std::string& url) {
    auto matches = findSensorsByUrl(url);
    if (matches.empty()) {
        throw std::invalid_argument("No sensor found matching URL: " + url);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // Cleanup closed connections
    m_activeConnections.erase(
        std::remove_if(m_activeConnections.begin(), m_activeConnections.end(),
                       [](const std::shared_ptr<SensorConnection>& conn) {
                           return !conn || conn->getState() == SENSOR_STATE_CLOSED;
                       }),
        m_activeConnections.end());

    auto conn = std::make_shared<SensorConnection>(matches.front());
    m_activeConnections.push_back(conn);
    return conn;
}

void SensorManager::updateAccelerometer(double x, double y, double z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> vals = {x, y, z};
    for (const auto& conn : m_activeConnections) {
        if (conn && conn->getState() != SENSOR_STATE_CLOSED &&
            conn->getSensorInfo().getQuantity() == "acceleration") {
            conn->pushSamples(vals);
        }
    }
}

void SensorManager::updateAmbientLight(double lux) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> vals = {lux};
    for (const auto& conn : m_activeConnections) {
        if (conn && conn->getState() != SENSOR_STATE_CLOSED &&
            conn->getSensorInfo().getQuantity() == "ambient_light") {
            conn->pushSamples(vals);
        }
    }
}

void SensorManager::updateMagneticField(double x, double y, double z) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> vals = {x, y, z};
    for (const auto& conn : m_activeConnections) {
        if (conn && conn->getState() != SENSOR_STATE_CLOSED &&
            conn->getSensorInfo().getQuantity() == "magnetic_field") {
            conn->pushSamples(vals);
        }
    }
}

void SensorManager::updateOrientation(double azimuth, double pitch, double roll) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> vals = {azimuth, pitch, roll};
    for (const auto& conn : m_activeConnections) {
        if (conn && conn->getState() != SENSOR_STATE_CLOSED &&
            conn->getSensorInfo().getQuantity() == "orientation") {
            conn->pushSamples(vals);
        }
    }
}

void SensorManager::updateTemperature(double celsius) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> vals = {celsius};
    for (const auto& conn : m_activeConnections) {
        if (conn && conn->getState() != SENSOR_STATE_CLOSED &&
            conn->getSensorInfo().getQuantity() == "temperature") {
            conn->pushSamples(vals);
        }
    }
}

void SensorManager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& conn : m_activeConnections) {
        if (conn) {
            conn->close();
        }
    }
    m_activeConnections.clear();
    initDefaultSensors();
}

} // namespace sensor
} // namespace universal_loader
