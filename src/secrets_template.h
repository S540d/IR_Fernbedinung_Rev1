#ifndef SECRETS_H
#define SECRETS_H

// ============================================================================
// SECRETS TEMPLATE - Kopiere diese Datei zu 'secrets.h' und fülle die Werte aus
// ============================================================================
// WICHTIG: secrets.h ist bereits in .gitignore und wird NICHT ins Repository übertragen!
// Diese Template-Datei dient als Vorlage für andere Entwickler.

// WiFi Konfiguration für Station Mode
#define WIFI_SSID "IhrWLANName"          // Name des WLAN-Netzwerks
#define WIFI_PASSWORD "IhrWLANPasswort"  // WLAN-Passwort

// Access Point Konfiguration (optional anpassbar)
#define AP_SSID "AstroController"        // Name des ESP32 Hotspots
#define AP_PASSWORD "astro2024"          // Passwort für ESP32 Hotspot

// Optional: Weitere Netzwerk-Einstellungen
// #define WIFI_HOSTNAME "ir-controller-esp32"
// #define WIFI_TIMEOUT_MS 10000

// NTP Server Konfiguration
// #define NTP_SERVER "pool.ntp.org"
// #define GMT_OFFSET_SEC 3600      // GMT+1 für Deutschland
// #define DAYLIGHT_OFFSET_SEC 3600 // Sommerzeit

// Web Server Konfiguration
// #define WEB_SERVER_PORT 80
// #define API_KEY "your-api-key-here"  // Für API-Authentifizierung

#endif