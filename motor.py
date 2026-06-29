"""
motor_control.py

Simple serial motor controller.

Exports:
    turn_on_motor(ser)   -> writes '1' to the serial port
    turn_off_motor(ser)  -> writes '0' to the serial port
    select_usb_port()    -> lists USB serial ports and asks the user to pick one

Run standalone to pick a port and then toggle the motor on/off
(1 second each) in a loop.

Requires: pyserial  ->  pip install pyserial
"""

import sys
import time

import serial
import serial.tools.list_ports


def turn_on_motor(ser):
    """Send the '1' character to turn the motor on."""
    ser.write(b"1")
    ser.flush()


def turn_off_motor(ser):
    """Send the '0' character to turn the motor off."""
    ser.write(b"0")
    ser.flush()


def select_usb_port():
    """List all available serial ports and let the user choose one.

    Returns the device string of the selected port (e.g. '/dev/ttyUSB0'
    or 'COM3'), or None if no ports are available.
    """
    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("No serial ports found.")
        return None

    print("Available serial ports:")
    for index, port in enumerate(ports):
        # port.description gives a human-readable name when available
        print(f"  [{index}] {port.device} - {port.description}")

    while True:
        choice = input("Select a port by number: ").strip()
        if not choice.isdigit():
            print("Please enter a valid number.")
            continue

        choice = int(choice)
        if 0 <= choice < len(ports):
            return ports[choice].device

        print(f"Please enter a number between 0 and {len(ports) - 1}.")


def main():
    port = select_usb_port()
    if port is None:
        sys.exit(1)

    # Open the chosen port. 9600 baud is a common default; change if needed.
    with serial.Serial(port, baudrate=9600, timeout=1) as ser:
        # Give the connection a moment to settle (many boards reset on connect).
        time.sleep(2)

        print(f"Connected to {port}. Toggling motor on/off. Press Ctrl+C to stop.")
        try:
            while True:
                turn_on_motor(ser)
                print("Motor ON")
                time.sleep(1)

                turn_off_motor(ser)
                print("Motor OFF")
                time.sleep(1)
        except KeyboardInterrupt:
            # Make sure the motor is off before we exit.
            turn_off_motor(ser)
            print("\nStopped. Motor turned off.")


if __name__ == "__main__":
    main()