#include "services/WiFiManager.hpp"
#include "services/ConfigManager.hpp"
#include "util/Logger.hpp"

namespace services {

util::Result<void, app::WiFiError> WiFiManager::connect(const std::vector<WiFiCredential>& credentials) {
    if (credentials.empty()) {
        return util::Result<void, app::WiFiError>::Err(app::WiFiError::NoCredentials);
    }

    // Scan for networks
    auto scanResults = scanNetworks();
    if (scanResults.empty()) {
        return util::Result<void, app::WiFiError>::Err(app::WiFiError::ScanFailed);
    }

    // Find best matching network
    auto bestNetwork = selectBestNetwork(scanResults, credentials);
    if (!bestNetwork) {
        LOG_ERROR("No known networks found");
        return util::Result<void, app::WiFiError>::Err(app::WiFiError::ConnectionFailed);
    }

    // Connect to selected network
    LOG_INFO("Connecting to %s (RSSI: %d, Ch: %u)...",
              bestNetwork->ssid.c_str(), bestNetwork->rssi, bestNetwork->channel);

    // Prefer a specific BSSID to ensure strongest AP for SSID
    WiFi.begin(bestNetwork->ssid.c_str(),
               bestNetwork->password.c_str(),
               bestNetwork->channel,
               bestNetwork->bssid.data());

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > config::WIFI_CONNECT_TIMEOUT_MS) {
            WiFi.disconnect();
            return util::Result<void, app::WiFiError>::Err(app::WiFiError::Timeout);
        }
        delay(100);
    }

    connectedSsid_ = bestNetwork->ssid;
    LOG_INFO("Connected! IP: %s", WiFi.localIP().toString().c_str());

    return util::Result<void, app::WiFiError>::Ok();
}

std::string WiFiManager::getConnectedSSID() const {
    if (isConnected()) {
        return WiFi.SSID().c_str();
    }
    return connectedSsid_;
}

int8_t WiFiManager::getRSSI() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return -100;
}

std::vector<app::WiFiScanResult> WiFiManager::scanNetworks() {
    std::vector<app::WiFiScanResult> results;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    LOG_INFO("WiFi scan found %d networks", n);

    for (int i = 0; i < n; i++) {
        app::WiFiScanResult result;
        result.ssid = WiFi.SSID(i).c_str();
        result.rssi = WiFi.RSSI(i);
        result.channel = WiFi.channel(i);
        result.encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        const uint8_t* bssidPtr = WiFi.BSSID(i);
        if (bssidPtr != nullptr) {
            for (int j = 0; j < 6; ++j) {
                result.bssid[j] = bssidPtr[j];
            }
        } else {
            // Zero if unavailable
            result.bssid.fill(0);
        }

        results.push_back(result);
        LOG_DEBUG("  %s (RSSI: %d, Ch: %d)",
                      result.ssid.c_str(), result.rssi, result.channel);
    }

    lastScanResults_ = results;
    WiFi.scanDelete();

    return results;
}

std::optional<WiFiManager::NetworkMatch> WiFiManager::selectBestNetwork(
    const std::vector<app::WiFiScanResult>& scanResults,
    const std::vector<WiFiCredential>& credentials) {

    std::optional<NetworkMatch> best;
    int8_t bestRssi = -100;

    // Find configured network with best signal
    for (const auto& cred : credentials) {
        for (const auto& scan : scanResults) {
            if (scan.ssid == cred.ssid.c_str() && scan.rssi > bestRssi) {
                NetworkMatch match;
                match.ssid = cred.ssid.c_str();
                match.password = cred.password.c_str();
                match.rssi = scan.rssi;
                match.channel = scan.channel;
                match.bssid = scan.bssid;
                best = match;
                bestRssi = scan.rssi;
            }
        }
    }

    return best;
}

} // namespace services
