#include "app/Application.hpp"

// Global application instance
app::Application gApp;

void setup() {
    gApp.setup();
}

void loop() {
    gApp.loop();
}
