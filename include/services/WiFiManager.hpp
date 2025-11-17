#pragma once
#include "Config.hpp"
#include "app/Types.hpp"
#include "util/Result.hpp"
#include "services/ConfigManager.hpp"
#include <WiFi.h>
#include <vector>
#include <algorithm>
#include <optional>
#include <array>

namespace services {

class WiFiManager {
public:
    WiFiManager() = default;

    // Scan and connect to best available network
    // Credentials are passed at connection time
    util::Result<void, app::WiFiError> connect(const std::vector<WiFiCredential>& credentials);

    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    std::string getConnectedSSID() const;
    int8_t getRSSI() const;

    String getLocalIP() const {
        return WiFi.localIP().toString();
    }

    std::vector<app::WiFiScanResult> getLastScan() const {
        return lastScanResults_;
    }

private:
    struct NetworkMatch {
        std::string ssid;
        std::string password;
        int8_t rssi;
        uint8_t channel;
        std::array<uint8_t, 6> bssid;
    };

    std::vector<app::WiFiScanResult> scanNetworks();
    std::optional<NetworkMatch> selectBestNetwork(
        const std::vector<app::WiFiScanResult>& scanResults,
        const std::vector<WiFiCredential>& credentials);

    std::string connectedSsid_;
    std::vector<app::WiFiScanResult> lastScanResults_;
};

} // namespace services
