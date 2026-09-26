#ifndef UNIVERSAL_LOADER_LOCATION_PROVIDER_H
#define UNIVERSAL_LOADER_LOCATION_PROVIDER_H

#include "location_types.h"
#include "location.h"
#include "criteria.h"
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace universal_loader {
namespace location {

class LocationProvider;

class J2ME_API LocationListener {
public:
    virtual ~LocationListener() = default;
    virtual void locationUpdated(LocationProvider* provider, const Location& location) = 0;
    virtual void providerStateChanged(LocationProvider* provider, int32_t newState) = 0;
};

class J2ME_API LocationProvider {
public:
    static LocationProvider* getInstance(const Criteria* criteria = nullptr);
    static Location getLastKnownLocation();

    // Injection of Real GPS/Host Platform Location (from iOS CoreLocation / Android / Desktop)
    static void updateHostLocation(double lat, double lon, float alt,
                                   float speed = 0.0f, float course = 0.0f,
                                   float horizontalAccuracy = 5.0f, float verticalAccuracy = 5.0f);
    static void setHostProviderState(int32_t state);

    // Standard JSR-179 Provider Methods
    virtual ~LocationProvider();

    int32_t getState() const;
    void reset();

    Location getLocation(int32_t timeoutSec);
    void setLocationListener(LocationListener* listener, int32_t interval, int32_t timeout, int32_t maxAge);

    // NMEA-0183 Generator Helper
    static std::string generateNmeaSequence(double lat, double lon, float alt, float speed, float course, int64_t timestamp);

private:
    LocationProvider();

    mutable std::mutex m_mutex;
    LocationListener* m_listener{nullptr};
    int32_t m_interval{0};
    int32_t m_timeout{0};
    int32_t m_maxAge{0};

    std::thread m_workerThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_cv;

    static std::mutex s_globalMutex;
    static std::unique_ptr<LocationProvider> s_instance;
    static std::unique_ptr<Location> s_lastKnownLocation;
    static int32_t s_providerState;
};

} // namespace location
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_LOCATION_PROVIDER_H
