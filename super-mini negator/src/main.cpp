#include <WiFi.h>
#include <esp_wifi.h>

// ================== CONFIG ==================
#define BUTTON_PIN 0  // Boot button on standard ESP32 boards (active LOW)
#define LED_PIN    2  // Onboard LED GPIO pin
const char* fakeSSIDs[1000] = {
    // 1-100
    "TellMyWiFiLoveHer", "Loading...", "Searching..."
};
int currentChannel = 1;
const int numSSIDs = sizeof(fakeSSIDs) / sizeof(fakeSSIDs[0]);

void sendBeacon(const char* ssid);

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_PIN, OUTPUT);

  setCpuFrequencyMhz(240);
  // Clean start
  esp_wifi_stop();
  delay(100);
  WiFi.mode(WIFI_MODE_NULL);
  delay(100);

  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  delay(200);

  uint8_t mac[6];

  unsigned long pressStartTime = 0;
  
  while (true) {
    // The boot button is active-LOW (reads LOW when pressed)
    if (digitalRead(BUTTON_PIN) == LOW) {
      if (pressStartTime == 0) {
        pressStartTime = millis(); // Start the timer on initial press
      } else if (millis() - pressStartTime >= 2000) {
        Serial.println("Hold detected! Proceeding to loop...");
        break; // Exit the setup block's blocking loop
      }
    } else {
      // Reset timer if the button is released prematurely
      pressStartTime = 0;
    }
    
    delay(20); // Prevent watchdog trigger and debounce slightly
  }

  // Turn LED on once the button check passes
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  for (int i = 0; i < numSSIDs; i++) {
    sendBeacon(fakeSSIDs[i]);
  }
}

void sendBeacon(const char* ssid) {
  int ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  uint8_t packet[200];  // bigger buffer to be safe
  int offset = 0;

  // Fixed beacon header
  packet[offset++] = 0x80;  // Management, Beacon
  packet[offset++] = 0x00;
  packet[offset++] = 0x00;  // Duration
  packet[offset++] = 0x00;

  // Destination = broadcast
  for (int i = 0; i < 6; i++) packet[offset++] = 0xFF;

  // Source MAC + BSSID (randomized)
  uint8_t mac[6];
  for (int j = 0; j < 6; j++) mac[j] = random(0, 256);
  for (int i = 0; i < 6; i++) packet[offset++] = mac[i];  // Source
  for (int i = 0; i < 6; i++) packet[offset++] = mac[i];  // BSSID

  packet[offset++] = 0x00;  // Sequence control low
  packet[offset++] = 0x00;

  // Timestamp (8 zero bytes is fine)
  for (int i = 0; i < 8; i++) packet[offset++] = 0x00;

  packet[offset++] = 0x64;  // Beacon interval low
  packet[offset++] = 0x00;  // high

  packet[offset++] = 0x01;  // Capability low (open)
  packet[offset++] = 0x04;  // high

  // === SSID IE ===
  packet[offset++] = 0x00;           // Element ID: SSID
  packet[offset++] = ssidLen;
  memcpy(&packet[offset], ssid, ssidLen);
  offset += ssidLen;

  // Supported Rates
  packet[offset++] = 0x01;
  packet[offset++] = 0x08;
  uint8_t rates[8] = {0x82, 0x84, 0x8b, 0x96, 0x12, 0x24, 0x48, 0x6c};
  memcpy(&packet[offset], rates, 8);
  offset += 8;

  
  packet[offset++] = 0x03;
  packet[offset++] = 0x01;
  packet[offset++] = currentChannel;   

  // Extended Supported Rates (helps a lot with modern phones)
  packet[offset++] = 0x32;
  packet[offset++] = 0x04;
  uint8_t ext_rates[4] = {0x0c, 0x18, 0x30, 0x60};
  memcpy(&packet[offset], ext_rates, 4);
  offset += 4;

  // Send it
  esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, packet, offset, false);
  currentChannel++;
  
  if (currentChannel > 11) {
    currentChannel = 1;
  }
}