# WiFi Clock with LED Matrix Display

This project implements a WiFi-enabled clock using a NodeMCU (ESP8266) and 4x MAX7219 LED matrix modules. The clock displays time and supports scrolling text messages through a REST API.

## Hardware Requirements

- NodeMCU (ESP8266)
- 4x MAX7219 8x8 LED Matrix Modules (32x8 total display)
- Connections:
  - CLK: D5 (GPIO14)
  - DATA: D7 (GPIO13)
  - CS: D4 (GPIO2)

## Features

- Automatic WiFi configuration using WiFiManager
- NTP time synchronization
- Argentina timezone (GMT-3)
- Automatic brightness adjustment (dimmer at night)
- HTTP API for displaying scrolling messages

## API Documentation

The clock exposes a REST API on port 80 that allows control of the display messages.

### Display a Scrolling Message

Send a text message to be displayed on the LED matrix.

```
POST /api/message
```

#### Request Body

```json
{
    "message": "Your text here"
}
```

#### Response

```json
{
    "status": "success",
    "message": "Your text here",
    "scrolls": "3"
}
```

#### Notes
- The message will scroll across the display 3 times
- After completing the scrolls, the display returns to showing the time
- Maximum recommended message length: 100 characters

### Stop Scrolling Message

Stop the current scrolling message and return to time display.

```
POST /api/stop
```

#### Response

```
Scrolling stopped
```

### Get Display Status

Get the current status of the display.

```
GET /api/status
```

#### Response

```json
{
    "scrolling": true|false,
    "message": "Current message (if any)"
}
```

## Example Usage

### Display a Message

```bash
curl -X POST http://[CLOCK-IP]/api/message \
     -H "Content-Type: application/json" \
     -d '{"message": "Hello World!"}'
```

### Stop Scrolling

```bash
curl -X POST http://[CLOCK-IP]/api/stop
```

### Get Status

```bash
curl http://[CLOCK-IP]/api/status
```

## Initial Setup

1. Power on the device
2. Connect to the "Clock-Setup" WiFi network
3. Follow the configuration portal to set up your WiFi credentials
4. The device will connect to your network and start displaying the time

## Notes

- The display automatically dims between 22:00 and 06:00
- Scroll speed can be adjusted by modifying the `SCROLL_DELAY` constant (default: 150ms)
- The device maintains accurate time through NTP synchronization

## Display Behavior

- Time is displayed in 24-hour format (HH:MM)
- Uses custom 8x8 patterns for large, clear digits
- Special displays:
  - "WIFI" during initial WiFi setup
  - "CONF" when in configuration mode
  - "ERR" if WiFi connection is lost

## Error Handling

- WiFi Connection Loss:
  - Display shows "ERR"
  - Automatic reconnection attempts every 30 seconds
  - Returns to normal display once connection is restored
- API Errors:
  - Invalid JSON requests receive 400 Bad Request responses
  - Failed requests include error description in response
  - Malformed messages are rejected with appropriate error messages
