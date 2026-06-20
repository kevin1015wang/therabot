#!/usr/bin/env python3
"""
TheraBot BLE host — connects to the ESP32 and writes accelerometer CSV rows.

Usage:
  pip install bleak
  python ble_host.py > accel_log.csv
  python ble_host.py --label calm > calm.csv

On macOS: System Settings → Privacy & Security → Bluetooth → allow Terminal/Cursor.
"""

import argparse
import asyncio
import csv
import math
import os
from pathlib import Path
import struct
import sys
import time
from datetime import datetime, timezone

if sys.prefix == sys.base_prefix:
    venv_python = Path(__file__).resolve().parent / ".venv" / "bin" / "python"
    if venv_python.exists():
        os.execv(str(venv_python), [str(venv_python), __file__, *sys.argv[1:]])

from bleak import BleakClient, BleakScanner

DEVICE_NAME = "TheraBot"
DATA_CHAR = "6e646973-7468-6572-6162-6f7400000002"
CMD_CHAR = "6e646973-7468-6572-6162-6f7400000003"
COUNTS_PER_G = 16384.0
SAMPLE_HZ = 50.0
CHUNK_SAMPLES = 20
CSV_HEADER = [
    "timestamp",
    "elapsed_s",
    "batch_id",
    "chunk_id",
    "chunk_count",
    "sample_index",
    "x_raw",
    "y_raw",
    "z_raw",
    "x_g",
    "y_g",
    "z_g",
    "magnitude",
    "label",
]

chunks_received = 0
batches_complete = 0
last_batch_id = None
start_time = time.time()
csv_writer = None
label = ""


def on_notify(_handle: int, data: bytearray) -> None:
    global chunks_received, batches_complete, last_batch_id

    if len(data) < 4:
        print(f"[warn] short packet ({len(data)} bytes)", file=sys.stderr)
        return

    batch_id, chunk_idx, chunk_count, n_samples = data[0], data[1], data[2], data[3]
    payload = data[4:]
    expected = n_samples * 6

    if len(payload) < expected:
        print(
            f"[warn] batch={batch_id} chunk={chunk_idx} expected {expected} payload bytes, got {len(payload)}",
            file=sys.stderr,
        )
        return

    chunks_received += 1

    receive_time = time.time()
    chunk_id = chunk_idx + 1

    for i in range(n_samples):
        x_raw, y_raw, z_raw = struct.unpack_from("<hhh", payload, i * 6)
        sample_index = chunk_idx * CHUNK_SAMPLES + i
        sample_elapsed = receive_time - start_time + (i / SAMPLE_HZ)
        timestamp = datetime.fromtimestamp(receive_time + (i / SAMPLE_HZ), timezone.utc).isoformat()

        x_g = x_raw / COUNTS_PER_G
        y_g = y_raw / COUNTS_PER_G
        z_g = z_raw / COUNTS_PER_G
        magnitude = math.sqrt((x_g * x_g) + (y_g * y_g) + (z_g * z_g))

        csv_writer.writerow(
            [
                timestamp,
                f"{sample_elapsed:.3f}",
                batch_id,
                chunk_id,
                chunk_count,
                sample_index,
                x_raw,
                y_raw,
                z_raw,
                f"{x_g:.6f}",
                f"{y_g:.6f}",
                f"{z_g:.6f}",
                f"{magnitude:.6f}",
                label,
            ]
        )
    sys.stdout.flush()

    if chunk_idx + 1 == chunk_count:
        batches_complete += 1
        last_batch_id = batch_id
        print(f"[ok] batch {batch_id} complete (total batches={batches_complete})", file=sys.stderr)


async def find_device(timeout: float = 15.0):
    print(f"Scanning for '{DEVICE_NAME}' ({timeout:.0f}s)...", file=sys.stderr)
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: (d.name or "") == DEVICE_NAME or DEVICE_NAME in (ad.local_name or ""),
        timeout=timeout,
    )
    return device


async def main() -> None:
    global csv_writer, label, start_time

    parser = argparse.ArgumentParser(description="Stream TheraBot BLE accelerometer data as CSV.")
    parser.add_argument("--label", default="", help="Label to write into each CSV row, e.g. calm or restless.")
    parser.add_argument("--no-header", action="store_true", help="Do not write the CSV header row.")
    args = parser.parse_args()

    label = args.label
    start_time = time.time()
    csv_writer = csv.writer(sys.stdout)
    if not args.no_header:
        csv_writer.writerow(CSV_HEADER)
        sys.stdout.flush()

    device = await find_device()
    if device is None:
        print(f"Could not find '{DEVICE_NAME}'.", file=sys.stderr)
        print("- Is the ESP32 powered and Serial Monitor showing 'advertising'?", file=sys.stderr)
        print("- Is Bluetooth enabled on this Mac?", file=sys.stderr)
        sys.exit(1)

    print(f"Found {device.name or DEVICE_NAME} at {device.address}, connecting...", file=sys.stderr)

    async with BleakClient(device, timeout=20.0) as client:
        if not client.is_connected:
            print("Connection failed.", file=sys.stderr)
            sys.exit(1)

        print("Connected. Subscribing to data notifications...", file=sys.stderr)
        await client.start_notify(DATA_CHAR, on_notify)
        print("Listening for batches. Press Ctrl+C to stop.", file=sys.stderr)

        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print(f"\nDone. batches={batches_complete}, chunks={chunks_received}", file=sys.stderr)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
