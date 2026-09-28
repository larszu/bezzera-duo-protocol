#!/usr/bin/env python3
"""Befehle an die ESP32-Bridge (../bridge/duo_bridge) schicken und die
Antwort eine Weile mitlesen. Nur Standardbibliothek.

    python3 bridge.py "p 90"                  # Seite 90, 1 s mitlesen
    python3 bridge.py "?" --lesen 0.5
    python3 bridge.py --lesen 5               # nur mitlesen
    python3 bridge.py "p 1" "p 2" --pause 2   # mehrere Befehle nacheinander

Port: --port, sonst der erste /dev/cu.usbmodem*.
"""

from __future__ import annotations

import argparse
import glob
import os
import select
import sys
import termios
import time


def oeffne(port: str) -> int:
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    a = termios.tcgetattr(fd)
    a[0] = 0
    a[1] = 0
    a[3] &= ~(termios.ICANON | termios.ECHO | termios.ISIG)
    a[4] = a[5] = termios.B115200  # USB-CDC ignoriert die Baudrate
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd


def lies(fd: int, sekunden: float, out=sys.stdout) -> None:
    ende = time.monotonic() + sekunden
    while (rest := ende - time.monotonic()) > 0:
        if select.select([fd], [], [], rest)[0]:
            try:
                out.write(os.read(fd, 4096).decode("latin-1"))
                out.flush()
            except BlockingIOError:
                pass


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("befehle", nargs="*")
    ap.add_argument("--port")
    ap.add_argument("--lesen", type=float, default=1.0, help="Sekunden mitlesen nach jedem Befehl")
    ap.add_argument("--pause", type=float, help="Sekunden zwischen Befehlen (Standard: --lesen)")
    a = ap.parse_args(argv)
    port = a.port or next(iter(sorted(glob.glob("/dev/cu.usbmodem*"))), None)
    if not port:
        print("Kein /dev/cu.usbmodem* gefunden. Steckt der ESP32?", file=sys.stderr)
        return 1
    fd = oeffne(port)
    try:
        if not a.befehle:
            lies(fd, a.lesen)
        for b in a.befehle:
            os.write(fd, (b + "\n").encode())
            lies(fd, a.pause if a.pause is not None else a.lesen)
    finally:
        os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
