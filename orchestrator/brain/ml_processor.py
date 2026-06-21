"""Consumes accel samples, runs sliding-window inference, drives the
plushie. infer_stress() is the placeholder seam — swap it for a real
model later without touching anything else in this file."""
import statistics
import threading

from . import config, plushie_actions
from .server import accel_queue
from .shared_state import stress_level


def infer_stress(window: list[dict]) -> float:
    """Placeholder: uses accel variability (jerk/fidgeting) as a stand-in
    for a trained model. Returns a 0..1 stress estimate.

    TODO(ml-teammate): replace this body with the real model. Keep the
    signature (list[dict] of {"ax","ay","az","t"} samples -> float 0..1)
    so nothing else in the pipeline needs to change.
    """
    mags = [(s["ax"] ** 2 + s["ay"] ** 2 + s["az"] ** 2) ** 0.5 for s in window]
    if len(mags) < 2:
        return 0.0
    jitter = statistics.pstdev(mags)
    return max(0.0, min(1.0, jitter / 4.0))  # 4.0 m/s^2 stdev ~= max stress, tune empirically


def act_on(stress: float) -> None:
    # TODO(plushie-teammate): tune these thresholds/intensities once you've
    # felt the motor response on hardware.
    if stress < 0.25:
        plushie_actions.relax()
    elif stress < 0.6:
        plushie_actions.squeeze(0.4)
    else:
        plushie_actions.pulse(0.8)


def _run():
    window: list[dict] = []
    keep = int(config.WINDOW_SIZE * config.WINDOW_OVERLAP)

    while True:
        sample = accel_queue.get()
        window.append(sample)

        if len(window) >= config.WINDOW_SIZE:
            stress = infer_stress(window)
            stress_level.set(stress)
            act_on(stress)
            window = window[-keep:] if keep else []


def run_ml_thread() -> threading.Thread:
    t = threading.Thread(target=_run, daemon=True, name="ml-processor")
    t.start()
    return t
