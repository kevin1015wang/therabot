"""Plushie motor control — plain Python functions called directly from
ml_processor.py on the ML thread. No network hop: the plushie's motors
are driven straight from this process (e.g. via GPIO/serial/whatever the
plushie hardware is wired into on this machine).

TODO(plushie-teammate): implement these three functions with the real
hardware control (GPIO pins, serial command to a motor driver, etc).
Keep the signatures as-is so ml_processor.py doesn't need to change.
"""


def relax() -> None:
    # TODO(plushie-teammate): release/stop the motor.
    print("[plushie] relax")


def squeeze(intensity: float) -> None:
    # TODO(plushie-teammate): drive the motor to a gentle, sustained squeeze.
    # intensity is 0..1.
    print(f"[plushie] squeeze intensity={intensity:.2f}")


def pulse(intensity: float) -> None:
    # TODO(plushie-teammate): a short, stronger squeeze-and-release pulse.
    # intensity is 0..1.
    print(f"[plushie] pulse intensity={intensity:.2f}")
