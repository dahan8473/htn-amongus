#pragma once

// Brings up WiFi station mode and starts connecting to the router, using
// credentials from wifi_credentials.h (gitignored -- see
// wifi_credentials.h.example). Connection itself continues asynchronously
// via a registered event handler that auto-retries on drop.
void setupWiFi();

bool isWiFiConnected();
