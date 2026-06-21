"""WebSocket server the wristband ESP32 connects to as a client
(see config.SENSOR_ROUTE):

  wristband ESP32 --(/sensor, sends accel JSON)--> this server --> accel_queue

The plushie is no longer a network peer — its motor control is a plain
Python function call (see plushie_actions.py), called directly from
ml_processor.py on the ML thread.

Runs its own asyncio event loop in a dedicated thread. Incoming accel
samples are pushed onto a plain (thread-safe) queue.Queue so the ML thread
can consume them with a blocking .get().
"""
import asyncio
import json
import queue
import threading

import websockets

from . import config

accel_queue: "queue.Queue[dict]" = queue.Queue()


def _path_of(ws) -> str:
    # websockets <12 exposes ws.path; 13+ moved it to ws.request.path
    return getattr(ws, "path", None) or ws.request.path


async def _handle_sensor(ws):
    print("[server] wristband connected")
    try:
        async for message in ws:
            try:
                sample = json.loads(message)
            except json.JSONDecodeError:
                continue  # drop malformed packet, don't crash the stream
            accel_queue.put(sample)
    finally:
        print("[server] wristband disconnected")


async def _handle_connection(ws):
    path = _path_of(ws)
    if path == config.SENSOR_ROUTE:
        await _handle_sensor(ws)
    else:
        print(f"[server] rejecting connection on unknown path {path!r}")
        await ws.close(code=1008, reason="unknown path")


async def _serve():
    async with websockets.serve(_handle_connection, config.HOST, config.PORT):
        print(f"[server] listening on ws://{config.HOST}:{config.PORT}{config.SENSOR_ROUTE}")
        await asyncio.Future()  # run forever


def run_server_thread() -> threading.Thread:
    """Starts the WS server on a daemon thread and returns it."""

    def _runner():
        asyncio.run(_serve())

    t = threading.Thread(target=_runner, daemon=True, name="ws-server")
    t.start()
    return t
