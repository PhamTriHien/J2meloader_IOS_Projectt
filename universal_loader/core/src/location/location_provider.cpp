#include "location_provider.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <chrono>

namespace universal_loader {
namespace location {

std::mutex LocationProvider::s_globalMutex;
std::unique_ptr<LocationProvider> LocationProvider::s_instance;
std::unique_ptr<Location> LocationProvider::s_lastKnownLocation;
int32_t LocationProvider::s_providerState = LOCATION_PROVIDER_AVAILABLE;

LocationProvider::LocationProvider() = default;

LocationProvider::~LocationProvider() {
    reset();
}

LocationProvider* LocationProvider::getInstance(const Criteria* criteria) {
    (void)criteria;
    std::lock_guard<std::mutex> lock(s_globalMutex);
    if (!s_instance) {
        s_instance = std::unique_ptr<LocationProvider>(new LocationProvider());
        // Default initial location if none set yet: 0.0, 0.0 (Null Island) with 10m accuracy
        if (!s_lastKnownLocation) {
            QualifiedCoordinates coords(0.0, 0.0, 0.0f, 10.0f, 10.0f);
            std::string nmea = generateNmeaSequence(0.0, 0.0, 0.0f, 0.0f, 0.0f, 0);
            s_lastKnownLocation = std::make_unique<Location>(coords, 0.0f, 0.0f, 0, MTE_SATELLITE | MTY_TERMINALBASED, nmea);
        }
    }
    return s_instance.get();
}

Location LocationProvider::getLastKnownLocation() {
    std::lock_guard<std::mutex> lock(s_globalMutex);
    if (s_lastKnownLocation) {
        return *s_lastKnownLocation;
    }
    QualifiedCoordinates coords(0.0, 0.0, 0.0f, 10.0f, 10.0f);
    std::string nmea = generateNmeaSequence(0.0, 0.0, 0.0f, 0.0f, 0.0f, 0);
    return Location(coords, 0.0f, 0.0f, 0, MTE_SATELLITE | MTY_TERMINALBASED, nmea);
}

void LocationProvider::updateHostLocation(double lat, double lon, float alt,
                                         float speed, float course,
                                         float horizontalAccuracy, float verticalAccuracy) {
    LocationListener* listenerCopy = nullptr;
    LocationProvider* prov = nullptr;
    Location locCopy(QualifiedCoordinates(0, 0, 0));

    {
        std::lock_guard<std::mutex> lock(s_globalMutex);
        QualifiedCoordinates coords(lat, lon, alt, horizontalAccuracy, verticalAccuracy);
        auto now = std::chrono::system_clock::now().time_since_epoch();
        int64_t ts = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        std::string nmea = generateNmeaSequence(lat, lon, alt, speed, course, ts);

        s_lastKnownLocation = std::make_unique<Location>(coords, speed, course, ts, MTE_SATELLITE | MTY_TERMINALBASED, nmea);
        locCopy = *s_lastKnownLocation;

        if (s_instance) {
            prov = s_instance.get();
            std::lock_guard<std::mutex> innerLock(prov->m_mutex);
            listenerCopy = prov->m_listener;
        }
    }

    if (listenerCopy && prov) {
        listenerCopy->locationUpdated(prov, locCopy);
    }
}

void LocationProvider::setHostProviderState(int32_t state) {
    LocationListener* listenerCopy = nullptr;
    LocationProvider* prov = nullptr;

    {
        std::lock_guard<std::mutex> lock(s_globalMutex);
        s_providerState = state;
        if (s_instance) {
            prov = s_instance.get();
            std::lock_guard<std::mutex> innerLock(prov->m_mutex);
            listenerCopy = prov->m_listener;
        }
    }

    if (listenerCopy && prov) {
        listenerCopy->providerStateChanged(prov, state);
    }
}

int32_t LocationProvider::getState() const {
    std::lock_guard<std::mutex> lock(s_globalMutex);
    return s_providerState;
}

void LocationProvider::reset() {
    m_running = false;
    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listener = nullptr;
    m_interval = 0;
}

Location LocationProvider::getLocation(int32_t timeoutSec) {
    (void)timeoutSec;
    std::lock_guard<std::mutex> lock(s_globalMutex);
    if (s_providerState == LOCATION_PROVIDER_OUT_OF_SERVICE) {
        throw std::runtime_error("All positioning methods disabled");
    }
    if (s_lastKnownLocation) {
        return *s_lastKnownLocation;
    }
    throw std::runtime_error("Location request timed out");
}

void LocationProvider::setLocationListener(LocationListener* listener, int32_t interval, int32_t timeout, int32_t maxAge) {
    reset();

    if (!listener) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_listener = listener;
    m_interval = (interval < 0) ? 2 : (interval == 0 ? 1 : interval);
    m_timeout = timeout;
    m_maxAge = maxAge;
    m_running = true;

    m_workerThread = std::thread([this]() {
        while (m_running) {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait_for(lk, std::chrono::seconds(m_interval), [this]() { return !m_running; });
            if (!m_running) break;

            if (m_listener) {
                Location loc = getLastKnownLocation();
                m_listener->locationUpdated(this, loc);
            }
        }
    });
}

static std::string formatNmeaChecksum(const std::string& sentenceWithoutDollar) {
    uint8_t chk = 0;
    for (char c : sentenceWithoutDollar) {
        chk ^= static_cast<uint8_t>(c);
    }
    std::ostringstream ss;
    ss << '$' << sentenceWithoutDollar << '*' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(chk);
    return ss.str();
}

std::string LocationProvider::generateNmeaSequence(double lat, double lon, float alt, float speed, float course, int64_t timestamp) {
    (void)timestamp;
    // Format Latitude in NMEA: DDMM.mmmmm,N/S
    int latDeg = static_cast<int>(std::abs(lat));
    double latMin = (std::abs(lat) - latDeg) * 60.0;
    char latHemi = (lat >= 0.0) ? 'N' : 'S';

    std::ostringstream latSs;
    latSs << std::setw(2) << std::setfill('0') << latDeg
          << std::fixed << std::setprecision(4) << (latMin < 10.0 ? "0" : "") << latMin;

    // Format Longitude in NMEA: DDDMM.mmmmm,E/W
    int lonDeg = static_cast<int>(std::abs(lon));
    double lonMin = (std::abs(lon) - lonDeg) * 60.0;
    char lonHemi = (lon >= 0.0) ? 'E' : 'W';

    std::ostringstream lonSs;
    lonSs << std::setw(3) << std::setfill('0') << lonDeg
          << std::fixed << std::setprecision(4) << (lonMin < 10.0 ? "0" : "") << lonMin;

    // 1. $GPGGA Sentence
    std::ostringstream ggaBody;
    ggaBody << "GPGGA,120000.00," << latSs.str() << ',' << latHemi << ','
            << lonSs.str() << ',' << lonHemi << ",1,08,1.0,"
            << std::fixed << std::setprecision(1) << (std::isnan(alt) ? 0.0f : alt) << ",M,0.0,M,,";
    std::string gga = formatNmeaChecksum(ggaBody.str());

    // 2. $GPRMC Sentence
    double speedKnots = (std::isnan(speed) ? 0.0 : speed) * 1.94384;
    double courseDeg = std::isnan(course) ? 0.0 : course;

    std::ostringstream rmcBody;
    rmcBody << "GPRMC,120000.00,A," << latSs.str() << ',' << latHemi << ','
            << lonSs.str() << ',' << lonHemi << ','
            << std::fixed << std::setprecision(1) << speedKnots << ','
            << std::fixed << std::setprecision(1) << courseDeg << ",240926,,,A";
    std::string rmc = formatNmeaChecksum(rmcBody.str());

    // 3. $GPGSA Sentence
    std::string gsa = formatNmeaChecksum("GPGSA,A,3,01,02,03,04,05,06,07,08,,,,,1.5,1.0,1.2");

    return gga + "\n" + rmc + "\n" + gsa;
}

} // namespace location
} // namespace universal_loader
