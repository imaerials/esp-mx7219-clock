#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

// Define pins for the LED matrix
#define CLK_PIN   D5  // or GPIO14
#define DATA_PIN  D7  // or GPIO13
#define CS_PIN    D4  // or GPIO2

// Scrolling message parameters
#define SCROLL_DELAY 150  // Increased delay for slower scrolling (milliseconds)

// Define matrix properties
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define TOTAL_COLUMNS MAX_DEVICES * 8

// Create matrix object
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variables for time tracking
unsigned long lastUpdate = 0;
const long updateInterval = 1000; // Update every second
bool showColon = true;
bool configMode = false;

// Message scrolling variables
String currentMessage = "";
bool isScrolling = false;
int scrollPosition = 0;
int scrollCount = 0;
const int MAX_SCROLL_COUNT = 3;  // Number of times to scroll the message
unsigned long lastScrollUpdate = 0;

// Web Server
ESP8266WebServer server(80);

// Function prototypes
void displayTime(int hours, int minutes);
void displayError();
void checkWiFiConnection();
void scrollMessage();
void setupAPI();
void handleMessage();
void handleStopScroll();
void displayIPAddress(); // New prototype

// Variables for IP display
bool showingIP = false;
String ipToShow = "";

void setup() {
    Serial.begin(115200);
    
    // Initialize the LED matrix
    mx.begin();
    
    // Initialize all devices
    for (int i = 0; i < MAX_DEVICES; i++) {
        mx.control(i, MD_MAX72XX::INTENSITY, 1); // Set initial brightness (0-15)
        mx.control(i, MD_MAX72XX::SHUTDOWN, false); // Ensure display is on
        mx.control(i, MD_MAX72XX::DECODE, false); // No decode for digits
    }
    mx.clear();
    
    // Display "WIFI" while connecting
    mx.setChar(31, 'W');
    mx.setChar(23, 'I');
    mx.setChar(15, 'F');
    mx.setChar(7, 'I');
    mx.update();

    // Initialize WiFiManager
    WiFiManager wifiManager;
    
    // Set callback for entering configuration mode
    wifiManager.setAPCallback([](WiFiManager* mgr) {
        configMode = true;
        mx.clear();
        mx.setChar(31, 'C');
        mx.setChar(23, 'O');
        mx.setChar(15, 'N');
        mx.setChar(7, 'F');
        mx.update();
    });

    // Try to connect to WiFi
    if (!wifiManager.autoConnect("Clock-Setup")) {
        Serial.println("Failed to connect and hit timeout");
        ESP.restart();
    }
    configMode = false;
    
    // Wait a moment for the connection to stabilize
    delay(1000);
    
    // Display IP address once connected
    displayIPAddress();
    
    // Initialize time client
    timeClient.begin();
    timeClient.setTimeOffset(-10800); // Argentina timezone (GMT-3)
    
    // Setup API endpoints
    setupAPI();
    Serial.println("Web API started");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();  // Handle API requests
  
  if (WiFi.status() != WL_CONNECTED) {
    checkWiFiConnection();
    return;
  }

  unsigned long currentMillis = millis();
  
  // Handle scrolling message if active
  if (isScrolling) {
    scrollMessage();
  } else if (currentMillis - lastUpdate >= updateInterval) {
    timeClient.update();
    
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    
    displayTime(hours, minutes);
    showColon = !showColon;
    lastUpdate = currentMillis;
    
    // Add brightness control based on time of day
    if (hours >= 22 || hours < 6) {
      mx.control(MD_MAX72XX::INTENSITY, 0); // Dim at night
    } else {
      mx.control(MD_MAX72XX::INTENSITY, 3); // Brighter during the day
    }
  }
}

// Define custom large number patterns (8x8 each)
const uint8_t PROGMEM largeNumbers[10][8] = {
    {
        B00111100,  // 0
        B01111110,
        B11000110,
        B11000110,
        B11000110,
        B11000110,
        B01111110,
        B00111100
    },
    {
        B00011000,  // 1
        B00111000,
        B01111000,
        B00011000,
        B00011000,
        B00011000,
        B01111110,
        B01111110
    },
    {
        B01111100,  // 2
        B11111110,
        B11000110,
        B00001110,
        B00111100,
        B01111000,
        B11111110,
        B11111110
    },
    {
        B01111100,  // 3
        B11111110,
        B11000110,
        B00111100,
        B00000110,
        B11000110,
        B11111110,
        B01111100
    },
    {
        B00011110,  // 4
        B00111110,
        B01101110,
        B11001110,
        B11111110,
        B00001110,
        B00001110,
        B00001110
    },
    {
        B11111110,  // 5
        B11111110,
        B11000000,
        B11111100,
        B00000110,
        B11000110,
        B11111110,
        B01111100
    },
    {
        B01111100,  // 6
        B11111110,
        B11000000,
        B11111100,
        B11000110,
        B11000110,
        B11111110,
        B01111100
    },
    {
        B11111110,  // 7
        B11111110,
        B00000110,
        B00001100,
        B00011000,
        B00110000,
        B01100000,
        B11000000
    },
    {
        B01111100,  // 8
        B11111110,
        B11000110,
        B01111100,
        B11000110,
        B11000110,
        B11111110,
        B01111100
    },
    {
        B01111100,  // 9
        B11111110,
        B11000110,
        B11000110,
        B01111110,
        B00000110,
        B11111110,
        B01111100
    }
};

void displayLargeDigit(int digit, int module) {
    for (int row = 0; row < 8; row++) {
        mx.setRow(module, row, pgm_read_byte(&(largeNumbers[digit][row])));
    }
}

void displayTime(int hours, int minutes) {
    mx.clear();
    
    // Split time into digits
    int h1 = hours / 10;    // First digit of hours
    int h2 = hours % 10;    // Second digit of hours
    int m1 = minutes / 10;  // First digit of minutes
    int m2 = minutes % 10;  // Second digit of minutes

    // Display large digits (8x8 each)
    displayLargeDigit(h1, 3);  // Hours tens (leftmost module)
    displayLargeDigit(h2, 2);  // Hours ones
    
    // Display colon using two modules in the middle
    if (showColon) {
        // Draw larger colon dots
        mx.setPoint(15, 2, true);
        mx.setPoint(15, 3, true);
        mx.setPoint(16, 2, true);
        mx.setPoint(16, 3, true);
        
        mx.setPoint(15, 5, true);
        mx.setPoint(15, 6, true);
        mx.setPoint(16, 5, true);
        mx.setPoint(16, 6, true);
    }
    
    displayLargeDigit(m1, 1);  // Minutes tens
    displayLargeDigit(m2, 0);  // Minutes ones (rightmost module)
    
    mx.update();
}

void displayError() {
  mx.clear();
  mx.setChar(24, 'E');
  mx.setChar(16, 'R');
  mx.setChar(8, 'R');
  mx.setChar(0, ' ');
}

void checkWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  const unsigned long checkInterval = 30000; // Check every 30 seconds
  
  if (millis() - lastWiFiCheck >= checkInterval) {
    displayError();
    Serial.println("WiFi disconnected. Attempting to reconnect...");
    WiFi.begin();
    lastWiFiCheck = millis();
  }
}

// Function to scroll text
void scrollMessage() {
    if (!isScrolling || currentMessage.length() == 0) return;
    
    if (millis() - lastScrollUpdate >= SCROLL_DELAY) {
        mx.clear();
        
        int charPosition = 31;  // Start from leftmost position
        size_t messageIndex = static_cast<size_t>(scrollPosition);
        
        // Display as many characters as will fit
        while (charPosition >= 0 && messageIndex < currentMessage.length()) {
            mx.setChar(charPosition, currentMessage[messageIndex]);
            charPosition -= 8;
            messageIndex++;
        }
        
        mx.update();
        scrollPosition++;
        
        // Check if one complete scroll cycle is done
        if (scrollPosition >= static_cast<int>(currentMessage.length() + MAX_DEVICES)) {
            scrollPosition = 0;
            scrollCount++;
            
            // Reset after appropriate number of scrolls (1 for IP, MAX_SCROLL_COUNT for messages)
            if ((showingIP && scrollCount >= 1) || (!showingIP && scrollCount >= MAX_SCROLL_COUNT)) {
                isScrolling = false;
                currentMessage = "";
                showingIP = false;
                scrollCount = 0;
            }
        }
        
        lastScrollUpdate = millis();
    }
}

// API endpoint handlers
void handleMessage() {
    if (server.hasArg("plain")) {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, server.arg("plain"));
        
        if (error) {
            server.send(400, "text/plain", "Invalid JSON");
            return;
        }
        
        if (doc.containsKey("message")) {
            currentMessage = doc["message"].as<String>();
            scrollPosition = 0;
            scrollCount = 0;
            isScrolling = true;
            
            String response = "{\"status\":\"success\",\"message\":\"" + currentMessage + "\",\"scrolls\":\"" + String(MAX_SCROLL_COUNT) + "\"}";
            server.send(200, "application/json", response);
        } else {
            server.send(400, "text/plain", "Missing message parameter");
        }
    } else {
        server.send(400, "text/plain", "No message provided");
    }
}

void handleStopScroll() {
    isScrolling = false;
    currentMessage = "";
    mx.clear();
    mx.update();
    server.send(200, "text/plain", "Scrolling stopped");
}

void setupAPI() {
    // API endpoints
    server.on("/api/message", HTTP_POST, handleMessage);
    server.on("/api/stop", HTTP_POST, handleStopScroll);
    
    // Simple status endpoint
    server.on("/api/status", HTTP_GET, []() {
        StaticJsonDocument<200> doc;
        doc["scrolling"] = isScrolling;
        doc["message"] = currentMessage;
        
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
    });
    
    server.begin();
}

void displayIPAddress() {
    if (ipToShow.isEmpty()) {
        ipToShow = WiFi.localIP().toString();
    }
    
    // Use the existing scroll mechanism
    currentMessage = "IP: " + ipToShow;
    scrollPosition = 0;
    scrollCount = 0;
    isScrolling = true;
    showingIP = true;
    
    // Let it scroll through the message once
    while (isScrolling) {
        scrollMessage();
        delay(SCROLL_DELAY);
        
        // Stop after one complete scroll
        if (scrollCount >= 1) {
            isScrolling = false;
            currentMessage = "";
            showingIP = false;
        }
    }
}