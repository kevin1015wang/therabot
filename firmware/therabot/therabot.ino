/*
   TheraBot - Sampling + Filtering Node (ESP32 DevKitC v4)

   EDGE half of the TheraBot pipeline (Stages 1 + 2):
     Stage 1  Sampling  : MPU6050 accel at 50 Hz, batched every ~2 s over BLE
     Stage 2  Filtering : moving-average filter (window = 5) on each axis

   Host pipeline (Stages 3-6) connects over BLE and listens for batches.
   Run ble_host.py on your Mac while this sketch is uploaded.

   Sensor wiring (MPU6050):
     VCC -> 3V3,  GND -> GND,  SDA -> GPIO21,  SCL -> GPIO22
   Motor driver wiring (L293D):
     Enable 1,2 -> GPIO25,  Input 1 -> GPIO33,  Input 2 -> GPIO32
   Serial Monitor at 115200 baud.

   Sample units on the wire: raw signed 16-bit counts at +/-2g.
   Host divides by 16384.0 to get g.
   Serial CSV rows include raw MPU6050 counts and converted g values.
*/

#include <Wire.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ===================== MPU6050 =====================
const int MPU = 0x68;

// ===================== Sampling (Stage 1) =====================
const int           SAMPLE_HZ        = 50;
const unsigned long SAMPLE_PERIOD_MS = 1000 / SAMPLE_HZ;
const int           BATCH_SECONDS    = 2;
const int           BATCH_SAMPLES    = SAMPLE_HZ * BATCH_SECONDS;
const int           CHUNK_SAMPLES    = 20;
const float         ACCEL_COUNTS_PER_G = 16384.0;

// ===================== Moving-average filter (Stage 2) =====================
const int MA_WINDOW = 5;
int16_t   maBuf[3][MA_WINDOW];
int       maIdx   = 0;
long      maSum[3] = {0, 0, 0};
bool      maFilled = false;

// ===================== Batch + outbox buffers =====================
int16_t batchBuf[BATCH_SAMPLES][3];
int     batchIdx = 0;

int16_t outBuf[BATCH_SAMPLES][3];
bool    outPending = false;
int     outChunk   = 0;
uint8_t batchId    = 0;
uint32_t batchesDropped = 0;

// ===================== BLE =====================
#define SERVICE_UUID   "6e646973-7468-6572-6162-6f7400000001"
#define DATA_CHAR_UUID "6e646973-7468-6572-6162-6f7400000002"
#define CMD_CHAR_UUID  "6e646973-7468-6572-6162-6f7400000003"

BLEServer*         pServer   = nullptr;
BLECharacteristic* pDataChar = nullptr;
BLECharacteristic* pCmdChar  = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// ===================== L293D motor =====================
const int MOTOR_ENA = 25;
const int MOTOR_IN1 = 33;
const int MOTOR_IN2 = 32;

enum MotorMode { MODE_CALM, MODE_AGITATED, MODE_ATTENTION, MODE_OFF };
MotorMode     motorMode       = MODE_CALM;
int           motorPhase      = 0;
unsigned long motorPhaseStart = 0;
unsigned long modeUntil       = 0;

enum MotorDrive { DRIVE_STOP, DRIVE_FORWARD, DRIVE_REVERSE };
MotorDrive motorDrive = DRIVE_STOP;
bool       motorSlow  = false;

const unsigned long SLOW_PWM_PERIOD_MS = 20;
const unsigned long SLOW_PWM_ON_MS     = 7;
unsigned long motorPwmCycleStart = 0;

const int AGITATED_REPEATS = 5;
int       agitatedRepeat   = 0;

unsigned long lastSampleMs = 0;
unsigned long lastStatusMs = 0;
uint32_t sampleNumber = 0;

// ---- I2C helpers ----
int16_t read16() {
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  return (int16_t)((hi << 8) | lo);
}

bool mpuPresent() {
  Wire.beginTransmission(MPU);
  return Wire.endTransmission() == 0;
}

void readAccel(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU, 6, true);
  ax = read16();
  ay = read16();
  az = read16();
}

// ---- Moving-average filter ----
void filterSample(int16_t rawX, int16_t rawY, int16_t rawZ,
                  int16_t &outX, int16_t &outY, int16_t &outZ) {
  int16_t raw[3] = {rawX, rawY, rawZ};
  for (int a = 0; a < 3; a++) {
    maSum[a] -= maBuf[a][maIdx];
    maBuf[a][maIdx] = raw[a];
    maSum[a] += raw[a];
  }
  maIdx = (maIdx + 1) % MA_WINDOW;
  if (maIdx == 0) maFilled = true;

  int divisor = maFilled ? MA_WINDOW : (maIdx == 0 ? MA_WINDOW : maIdx);
  if (divisor < 1) divisor = 1;
  outX = (int16_t)(maSum[0] / divisor);
  outY = (int16_t)(maSum[1] / divisor);
  outZ = (int16_t)(maSum[2] / divisor);
}

// ---- BLE callbacks ----
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) {
    deviceConnected = true;
    Serial.println("[ble] host connected");
  }

  void onDisconnect(BLEServer* s) {
    deviceConnected = false;
    Serial.println("[ble] host disconnected — re-advertising");
    delay(500);  // give the BLE stack time to tear down
    BLEDevice::startAdvertising();
    Serial.println("[ble] advertising again");
  }
};

void startCalmMode();
void startOffMode();
void startAgitatedPattern();
void handleLabelCommand(String label);

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) {
    String v = c->getValue();
    if (v.length() == 0) return;

    v.trim();
    v.toLowerCase();

    handleLabelCommand(v);
  }
};

// ---- Motor ----
void setMotorDrive(MotorDrive drive, bool slow) {
  motorDrive = drive;
  motorSlow = slow;

  if (drive == DRIVE_FORWARD) {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
  } else if (drive == DRIVE_REVERSE) {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
  } else {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    digitalWrite(MOTOR_ENA, LOW);
  }
}

void updateMotorOutput(unsigned long now) {
  if (motorDrive == DRIVE_STOP) {
    digitalWrite(MOTOR_ENA, LOW);
    return;
  }

  if (!motorSlow) {
    digitalWrite(MOTOR_ENA, HIGH);
    return;
  }

  if (now - motorPwmCycleStart >= SLOW_PWM_PERIOD_MS) {
    motorPwmCycleStart = now;
  }
  digitalWrite(MOTOR_ENA, (now - motorPwmCycleStart) < SLOW_PWM_ON_MS ? HIGH : LOW);
}

void motorForward()     { setMotorDrive(DRIVE_FORWARD, false); }
void motorReverse()     { setMotorDrive(DRIVE_REVERSE, false); }
void motorForwardSlow() { setMotorDrive(DRIVE_FORWARD, true);  }
void motorReverseSlow() { setMotorDrive(DRIVE_REVERSE, true);  }
void motorStop()        { setMotorDrive(DRIVE_STOP, false);    }

void startCalmMode() {
  motorMode = MODE_CALM;
  modeUntil = 0;
  motorPhase = 0;
  agitatedRepeat = 0;
  motorPhaseStart = millis();
  motorStop();
  Serial.println("[motor] label=calm action=stop");
}

void startOffMode() {
  motorMode = MODE_OFF;
  modeUntil = 0;
  motorPhase = 0;
  agitatedRepeat = 0;
  motorPhaseStart = millis();
  motorStop();
  Serial.println("[motor] OFF");
}

void startAgitatedPattern() {
  motorMode = MODE_AGITATED;
  modeUntil = 0;
  motorPhase = 0;
  agitatedRepeat = 0;
  motorPhaseStart = millis();
  motorPwmCycleStart = millis();
  Serial.println("[motor] label=agitated pattern=start");
}

void handleLabelCommand(String label) {
  label.trim();
  label.toLowerCase();

  if (label == "calm" || label == "c") {
    startCalmMode();
  } else if (label == "agitated" || label == "a") {
    startAgitatedPattern();
  } else if (label == "off" || label == "o") {
    startOffMode();
  } else if (label == "attention") {
    motorMode = MODE_ATTENTION;
    modeUntil = millis() + 3000;
    motorPhase = 0;
    motorPhaseStart = millis();
    Serial.println("[motor] ATTENTION");
  } else {
    Serial.printf("[motor] unknown label='%s'; staying calm\n", label.c_str());
    startCalmMode();
  }
}

void readSerialCommands() {
  static String serialCmd = "";

  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (serialCmd.length() > 0) {
        handleLabelCommand(serialCmd);
        serialCmd = "";
      }
    } else {
      serialCmd += ch;
    }
  }
}

void updateMotor() {
  if ((motorMode == MODE_CALM || motorMode == MODE_ATTENTION) && modeUntil != 0 && millis() > modeUntil) {
    startCalmMode();
  }

  unsigned long now = millis();

  if (motorMode == MODE_OFF || motorMode == MODE_CALM) {
    motorStop();
  } else if (motorMode == MODE_AGITATED) {
    unsigned long phaseDuration = 0;
    switch (motorPhase) {
      case 0: phaseDuration = 2000; break;
      case 1: phaseDuration = 5000; break;
      case 2: phaseDuration = 3000; break;
      case 3: phaseDuration = 5000; break;
    }

    if (now - motorPhaseStart >= phaseDuration) {
      motorPhaseStart = now;
      motorPhase++;
      if (motorPhase > 3) {
        motorPhase = 0;
        agitatedRepeat++;
        Serial.printf("[motor] agitated cycle %d/%d complete\n", agitatedRepeat, AGITATED_REPEATS);
      }
      if (agitatedRepeat >= AGITATED_REPEATS) {
        startCalmMode();
      }
    }

    if (motorMode == MODE_AGITATED) {
      switch (motorPhase) {
        case 0: motorForwardSlow(); break;
        case 1: motorStop();        break;
        case 2: motorReverseSlow(); break;
        case 3: motorStop();        break;
      }
    }
  } else if (motorMode == MODE_ATTENTION) {
    const unsigned long FLIP = 250;
    if (now - motorPhaseStart >= FLIP) { motorPhase = (motorPhase + 1) % 2; motorPhaseStart = now; }
    if (motorPhase == 0) motorForward(); else motorReverse();
  }

  updateMotorOutput(now);
}

// ---- BLE streaming ----
void streamOneChunk() {
  if (!outPending || !deviceConnected) return;

  int chunkCount = (BATCH_SAMPLES + CHUNK_SAMPLES - 1) / CHUNK_SAMPLES;
  int start = outChunk * CHUNK_SAMPLES;
  int n = BATCH_SAMPLES - start;
  if (n > CHUNK_SAMPLES) n = CHUNK_SAMPLES;

  uint8_t packet[4 + CHUNK_SAMPLES * 6];
  packet[0] = batchId;
  packet[1] = (uint8_t)outChunk;
  packet[2] = (uint8_t)chunkCount;
  packet[3] = (uint8_t)n;
  int p = 4;
  for (int i = 0; i < n; i++) {
    for (int a = 0; a < 3; a++) {
      int16_t v = outBuf[start + i][a];
      packet[p++] = (uint8_t)(v & 0xFF);
      packet[p++] = (uint8_t)((v >> 8) & 0xFF);
    }
  }

  pDataChar->setValue(packet, p);
  pDataChar->notify();

  outChunk++;
  if (outChunk >= chunkCount) {
    outPending = false;
    Serial.printf("[ble] sent batch %u (%d chunks)\n", batchId, chunkCount);
  }
}

void startBle() {
  BLEDevice::init("TheraBot");
  BLEDevice::setPower(ESP_PWR_LVL_P9);   // max TX power for easier discovery
  BLEDevice::setMTU(247);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService(SERVICE_UUID);

  pDataChar = svc->createCharacteristic(
    DATA_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pDataChar->addDescriptor(new BLE2902());

  pCmdChar = svc->createCharacteristic(
    CMD_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pCmdChar->setCallbacks(new CmdCallbacks());

  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);   // helps iOS / macOS discovery
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("[ble] advertising as 'TheraBot'");
  Serial.println("[ble] waiting for host — run ble_host.py on your Mac");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("=== TheraBot sampling node ===");

  Wire.begin(21, 22);
  if (!mpuPresent()) {
    Serial.println("[error] MPU6050 not found on I2C (SDA=21, SCL=22). Check wiring.");
  } else {
    Wire.beginTransmission(MPU); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true);
    delay(50);
    Wire.beginTransmission(MPU); Wire.write(0x1C); Wire.write(0x00); Wire.endTransmission(true);
    Wire.beginTransmission(MPU); Wire.write(0x1A); Wire.write(0x04); Wire.endTransmission(true);
    Wire.beginTransmission(MPU); Wire.write(0x19); Wire.write(0x13); Wire.endTransmission(true);
    delay(50);
    Serial.println("[mpu] MPU6050 ready at 50 Hz, +/-2g");
  }

  pinMode(MOTOR_ENA, OUTPUT);
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  motorStop();

  startBle();

  Serial.printf("[sample] %d Hz, %d samples per batch (~%d s)\n",
                SAMPLE_HZ, BATCH_SAMPLES, BATCH_SECONDS);
  Serial.println("csv,ms,sample_index,x_raw,y_raw,z_raw,x_g,y_g,z_g,magnitude_g");

  motorPhaseStart = millis();
  lastSampleMs    = millis();
  lastStatusMs    = millis();
  startCalmMode();
}

void loop() {
  unsigned long now = millis();
  readSerialCommands();

  // Log status every 5 s so you can see what's happening
  if (now - lastStatusMs >= 5000) {
    lastStatusMs = now;
    Serial.printf("[status] ble=%s pending=%d batchIdx=%d dropped=%lu\n",
                  deviceConnected ? "connected" : "waiting",
                  outPending, batchIdx, (unsigned long)batchesDropped);
    if (!deviceConnected) {
      Serial.println("[hint] start the host:  python ble_host.py");
    }
  }

  // Detect connect / disconnect edges for extra logging
  if (deviceConnected != oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  // Stage 1 + 2: sample at 50 Hz
  if (now - lastSampleMs >= SAMPLE_PERIOD_MS) {
    lastSampleMs += SAMPLE_PERIOD_MS;

    int16_t rx, ry, rz;
    readAccel(rx, ry, rz);

    float xg = rx / ACCEL_COUNTS_PER_G;
    float yg = ry / ACCEL_COUNTS_PER_G;
    float zg = rz / ACCEL_COUNTS_PER_G;
    float mag = sqrt((xg * xg) + (yg * yg) + (zg * zg));
    Serial.printf("csv,%lu,%lu,%d,%d,%d,%.6f,%.6f,%.6f,%.6f\n",
                  now, (unsigned long)sampleNumber, rx, ry, rz, xg, yg, zg, mag);
    sampleNumber++;

    int16_t fx, fy, fz;
    filterSample(rx, ry, rz, fx, fy, fz);

    batchBuf[batchIdx][0] = rx;
    batchBuf[batchIdx][1] = ry;
    batchBuf[batchIdx][2] = rz;
    batchIdx++;

    if (batchIdx >= BATCH_SAMPLES) {
      if (outPending && !deviceConnected) {
        batchesDropped++;
        Serial.printf("[batch] #%u overwritten (no host yet, total dropped=%lu)\n",
                      batchId + 1, (unsigned long)batchesDropped);
      } else if (!deviceConnected) {
        Serial.println("[batch] ready — waiting for BLE host (run ble_host.py)");
      }

      memcpy(outBuf, batchBuf, sizeof(batchBuf));
      batchIdx   = 0;
      batchId++;
      outChunk   = 0;
      outPending = true;
    }
  }

  streamOneChunk();
  updateMotor();
}
