"""Runs on the main thread. Cues paced breathing out loud; the exhale
lengthens as the detected stress level rises, so the pacing responds to
the same feedback loop driving the bracelet's motor."""
import time

import pyttsx3

from . import config
from .shared_state import stress_level


def _say(engine: "pyttsx3.Engine", text: str) -> None:
    print(f"[breathing] {text}")
    engine.say(text)
    engine.runAndWait()


def run_breathing_loop() -> None:
    engine = pyttsx3.init()
    # TODO(audio-teammate): swap pyttsx3's TTS voice for real recorded/produced
    # audio cues if you want a specific voice/tone, e.g. via playsound() on
    # pre-rendered clips instead of _say().
    while True:
        stress = stress_level.get()
        exhale_s = config.BREATHING_EXHALE_S + stress * 3.0  # longer exhale when stressed

        _say(engine, "Breathe in.")
        time.sleep(config.BREATHING_INHALE_S)

        _say(engine, "Hold.")
        time.sleep(config.BREATHING_HOLD_S)

        _say(engine, "Breathe out, slowly.")
        time.sleep(exhale_s)
