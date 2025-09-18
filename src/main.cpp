#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <IRremote.hpp>
#include <time.h>
#include "esp_sntp.h"

// Include WiFi credentials (copy secrets_template.h to secrets.h and configure)
#include "secrets.h"

// Hardware pins
#define IR_SEND_PIN 4
#define LED_PIN LED_BUILTIN

// WiFi Configuration from secrets.h
const char* WIFI_SSID_CONFIG = WIFI_SSID;
const char* WIFI_PASSWORD_CONFIG = WIFI_PASSWORD;
const char* AP_SSID_CONFIG = AP_SSID;
const char* AP_PASSWORD_CONFIG = AP_PASSWORD;
const unsigned long WIFI_TIMEOUT = 10000;

// Global objects
WebServer server(80);

// NTP Configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;     // GMT+1 for Germany
const int daylightOffset_sec = 3600; // Daylight saving time offset
bool ntpSynced = false;

// Session states
enum SessionState {
    STATE_IDLE = 0,
    STATE_RUNNING = 1,
    STATE_PAUSED = 2,
    STATE_COMPLETED = 3
};

// Session management
struct SessionData {
    SessionState state = STATE_IDLE;
    uint16_t totalMinutes = 0;
    uint16_t totalShots = 0;
    uint16_t currentShot = 0;
    unsigned long sessionStartTime = 0;
    unsigned long nextShotTime = 0;
    uint32_t intervalMs = 60000;
    float lastTemperature = 20.0;
} session;

// Constants
const uint16_t MAX_SESSION_MINUTES = 480;

// Function declarations
bool connectToWiFi();
void setupWiFiAP();
void setupWebServer();
void setupHardware();
void setupNTP();
void syncTimeCallback(struct timeval *tv);
void handleWebServerClient();
void updateSession();
void executeShot();
void handleRoot();
void handleAPI();
void handleStart();
void handleStop();
void handleSingleShot();
void handleBurstShot();
void handleSystemOverview();
uint16_t calculateTotalShots(uint16_t minutes);
uint32_t calculateIntervalMs(uint16_t minutes);

void setup() {
    Serial.begin(115200);
    Serial.println("=== AstroController Rev 1 - ESP32 Starting ===");
    
    // Initialize hardware
    setupHardware();
    
    // Try to connect to local WiFi first, fallback to AP mode
    if (!connectToWiFi()) {
        Serial.println("Failed to connect to local WiFi, starting Access Point...");
        setupWiFiAP();
    } else {
        // Setup NTP client if connected to internet
        setupNTP();
    }
    
    // Setup web server
    setupWebServer();
    
    Serial.println("ESP32 initialization complete");
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("Connected to WiFi. Local IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("Access Point IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

void loop() {
    // Handle web server
    handleWebServerClient();
    
    // Update session logic
    updateSession();
    
    delay(100); // Small delay to prevent watchdog issues
}

void setupHardware() {
    // Initialize IR sender using IRremote library
    IrSender.begin(IR_SEND_PIN);
    Serial.println("IR sender initialized on pin " + String(IR_SEND_PIN));
}

bool connectToWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID_CONFIG);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID_CONFIG, WIFI_PASSWORD_CONFIG);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Signal strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        return true;
    } else {
        Serial.printf("WiFi connection failed. Status: %d\n", WiFi.status());
        return false;
    }
}

void setupWiFiAP() {
    WiFi.mode(WIFI_AP);
    delay(100);
    
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    bool result = WiFi.softAP(AP_SSID_CONFIG, AP_PASSWORD_CONFIG);
    delay(1000);

    if (result) {
        Serial.printf("Access Point '%s' started successfully\n", AP_SSID_CONFIG);
        Serial.print("AP IP Address: ");
        Serial.println(WiFi.softAPIP());
        Serial.printf("AP Password: %s\n", AP_PASSWORD_CONFIG);
        Serial.println("Connect to AstroController WiFi and go to http://192.168.4.1");
    } else {
        Serial.println("Failed to start Access Point!");
    }
}

void syncTimeCallback(struct timeval *tv) {
    ntpSynced = true;
    Serial.println("NTP time synchronized successfully!");
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

void setupNTP() {
    Serial.println("Setting up NTP client...");
    
    // Set time sync notification callback
    sntp_set_time_sync_notification_cb(syncTimeCallback);
    
    // Configure SNTP
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntpServer);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_init();
    
    // Set timezone
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    
    Serial.printf("NTP server: %s\n", ntpServer);
    Serial.println("Waiting for time synchronization...");
    
    // Wait for time sync (with timeout)
    int timeout = 30; // 30 seconds timeout
    while (!ntpSynced && timeout > 0) {
        delay(1000);
        timeout--;
        Serial.print(".");
    }
    Serial.println();
    
    if (ntpSynced) {
        Serial.println("NTP synchronization successful!");
    } else {
        Serial.println("NTP synchronization timeout - continuing without sync");
    }
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/api/status", handleAPI);
    server.on("/start", HTTP_POST, handleStart);
    server.on("/stop", HTTP_POST, handleStop);
    server.on("/shot", HTTP_POST, handleSingleShot);
    server.on("/burst", HTTP_POST, handleBurstShot);
    server.on("/system", handleSystemOverview);
    
    server.begin();
    Serial.println("Web server started on port 80");
}

void handleWebServerClient() {
    server.handleClient();
}

void updateSession() {
    if (session.state != STATE_RUNNING) return;
    
    // Check if it's time for next shot
    if (millis() >= session.nextShotTime) {
        executeShot();
        
        session.currentShot++;
        session.nextShotTime = millis() + session.intervalMs;
        
        // Check if session is complete
        if (session.currentShot >= session.totalShots) {
            session.state = STATE_COMPLETED;
            Serial.println("Session completed!");
        }
    }
}

void executeShot() {
    Serial.printf("Taking shot %d/%d\n", session.currentShot + 1, session.totalShots);
    
    // Send Sony SIRC command using IRremote library
    IrSender.sendSony(0x1E3A, 0x2D, 3, 20);
    
    Serial.printf("Sent Sony IR: Address=0x%X, Command=0x%X, Bits=20, Repeats=3\n", 0x1E3A, 0x2D);
    
    delay(100);
}

uint16_t calculateTotalShots(uint16_t minutes) {
    // Calculate shots based on interval (10 seconds for testing)
    return (minutes * 60) / 10;
}

uint32_t calculateIntervalMs(uint16_t minutes) {
    // Fixed 10 second interval for testing
    return 10000;
}

// Web server handlers
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>AstroController Rev 1</title>";
    html += "<style>body { background: #1a1a1a; color: #ff6b6b; font-family: monospace; margin: 0; padding: 20px; }";
    html += ".container { max-width: 400px; margin: 0 auto; } h1 { text-align: center; color: #ff6b6b; }";
    html += ".status { background: #2a2a2a; padding: 15px; border-radius: 8px; margin: 20px 0; }";
    html += "button { background: #ff6b6b; color: #000; padding: 15px 20px; border: none; margin: 5px; border-radius: 4px; cursor: pointer; }";
    html += "input[type=\"number\"] { background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 10px; width: 100%; }";
    html += "</style></head><body><div class=\"container\">";
    html += "<h1>AstroController Rev 1</h1>";
    html += "<div class=\"status\" id=\"status\">System ready</div>";
    html += "<div><label>Total time (minutes):</label>";
    html += "<input type=\"number\" id=\"minutes\" value=\"60\" min=\"1\" max=\"480\" onchange=\"updateCalculation()\">";
    html += "<div id=\"calculation\" style=\"margin: 10px 0; color: #ccc;\"></div>";
    html += "<button onclick=\"startSession()\">Start Session</button>";
    html += "<button onclick=\"stopSession()\">Stop</button>";
    html += "<button onclick=\"takeSingleShot()\" style=\"background: #4CAF50;\">Single Shot</button>";
    html += "<button onclick=\"takeBurstShot()\" style=\"background: #FF9800;\">10 Shot Burst</button>";
    html += "<br><a href=\"/system\" style=\"color: #ff6b6b; text-decoration: none;\">System Overview</a></div>";
    html += "<div id=\"progress\"></div></div><script>";
    html += "function calculateShots(minutes) { return Math.floor((minutes * 60) / 10); }";
    html += "function calculateInterval(minutes) { return 10; }";
    html += "function updateCalculation() { const minutes = parseInt(document.getElementById('minutes').value) || 60; ";
    html += "const shots = calculateShots(minutes); const interval = calculateInterval(minutes); ";
    html += "document.getElementById('calculation').innerHTML = shots + ' shots, every ' + interval + 's'; }";
    html += "function startSession() { const minutes = parseInt(document.getElementById('minutes').value); ";
    html += "fetch('/start', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({minutes: minutes}) });";
    html += "} function stopSession() { fetch('/stop', {method: 'POST'}); }";
    html += "function takeSingleShot() { fetch('/shot', {method: 'POST'}); }";
    html += "function takeBurstShot() { fetch('/burst', {method: 'POST'}); }";
    html += "function updateStatus() { fetch('/api/status').then(r => r.json()).then(data => {";
    html += "const statusEl = document.getElementById('status'); const progressEl = document.getElementById('progress');";
    html += "if (data.state === 1) { statusEl.innerHTML = 'Session active - Photo ' + data.current + '/' + data.total; ";
    html += "progressEl.innerHTML = data.remaining + ' minutes remaining<br>Temp: ' + data.temperature.toFixed(1) + '°C'; }";
    html += "else if (data.state === 3) { statusEl.innerHTML = 'Session completed!'; progressEl.innerHTML = data.total + ' photos taken'; }";
    html += "else { statusEl.innerHTML = 'System ready'; progressEl.innerHTML = 'Temp: ' + data.temperature.toFixed(1) + '°C'; }";
    html += "}); } setInterval(updateStatus, 5000); updateStatus(); updateCalculation(); </script></body></html>";
    
    server.send(200, "text/html", html);
}

void handleAPI() {
    DynamicJsonDocument doc(512);
    
    doc["state"] = session.state;
    doc["current"] = session.currentShot;
    doc["total"] = session.totalShots;
    doc["remaining"] = 0;
    
    if (session.state == STATE_RUNNING && session.totalShots > 0) {
        uint16_t remainingShots = session.totalShots - session.currentShot;
        doc["remaining"] = (remainingShots * session.intervalMs) / 60000;
    }
    
    doc["temperature"] = session.lastTemperature;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleStart() {
    if (session.state != STATE_IDLE) {
        server.send(400, "application/json", "{\"error\":\"Session already running\"}");
        return;
    }
    
    DynamicJsonDocument doc(256);
    deserializeJson(doc, server.arg("plain"));
    
    uint16_t minutes = doc["minutes"];
    if (minutes < 1 || minutes > MAX_SESSION_MINUTES) {
        server.send(400, "application/json", "{\"error\":\"Invalid duration\"}");
        return;
    }
    
    // Start session
    session.totalMinutes = minutes;
    session.totalShots = calculateTotalShots(minutes);
    session.intervalMs = calculateIntervalMs(minutes);
    session.currentShot = 0;
    session.sessionStartTime = millis();
    session.nextShotTime = millis() + 5000; // 5s delay
    session.state = STATE_RUNNING;
    
    Serial.printf("Session started: %d minutes, %d shots\n", minutes, session.totalShots);
    
    server.send(200, "application/json", "{\"success\":true}");
}

void handleStop() {
    if (session.state == STATE_RUNNING) {
        session.state = STATE_IDLE;
        Serial.println("Session stopped");
    }
    
    server.send(200, "application/json", "{\"success\":true}");
}

void handleSingleShot() {
    Serial.println("Single shot triggered");
    executeShot();
    server.send(200, "application/json", "{\"success\":true}");
}

void handleBurstShot() {
    Serial.println("10-shot burst triggered");
    
    for (int i = 0; i < 10; i++) {
        Serial.printf("Burst shot %d/10\n", i + 1);
        executeShot();
        
        if (i < 9) {
            delay(1000); // 1 second delay between burst shots
        }
    }
    
    Serial.println("Burst sequence completed");
    server.send(200, "application/json", "{\"success\":true}");
}

void handleSystemOverview() {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>AstroController Rev 1 - System Overview</title>";
    html += "<style>body { background: #1a1a1a; color: #ff6b6b; font-family: monospace; margin: 0; padding: 20px; }";
    html += ".container { max-width: 800px; margin: 0 auto; } h1, h2 { color: #ff6b6b; }";
    html += ".component { background: #2a2a2a; padding: 15px; border-radius: 8px; margin: 15px 0; }";
    html += ".status-ok { color: #4CAF50; } .status-warn { color: #FFC107; } .status-err { color: #F44336; }";
    html += ".back { color: #ff6b6b; text-decoration: none; }";
    html += "</style></head><body><div class=\"container\">";
    
    html += "<h1>AstroController Rev 1 System</h1>";
    html += "<a href=\"/\" class=\"back\">← Back to Control</a>";
    
    html += "<h2>System Architecture</h2>";
    html += "<div class=\"component\">";
    html += "<h3>ESP32 D32 Pro (Standalone)</h3>";
    html += "<p><strong>Function:</strong> IR sender for camera, session management, web interface</p>";
    html += "<p><strong>Hardware:</strong> IR LED on GPIO 4, WiFi, HTTP API</p>";
    html += "<p><strong>Status:</strong> <span class=\"status-ok\">Online</span></p>";
    html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    html += "<p><strong>Memory:</strong> " + String(ESP.getFreeHeap() / 1024) + " KB free</p>";
    html += "</div>";
    
    html += "<h2>Current Session</h2>";
    html += "<div class=\"component\">";
    html += "<p><strong>Status:</strong> ";
    
    switch(session.state) {
        case STATE_IDLE:
            html += "<span class=\"status-ok\">Ready</span>";
            break;
        case STATE_RUNNING:
            html += "<span class=\"status-warn\">Running</span>";
            break;
        case STATE_COMPLETED:
            html += "<span class=\"status-ok\">Completed</span>";
            break;
        default:
            html += "<span class=\"status-err\">Unknown</span>";
    }
    
    html += "</p>";
    html += "<p><strong>Photo Interval:</strong> " + String(calculateIntervalMs(1)/1000) + " seconds</p>";
    html += "<p><strong>Photos Taken:</strong> " + String(session.currentShot) + " / " + String(session.totalShots) + "</p>";
    html += "<p><strong>Runtime:</strong> " + String((millis() - session.sessionStartTime) / 1000) + " seconds</p>";
    html += "<p><strong>Temperature:</strong> " + String(session.lastTemperature, 1) + "°C (simulated)</p>";
    html += "</div>";
    
    html += "<h2>Hardware Status</h2>";
    html += "<div class=\"component\">";
    html += "<p><strong>IR Sender:</strong> <span class=\"status-ok\">Ready (Sony SIRC 20-bit)</span></p>";
    html += "<p><strong>WiFi:</strong> <span class=\"status-ok\">Connected</span> (RSSI: " + String(WiFi.RSSI()) + " dBm)</p>";
    html += "<p><strong>Web Server:</strong> <span class=\"status-ok\">Running on Port 80</span></p>";
    
    // Add NTP status
    if (WiFi.status() == WL_CONNECTED) {
        if (ntpSynced) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                html += "<p><strong>NTP Sync:</strong> <span class=\"status-ok\">Synchronized</span> (" + String(timeStr) + ")</p>";
            } else {
                html += "<p><strong>NTP Sync:</strong> <span class=\"status-ok\">Synchronized</span></p>";
            }
        } else {
            html += "<p><strong>NTP Sync:</strong> <span class=\"status-warn\">Not Synchronized</span></p>";
        }
    } else {
        html += "<p><strong>NTP Sync:</strong> <span class=\"status-warn\">AP Mode - No Internet</span></p>";
    }
    html += "</div>";
    
    html += "<h2>Usage</h2>";
    html += "<div class=\"component\">";
    html += "<p><strong>1. Start Session:</strong> Enter time → Start Session</p>";
    html += "<p><strong>2. Single Shot:</strong> Single Shot button</p>";
    html += "<p><strong>3. Automated:</strong> ESP32 handles timing automatically</p>";
    html += "<p><strong>4. Network:</strong> Web interface via WiFi</p>";
    html += "</div>";
    
    html += "<p style=\"text-align: center; margin-top: 30px; color: #666;\">";
    html += "AstroController Rev 1 | ESP32 Standalone | Uptime: " + String(millis() / 1000) + "s";
    html += "</p>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}