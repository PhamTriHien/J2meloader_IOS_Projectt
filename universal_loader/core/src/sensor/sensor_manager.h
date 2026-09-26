#ifndef UNIVERSAL_LOADER_SENSOR_MANAGER_H
#define UNIVERSAL_LOADER_SENSOR_MANAGER_H

#include "sensor_types.h"
#include "sensor_info.h"
#include "sensor_connection.h"
#include <vector>
#include <memory>
#include <mutex>
#include <string>

namespace universal_loader {
namespace sensor {

// Sensor Manager (JSR-256 SensorManager.java)
class J2ME_API SensorManager {
public:
    static SensorManager& instance();

    // Query available sensors
    std::vector<SensorInfo> findSensors(const std::string& quantity, const std::string& contextType = "");
    std::vector<SensorInfo> findSensorsByUrl(const std::string& url);
    std::vector<SensorInfo> getAllSensors() const;

    // Open sensor connection
    std::shared_ptr<SensorConnection> openSensor(const std::string& url);

    // Host platform sample injection
    void updateAccelerometer(double x, double y, double z);
    void updateAmbientLight(double lux);
    void updateMagneticField(double x, double y, double z);
    void updateOrientation(double azimuth, double pitch, double roll);
    void updateTemperature(double celsius);

    // Reset all connections and state
    void reset();

private:
    SensorManager();
    ~SensorManager() = default;

    void initDefaultSensors();

    mutable std::mutex m_mutex;
    std::vector<SensorInfo> m_sensors;
    std::vector<std::shared_ptr<SensorConnection>> m_activeConnections;
};

} // namespace sensor
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_SENSOR_MANAGER_H
