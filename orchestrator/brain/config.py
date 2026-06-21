"""Shared constants. The wristband ESP32 connects to this server as a
WebSocket client:
  wristband -> ws://<laptop-ip>:PORT/sensor   (sends accel JSON)

The plushie has no network leg — it's driven by plain function calls in
plushie_actions.py, called directly from the ML thread.
"""

# Laptop's IP on the hotspot network the wristband joins — find it with
# `ipconfig getifaddr en0` (Mac) or `hostname -I` (Linux/Pi).
HOST = "0.0.0.0"
PORT = 8765
SENSOR_ROUTE = "/sensor"

SAMPLE_HZ = 50          # must match SAMPLE_HZ in the .ino
WINDOW_SIZE = 50        # ~1s of samples at 50Hz
WINDOW_OVERLAP = 0.5    # fraction of window kept between inferences

BREATHING_INHALE_S = 4.0
BREATHING_HOLD_S = 2.0
BREATHING_EXHALE_S = 6.0
