"""Entry point: python orchestrator.py

Starts the WS server (bracelet connects to it) and the ML thread as
daemon threads, then runs the breathing coach on the main thread so
Ctrl+C cleanly tears everything down.
"""
from brain.breathing_coach import run_breathing_loop
from brain.ml_processor import run_ml_thread
from brain.server import run_server_thread


def main() -> None:
    run_server_thread()
    run_ml_thread()
    run_breathing_loop()  # blocks on the main thread


if __name__ == "__main__":
    main()
