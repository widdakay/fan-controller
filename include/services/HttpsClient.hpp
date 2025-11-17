#pragma once
#include "app/Types.hpp"
#include "util/Result.hpp"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "util/Logger.hpp"

namespace services {

class HttpsClient {
public:
    HttpsClient() {
        // Don't verify SSL certificates (for simplicity)
        // In production, should use proper certificate validation
        secureClient_.setInsecure();
    }

    util::Result<String, app::HttpError> post(const char* url, const String& jsonData) {
        if (WiFi.status() != WL_CONNECTED) {
            return util::Result<String, app::HttpError>::Err(app::HttpError::ConnectionFailed);
        }

        HTTPClient http;
        http.begin(secureClient_, url);
        http.addHeader("Content-Type", "application/json");

        int httpCode = http.POST(jsonData);

        if (httpCode > 0) {
            String response = http.getString();
            http.end();

            if (httpCode == HTTP_CODE_OK) {
                return util::Result<String, app::HttpError>::Ok(response);
            } else {
                Logger::error("HTTP POST failed with code %d", httpCode);
                Logger::error("URL: %s", url);
                Logger::error("Response: %s", response.c_str());
                Logger::error("JSON Data: %s", jsonData.c_str());
                return util::Result<String, app::HttpError>::Err(app::HttpError::RequestFailed);
            }
        } else {
            Logger::error("HTTP POST error: %s", http.errorToString(httpCode).c_str());
            http.end();
            return util::Result<String, app::HttpError>::Err(app::HttpError::RequestFailed);
        }
    }

    util::Result<String, app::HttpError> get(const char* url) {
        if (WiFi.status() != WL_CONNECTED) {
            return util::Result<String, app::HttpError>::Err(app::HttpError::ConnectionFailed);
        }

        HTTPClient http;
        http.begin(secureClient_, url);

        int httpCode = http.GET();

        if (httpCode > 0) {
            String response = http.getString();
            http.end();

            if (httpCode == HTTP_CODE_OK) {
                return util::Result<String, app::HttpError>::Ok(response);
            } else {
                return util::Result<String, app::HttpError>::Err(app::HttpError::RequestFailed);
            }
        } else {
            http.end();
            return util::Result<String, app::HttpError>::Err(app::HttpError::RequestFailed);
        }
    }

private:
    WiFiClientSecure secureClient_;
};

} // namespace services
