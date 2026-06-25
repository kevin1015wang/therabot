<div align="center">
  <img alt="TheraCat logo" src="https://github.com/user-attachments/assets/9fdde664-2e8a-4728-bfdc-6e741faf23a8" />
</div>

<div align="center">
  <img alt="TheraCat prototype" src="https://github.com/user-attachments/assets/3d8bfb6c-5452-4081-9cfd-a73aecb208ed" width="450">
</div>

# TheraBot / TheraCat

TheraBot is a hackathon prototype for responsive, embodied mental-health support. A wrist-worn ESP32 sensor streams motion data from an MPU6050 accelerometer, a Python host classifies stress-related movement patterns, and a plush companion responds with calming haptic feedback and paced breathing cues.

The goal is not to replace clinical care. It is to explore how a low-cost wearable and soft robotic object can notice agitation early and help a person downshift with a gentle, physical intervention.

## Demo Links

| Asset | Link |
| --- | --- |
| Demo video | [Watch on Vimeo](https://vimeo.com/1203255610?share=copy&fl=sv&fe=ci) |
| Project write-up / Devpost | [TheraCat on Devpost](https://devpost.com/software/theracat) |
| ML notebook | [`TheraBot_Classifier.ipynb`](./TheraBot_Classifier.ipynb) |
| Data capture host | [`ble_host.py`](./ble_host.py) |

## What It Does

- Reads wrist motion from an ESP32 + MPU6050 wearable.
- Streams accelerometer samples to a laptop over BLE or WebSocket, depending on the firmware path being tested.
- Converts raw accelerometer readings into features for calm versus agitated movement.
- Estimates a stress score from recent motion windows.
- Drives calming plushie actions such as relaxing, squeezing, or pulsing.
- Runs an adaptive breathing coach whose exhale length increases as detected stress rises.

## Why We Built It

Stress often shows up physically before a person has the words or attention to describe it. TheraBot explores a feedback loop where motion data becomes an immediate, comforting response:

1. Sense motion from the wrist.
2. Detect patterns associated with agitation or restlessness.
3. Respond through a soft companion instead of another screen.
4. Guide breathing until the user returns toward a calmer state.

This project is designed for hackathon demonstration and rapid prototyping. It keeps the hardware, firmware, data capture, and ML pieces visible so the system can be inspected and improved.

## System Architecture

```text
ESP32 wristband
  MPU6050 accelerometer
  BLE or WebSocket streaming
        |
        v
Laptop host
  ble_host.py for CSV capture
  orchestrator WebSocket server for live samples
        |
        v
ML / signal processing
  sliding motion window
  calm vs agitation features
  stress score from 0.0 to 1.0
        |
        v
Intervention layer
  plushie haptic actions
  adaptive breathing coach
```

## Repository Structure

```text
.
├── README.md
├── TheraBot_Classifier.ipynb
├── ble_host.py
├── calm.csv
├── firmware-real-wristband/
│   └── therabot/therabot.ino
├── wristband/
│   └── wristband.ino
└── orchestrator/
    ├── orchestrator.py
    ├── requirements.txt
    └── brain/
        ├── breathing_coach.py
        ├── config.py
        ├── ml_processor.py
        ├── plushie_actions.py
        ├── server.py
        └── shared_state.py
```

## Hardware

### Wristband

- ESP32 DevKitC or compatible ESP32 board
- MPU6050 accelerometer / gyroscope
- Optional motor driver path for haptic output
- USB cable and Arduino IDE

### Wiring Notes

For the MPU6050:

| MPU6050 | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

The real wristband firmware also documents L293D motor driver wiring:

| L293D | ESP32 |
| --- | --- |
| Enable 1,2 | GPIO25 |
| Input 1 | GPIO33 |
| Input 2 | GPIO32 |

## Firmware Paths

This repo currently contains two firmware paths:

### BLE wristband firmware

Path: [`firmware-real-wristband/therabot/therabot.ino`](./firmware-real-wristband/therabot/therabot.ino)

This sketch samples and filters MPU6050 data on the ESP32, batches accelerometer readings, and exposes them over BLE. It is paired with `ble_host.py`, which subscribes to the BLE characteristic and writes CSV rows for analysis or model training.

### WebSocket wristband firmware

Path: [`wristband/wristband.ino`](./wristband/wristband.ino)

This sketch connects the ESP32 to WiFi and sends JSON accelerometer samples to the Python orchestrator over:

```text
ws://<laptop-ip>:8765/sensor
```

Payload contract:

```json
{"ax": 0.0, "ay": 0.0, "az": 0.0, "t": 0.0}
```

## Running The BLE Data Capture

Install the Python BLE dependency:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install bleak
```

Upload the BLE firmware to the ESP32, then run:

```bash
python ble_host.py > accel_log.csv
```

To label a collection session:

```bash
python ble_host.py --label calm > calm.csv
python ble_host.py --label restless > restless.csv
```

On macOS, allow Bluetooth access for the terminal app you use:

```text
System Settings -> Privacy & Security -> Bluetooth
```

## Running The Live Orchestrator

Install the orchestrator dependencies:

```bash
cd orchestrator
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Start the orchestrator:

```bash
python orchestrator.py
```

The orchestrator starts:

- a WebSocket server for wristband samples
- a sliding-window motion processor
- a plushie action layer
- a text-to-speech breathing coach

Default live route:

```text
ws://0.0.0.0:8765/sensor
```

Update the ESP32 firmware with your laptop's local IP address before uploading the WebSocket sketch.

## ML Model Demo And Notebook

<div align="center">
  <img alt="TheraCat classifier visualization" src="https://github.com/user-attachments/assets/f92592f0-9d48-48d2-9166-e8e89213052c" width="700">
</div>

<div align="center">
  <img alt="Live accelerometer visualization" src="https://github.com/user-attachments/assets/0f37d528-b592-414d-864d-0601fb56f9c3" />
</div>

<p align="center"><em>
Example accelerometer signals from TheraCat show stable motion patterns during a calm state and larger fluctuations during an agitated state.
</em></p>

TheraCat's stress-detection pipeline is backed by a lightweight accelerometer-based classification workflow. The notebook walks through the model development process:

- loading accelerometer data from the MPU6050 sensor
- cleaning and preprocessing raw motion samples
- extracting features from X, Y, and Z accelerometer axes
- comparing calm and agitated movement patterns
- running stress or agitation classification
- visualizing model output

Open the notebook here:

```text
TheraBot_Classifier.ipynb
```

## Current Inference Logic

The live orchestrator currently uses a simple motion-variability stress estimate in [`orchestrator/brain/ml_processor.py`](./orchestrator/brain/ml_processor.py). It computes the magnitude of recent accelerometer samples, measures jitter with population standard deviation, and maps that value into a `0.0` to `1.0` stress score.

The function is intentionally isolated:

```python
def infer_stress(window: list[dict]) -> float:
    ...
```

That makes it straightforward to replace the placeholder logic with the trained notebook model while keeping the server, breathing coach, and haptic response code stable.

## Plushie And Breathing Response

The stress score drives two feedback layers:

- `stress < 0.25`: relax
- `0.25 <= stress < 0.6`: gentle squeeze
- `stress >= 0.6`: stronger pulse

The breathing coach runs on the main thread and gives paced prompts:

```text
Breathe in.
Hold.
Breathe out, slowly.
```

When stress rises, the exhale duration lengthens to encourage slower breathing.

## Development Status

TheraBot is a hackathon prototype. The repo includes working paths for firmware, BLE capture, CSV logging, a notebook-based model workflow, and a Python orchestrator. The next engineering step is to connect the notebook-trained model directly into the live `infer_stress` function, then move toward the product direction outlined below and on the [Devpost project page](https://devpost.com/software/theracat).

## What's Next For TheraCat

TheraCat's next phase focuses on shrinking the system down and exploring two complementary product directions. With the support of Berkeley SkyDeck, we plan to prototype and pressure-test both paths before committing to a primary product line.

### Plan 1: Integrate Everything Into A Smart Band

Consolidate the full experience onto the wrist by integrating the speaker and microphone directly into the band and miniaturizing the electronics.

Rethink the calming mechanism. Instead of using a motor to tighten the band, develop a material or actuator that can genuinely contract and release, delivering breathing and grounding cues more naturally.

Explore turning the band into a smart strap that replaces an ordinary Apple Watch or other smartwatch band, adding a panic-detection and intervention layer to everyday wearables.

### Plan 2: A Personalized Plush Companion, Especially For Kids

Offer a softer, more emotionally engaging form factor designed for younger users and families.

Lean into creativity and personalization to make the companion feel unique to each child.

### Next Steps With SkyDeck

- Validate the technical feasibility of the contracting band.
- Test the market fit of the plush companion.
- Decide which direction leads as the flagship product.

## Engineering Roadmap

- Keep the Devpost and demo video links current.
- Replace placeholder live inference with the trained classifier.
- Add sample datasets for calm, restless, and agitated sessions.
- Add a repeatable calibration flow for each user and wristband position.
- Add hardware photos and a wiring diagram.
- Package the orchestrator with a single setup script.

## Contributors

- [Emily Li](https://github.com/emilyLi2020)
- [Jingyi Zhang](https://github.com/jingy77)
- [Kevin Wang](https://github.com/kevin1015wang)
- [Siddhant Sanghi](https://github.com/Sid01123)

## Acknowledgments

This project was built as a hackathon prototype to explore accessible, embodied calming technology using low-cost sensors, lightweight ML, and soft haptic feedback.
