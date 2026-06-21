import threading


class StressLevel:
    """Thread-safe 0..1 stress estimate, written by the ML thread, read by
    the breathing coach (and anything else that wants the current level)."""

    def __init__(self):
        self._lock = threading.Lock()
        self._value = 0.0

    def set(self, value: float) -> None:
        with self._lock:
            self._value = max(0.0, min(1.0, value))

    def get(self) -> float:
        with self._lock:
            return self._value


stress_level = StressLevel()
