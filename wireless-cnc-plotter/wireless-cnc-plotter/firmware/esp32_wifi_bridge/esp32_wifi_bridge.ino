/*
  Wireless CNC plotter bridge
  Board: ESP32 Dev Module
  Libraries: ESP32Servo (install from Arduino Library Manager)

  Wi-Fi AP -> TCP client (port 23) -> UART2 -> Arduino Uno running GRBL 1.1.
  M3 lowers the pen, M5 raises it. Original G-code lines are still forwarded
  to GRBL so that each receives the normal GRBL ok/error response.

  UART2: ESP32 GPIO16 RX <- Uno D1 TX through a 1k/2k divider
         ESP32 GPIO17 TX -> Uno D0 RX (3.3 V logic is normally accepted)
  Servo signal: GPIO18. Servo power must come from an external regulated 5 V supply.
*/
#include <WiFi.h>
#include <ESP32Servo.h>
#include <math.h>

const char* WIFI_SSID = "Plotter-Setup";       // CHANGE before use
const char* WIFI_PASSWORD = "Plotter-1234";    // 8+ chars; CHANGE before use
constexpr uint16_t TCP_PORT = 23;
constexpr int UART_RX_PIN = 16;
constexpr int UART_TX_PIN = 17;
constexpr int SERVO_PIN = 18;
constexpr int PEN_UP_DEG = 75;     // tune to your linkage
constexpr int PEN_DOWN_DEG = 115;  // tune to your linkage
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2400;
constexpr size_t LINE_CAPACITY = 160;

WiFiServer server(TCP_PORT);
WiFiClient client;
Servo penServo;
char lineBuffer[LINE_CAPACITY];
size_t lineLength = 0;
bool droppingLongLine = false;
bool penIsDown = false;

void setPen(bool down) {
  if (down == penIsDown) return;
  penIsDown = down;
  penServo.write(down ? PEN_DOWN_DEG : PEN_UP_DEG);
}

// Parse M3/M5 outside semicolon and parenthesized comments.
void inspectPenCommand(const char* line) {
  bool inParenComment = false;
  for (const char* p = line; *p; ++p) {
    if (*p == ';') break;
    if (*p == '(') { inParenComment = true; continue; }
    if (*p == ')') { inParenComment = false; continue; }
    if (inParenComment || (*p != 'M' && *p != 'm')) continue;
    char* end = nullptr;
    const float code = strtof(p + 1, &end);
    if (end == p + 1) continue;
    if (fabsf(code - 3.0f) < 0.001f) setPen(true);
    else if (fabsf(code - 5.0f) < 0.001f) setPen(false);
    p = end - 1;
  }
}

void finishLine() {
  if (droppingLongLine) {
    if (client && client.connected()) client.println("error:line-too-long");
    lineLength = 0;
    droppingLongLine = false;
    return;
  }
  if (lineLength == 0) return; // ignore LF after CR / blank lines
  lineBuffer[lineLength] = '\0';
  inspectPenCommand(lineBuffer);
  Serial2.write(reinterpret_cast<const uint8_t*>(lineBuffer), lineLength);
  Serial2.write('\n');
  lineLength = 0;
}

void acceptClient() {
  WiFiClient incoming = server.available();
  if (!incoming) return;
  if (client && client.connected()) {
    incoming.println("busy: one sender at a time");
    incoming.stop();
    return;
  }
  client = incoming;
  client.setNoDelay(true);
  lineLength = 0;
  droppingLongLine = false;
}

void setup() {
  Serial.begin(115200); // USB diagnostics only; not used for Uno link
  Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  penServo.setPeriodHertz(50);
  penServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  penIsDown = true; // force first call to command a known pen-up angle
  setPen(false);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  server.begin();
  server.setNoDelay(true);
  Serial.print("Plotter AP: "); Serial.println(WIFI_SSID);
  Serial.print("TCP: "); Serial.print(WiFi.softAPIP()); Serial.print(':'); Serial.println(TCP_PORT);
}

void loop() {
  acceptClient();

  if (client && client.connected()) {
    while (client.available()) {
      const uint8_t c = static_cast<uint8_t>(client.read());
      // GRBL realtime bytes must not wait for a line terminator.
      if (c == '?' || c == '!' || c == '~' || c == 0x18 || c == 0x84 || c == 0x85) {
        Serial2.write(c);
        if (c == 0x18) setPen(false); // reset: leave pen safely raised
        continue;
      }
      if (c == '\r' || c == '\n') {
        finishLine();
      } else if (!droppingLongLine) {
        if (lineLength < LINE_CAPACITY - 1) lineBuffer[lineLength++] = static_cast<char>(c);
        else droppingLongLine = true;
      }
    }
  } else if (client) {
    client.stop();
    setPen(false);
    lineLength = 0;
    droppingLongLine = false;
  }

  while (Serial2.available()) {
    const uint8_t c = static_cast<uint8_t>(Serial2.read());
    if (client && client.connected()) client.write(c);
  }
}
