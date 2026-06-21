/*
 * TheraBot — wristband ESP32 firmware (PLACEHOLDER — fill in the TODOs).
 *
 * Connects to WiFi (phone hotspot), reads the MPU6050 accelerometer over
 * I2C, and pushes samples as a WebSocket CLIENT to the laptop orchestrator.
 * This is a one-way leg: wristband -> laptop only. It never receives
 * commands back (the plushie motor is now driven by plain Python function
 * calls in plushie_actions.py, not by this device).
 *
 * Contract (must match orchestrator/brain/config.py + server.py):
 *   route : ws://<laptop-ip>:8765/sensor
 *   payload (one JSON object per sample):
 *     {"ax": <float>, "ay": <float>, "az": <float>, "t": <float seconds>}
 *
 * Library: WebSockets by Markus Sattler (arduinoWebSockets / "Links2004")
 *          Install via Arduino IDE Library Manager.
 * Also needs: Adafruit_MPU6050, Adafruit_Sensor, ArduinoJson.
 *
 * Wiring:
 *   MPU6050 SDA -> ESP32 GPIO21, SCL -> GPIO22, VCC -> 3.3V, GND -> GND
 */
#include <Wire.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>

// ---------------- EDIT THESE ----------------
// TODO(wristband-teammate): fill in your hotspot creds and the laptop's IP
// (find it with `ipconfig getifaddr en0` on Mac / `hostname -I` on Linux).
const char* WIFI_SSID  = "TODO-your-hotspot-name";
const char* WIFI_PASS  = "TODO-your-hotspot-password";
const char* LAPTOP_HOST = "TODO-192.168.x.x";
const uint16_t LAPTOP_PORT = 8765;
const char* SENSOR_ROUTE = "/sensor";
// ---------------------------------------------

#define SAMPLE_HZ 50

Adafruit_MPU6050 mpu;
WebSocketsClient ws;

void onWsEvent(WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("[ws] connected to laptop");
      break;
    case WStype_DISCONNECTED:
      Serial.println("[ws] disconnected — will auto-retry");
      break;
    default:
      break;  // this device doesn't expect incoming messages
  }
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println();
  Serial.print("WiFi connected, ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // TODO(wristband-teammate): confirm sensor init settings (range/bandwidth)
  // match what infer_stress() in ml_processor.py expects/assumes.
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  connectWifi();

  ws.begin(LAPTOP_HOST, LAPTOP_PORT, SENSOR_ROUTE);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(2000);  // auto-reconnect if the laptop drops/restarts
}

void loop() {
  ws.loop();  // must be called every iteration for the WS client to work

  static uint32_t lastSample = 0;
  uint32_t now = millis();
  if (now - lastSample >= 1000 / SAMPLE_HZ) {
    lastSample = now;

    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    StaticJsonDocument<128> doc;
    doc["ax"] = a.acceleration.x;
    doc["ay"] = a.acceleration.y;
    doc["az"] = a.acceleration.z;
    doc["t"]  = now / 1000.0;

    String out;
    serializeJson(doc, out);
    ws.sendTXT(out);
  }
}
