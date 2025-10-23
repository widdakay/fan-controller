#include "https.h"

void sendHttpsData(String data) {
  if (client.connect(server, 443)) {
    client.println("POST /particle/log HTTP/1.0");
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.print(data);
    client.println();
    // Wait up to 5 seconds for a response (just to clear the buffer)
    unsigned long timeout = millis() + 5000;
    while (client.connected() && millis() < timeout) {
      while (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }
    client.stop();
  } else {
    Serial.println("Failed to connect to server for POST!");
  }
}