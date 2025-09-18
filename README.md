# IR Remote Controller ESP32

ESP32-based IR remote controller with web interface for programmable infrared control.

## Description

This project implements an IR remote controller using ESP32 with a web-based interface. The device can send infrared signals on programmable schedules and provides a comprehensive web interface for configuration and control.

## Hardware

- **ESP32 Dev Module**
- **IR LED**: Connected to pin 4 for infrared transmission
- **WiFi**: Built-in ESP32 WiFi for web interface

## Features

- **Web Interface**: Complete web-based control panel
- **WiFi Connectivity**: Connect to existing network or create access point
- **Session Management**: Programmable IR sessions with timing control
- **Real-time Control**: Start, pause, stop sessions via web interface
- **NTP Time Sync**: Accurate timing with network time synchronization
- **Temperature Monitoring**: Environmental data logging
- **RESTful API**: JSON-based API for external control

## Network Configuration

- **Station Mode**: Connect to existing WiFi network
- **Access Point Mode**: Create `AstroController` hotspot (password: `astro2024`)
- **Web Interface**: Accessible at ESP32 IP address on port 80

## API Endpoints

- `GET /api/status` - Get current system status
- `POST /api/session/start` - Start IR session
- `POST /api/session/pause` - Pause current session
- `POST /api/session/stop` - Stop current session
- `POST /api/session/config` - Configure session parameters

## Pin Configuration

- **IR Send Pin**: GPIO 4
- **Status LED**: Built-in LED

## Build and Upload

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio run --target monitor
```

## WiFi Setup

1. Connect to `AstroController` access point (password: `astro2024`)
2. Navigate to web interface
3. Configure WiFi credentials for station mode

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Author

Sven Strohkark