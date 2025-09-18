#ifndef CONFIG_H
#define CONFIG_H

// AstroController Rev 1 Configuration

// Hardware Configuration
#define IR_SEND_PIN 4

// Network Configuration
const char* WIFI_SSID = "heimfritz24";
const char* WIFI_PASSWORD = "3298306523051510";
const char* AP_SSID = "AstroController";
const char* AP_PASSWORD = "astro2024";
const unsigned long WIFI_TIMEOUT = 10000;

// Session Configuration
const uint16_t MAX_SESSION_MINUTES = 480;

// Sony IR Remote Configuration
const uint16_t SONY_ADDRESS = 0x1E3A;
const uint16_t SONY_COMMAND = 0x2D;
const uint8_t SONY_BITS = 20;
const uint8_t SONY_REPEATS = 3;

// Default timing
const uint32_t DEFAULT_INTERVAL_MS = 10000; // 10 seconds for testing

#endif // CONFIG_H