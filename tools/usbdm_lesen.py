#!/usr/bin/env python3
"""USBDM-Test, nur lesend, fuer die HCS08-MCU (S08PA32) des Mainboards.

Aufruf: python3 tools/usbdm_lesen.py [--special] [--watch SEKUNDEN]
  --special  Reset in den Debug-Modus (BKGD low), danach normaler Reset
  --watch N  N Sekunden lang jede Sekunde verbinden, Aenderungen ausgeben
Braucht pyusb und libusb (brew install libusb).
Protokoll nach podonoghue/usbdm-firmware (Commands.h) und
usbdm-eclipse-makefiles-build (low_level_usb.cpp).


Nur: GET_VER, GET_CAPABILITIES, SET_VDD(OFF), SET_TARGET(HCS08),
GET_BDM_STATUS, CONNECT, GET_SPEED, READ_STATUS_REG, READ_MEM.
Kein Schreiben, kein Loeschen, kein Halt; Reset nur mit --special.
"""
import sys
import usb.core, usb.util, usb.backend.libusb1

import os, time
LIB = "/opt/homebrew/lib/libusb-1.0.dylib"
if os.path.exists(LIB):
    be = usb.backend.libusb1.get_backend(find_library=lambda x: LIB)
else:
    try:  # Windows: pip install libusb-package (bringt libusb-1.0.dll mit)
        import libusb_package
        be = libusb_package.get_libusb1_backend()
    except ImportError:
        be = usb.backend.libusb1.get_backend()
dev = usb.core.find(idVendor=0x16D0, idProduct=0x0567, backend=be)
if dev is None:
    sys.exit("USBDM nicht gefunden")
dev.set_configuration()
usb.util.claim_interface(dev, 0)

ALLOWED = {12, 5, 2, 1, 4, 15, 17, 20, 22, 33}
SPECIAL = '--special' in sys.argv
RC = {0: "OK", 5: "NO_CONNECTION", 15: "UNKNOWN_TARGET", 18: "BDM_EN_FAILED",
      20: "BKGD_TIMEOUT", 21: "SYNC_TIMEOUT", 22: "UNKNOWN_SPEED",
      26: "VDD_NOT_PRESENT", 30: "ACK_TIMEOUT", 54: "HCS_ACCESS_ERROR"}

# GET_VER over EP0 (vendor IN, bRequest=12, wValue=0x0100)
ver = dev.ctrl_transfer(0xC0, 12, 0x0100, 0, 10, timeout=1000)
print("GET_VER raw:", ver.tobytes().hex(" "))
bdm_sw, bdm_hw, icp_sw, icp_hw = ver[1], ver[2], ver[3], ver[4]
print(f"  BDM SW {bdm_sw:#04x}  HW {bdm_hw:#04x}  ICP SW {icp_sw:#04x}  HW {icp_hw:#04x}")
v5 = bdm_sw >= 0x50
print("  Protokoll:", "V5" if v5 else "V4 (JMxx)")

seq = 0
toggle = False

def tx(cmd, payload=b"", rx=64, timeout=1500):
    global seq, toggle
    assert cmd in ALLOWED, f"Kommando {cmd} nicht erlaubt"
    buf = bytearray([0, cmd]) + bytearray(payload)
    buf[0] = len(buf)
    if v5:
        seq = 0 if cmd == 5 else (seq + 1) & 3
        buf[1] |= (seq << 6) & 0xC0
    else:
        if cmd == 5:
            toggle = False
        if toggle:
            buf[1] |= 0x80
    dev.write(0x01, bytes(buf), timeout=timeout)
    if v5 and len(buf) % 64 == 0:
        dev.write(0x01, b"", timeout=timeout)
    while True:
        r = bytearray(dev.read(0x82, 512, timeout=timeout))
        if (r[0] & 0x3F) != 3:  # BUSY -> poll again
            break
    if not v5:
        toggle = not toggle
    rc = r[0] & 0x3F
    return rc, r[1:]

def show(name, rc, data=b""):
    print(f"{name:16s} rc={rc} {RC.get(rc, '?'):18s} {bytes(data).hex(' ')}")

rc, d = tx(5); show("GET_CAPABILITIES", rc, d)
rc, d = tx(2, b"\x00\x00"); show("SET_VDD OFF", rc, d)
rc, d = tx(1, b"\x01"); show("SET_TARGET HCS08", rc, d)
rc, d = tx(4); show("GET_BDM_STATUS", rc, d[:2])
if rc == 0:
    st = (d[0] << 8) | d[1]
    pw = {0: "keine", 1: "extern", 2: "intern", 3: "Fehler"}[(st >> 6) & 3]
    print(f"  Zielspannung: {pw}, RESET-Pin {'high' if st & 4 else 'LOW'}")

if "--watch" in sys.argv:
    dauer = float(sys.argv[sys.argv.index("--watch") + 1])
    t0 = time.time(); last = None
    while time.time() - t0 < dauer:
        rc, d = tx(4); st = (d[0] << 8) | d[1]
        rc2, _ = tx(15, timeout=1000)
        s = f"RESET={'high' if st & 4 else 'LOW'} connect={RC.get(rc2, rc2)}"
        if s != last:
            print(f"{time.time()-t0:5.0f}s {s}", flush=True); last = s
        if rc2 == 0:
            break
        time.sleep(1)
if SPECIAL:
    for mode in (0x04, 0x00):
        rc, d = tx(22, bytes([mode]), timeout=3000); show(f"RESET SPECIAL {mode:#x}", rc)
        if rc == 0:
            break
    rc, d = tx(4); show("GET_BDM_STATUS", rc, d[:2])
rc, d = tx(15); show("CONNECT", rc)
if rc != 0:
    print("Keine Verbindung - Verdrahtung/Spannung pruefen.")
    if SPECIAL: tx(22, bytes([0x05]), timeout=3000)
    sys.exit(1)
rc, d = tx(17); show("GET_SPEED", rc, d[:2])
if rc == 0:
    ticks = (d[0] << 8) | d[1]
    if ticks:
        print(f"  BDM-Takt ~ {60e6*128/ticks/1e6:.2f} MHz")
rc, d = tx(20); show("READ_STATUS_REG", rc, d[:4])
if rc == 0:
    print(f"  BDCSCR = {d[3]:#04x}")

def readmem(addr, n):
    rc, d = tx(33, bytes([1, n]) + addr.to_bytes(4, "big"), timeout=2000)
    return rc, bytes(d[:n])

for name, addr, n in [("SYS SDID", 0x3006, 2),
                      ("FTMRE regs", 0x3020, 16),
                      ("NV 0xFF70", 0xFF70, 16),
                      ("Vektoren", 0xFFF0, 16),
                      ("Flash 0x8000", 0x8000, 16),
                      ("RAM 0x0080", 0x0080, 16)]:
    rc, d = readmem(addr, n)
    show(name, rc, d)

if SPECIAL:
    rc, d = tx(22, bytes([0x05]), timeout=3000); show("RESET NORMAL", rc)
usb.util.release_interface(dev, 0)
