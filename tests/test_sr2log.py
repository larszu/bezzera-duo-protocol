"""Tests fuer tools/sr2log.py: PulseView-.sr-Dateien -> Sniffer-Log.

Die .sr-Dateien werden hier synthetisch erzeugt (sigrok-Session-Format 2:
ZIP mit `version`, `metadata` und `logic-1-N`), mit echten UART-Bits.
"""

import io
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import duo_sniff as ds  # noqa: E402
import sr2log as sr  # noqa: E402

RATE = 2_000_000


def uart_bits(frames, baud, rate=RATE, pause_s=0.005):
    """Liste von (Startzeit in s, bytes) -> Pegelfolge 0/1 (Ruhepegel 1)."""
    bl = rate / baud
    ende = max(t for t, _ in frames) + pause_s * 4 + 0.01
    pegel = bytearray([1]) * int(ende * rate)
    for t, daten in frames:
        pos = t * rate
        for b in daten:
            bits = [0] + [(b >> k) & 1 for k in range(8)] + [1]
            for bit in bits:
                a, e = int(pos), int(pos + bl)
                pegel[a:e] = bytes([bit]) * (e - a)
                pos += bl
    return bytes(pegel)


def schreibe_sr(pfad, kanaele, rate=RATE, chunk=1_000_000):
    """kanaele: Liste von Pegelfolgen, Kanal i -> Bit i."""
    n = max(len(k) for k in kanaele)
    roh = bytearray(n)
    for i, k in enumerate(kanaele):
        for j in range(n):
            if (k[j] if j < len(k) else 1):
                roh[j] |= 1 << i
    meta = (
        "[global]\nsigrok version=0.5.2\n\n[device 1]\ncapturefile=logic-1\n"
        f"total probes=8\nsamplerate={rate // 1_000_000} MHz\ntotal analog=0\n"
        + "".join(f"probe{i + 1}=D{i}\n" for i in range(8))
        + "unitsize=1\n"
    )
    with zipfile.ZipFile(pfad, "w") as z:
        z.writestr("version", "2")
        z.writestr("metadata", meta)
        for i in range(0, n, chunk):
            z.writestr(f"logic-1-{i // chunk + 1}", bytes(roh[i : i + chunk]))


class Sr2Log(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.pfad = str(Path(self.tmp.name) / "t.sr")

    def tearDown(self):
        self.tmp.cleanup()

    def test_samplerate_texte(self):
        self.assertEqual(sr._rate("24 MHz"), 24e6)
        self.assertEqual(sr._rate("500 kHz"), 500e3)

    def test_dgus_rahmen_kommen_heil_an(self):
        # Mainboard schreibt VP 0x1000 = 935, Display meldet Taste 2
        a = uart_bits([(0.010, bytes.fromhex("5aa50582100003a7"))], 115200)
        b = uart_bits([(0.030, bytes.fromhex("5aa5068320000100 02".replace(" ", "")))], 115200)
        schreibe_sr(self.pfad, [a, b])  # ueber mehrere logic-1-N-Stuecke

        rate, unit, daten = sr.lese_sr(self.pfad)
        self.assertEqual((rate, unit), (RATE, 1))
        self.assertEqual(sr.schaetze_baud(sr.kanal(daten, unit, 0), rate), 115200)

        out = io.StringIO()
        alt, sys.stdout = sys.stdout, out
        try:
            sr.main([self.pfad])
        finally:
            sys.stdout = alt
        zeilen = out.getvalue().splitlines()
        self.assertEqual(zeilen, ["10 A 5a a5 05 82 10 00 03 a7", "30 B 5a a5 06 83 20 00 01 00 02"])

        # und das Auswerteskript versteht das Ergebnis direkt
        vps = ds.cmd_dgus(list(ds.lese_log(zeilen)), out=io.StringIO())
        self.assertEqual(vps[0x1000], b"\x03\xa7")
        self.assertEqual(vps[0x2000], b"\x00\x02")

    def test_zwei_frames_mit_pause_werden_getrennt(self):
        a = uart_bits([(0.010, b"\x5a\xa5"), (0.020, b"\x5a\xa5")], 115200)
        schreibe_sr(self.pfad, [a])
        rate, unit, daten = sr.lese_sr(self.pfad)
        bits = sr.kanal(daten, unit, 0)
        ev = [(s, "A", b) for s, b in sr.uart(bits, rate, 115200)]
        self.assertEqual(list(sr.log_zeilen(ev, rate, 115200)), ["10 A 5a a5", "20 A 5a a5"])

    def test_invertierte_leitung(self):
        a = uart_bits([(0.010, b"\x81\x00\x00\x5d")], 9600)
        schreibe_sr(self.pfad, [bytes(1 - x for x in a)])
        rate, unit, daten = sr.lese_sr(self.pfad)
        bits = sr.kanal(daten, unit, 0, invert=True)
        self.assertEqual([b for _, b in sr.uart(bits, rate, 9600)], [0x81, 0x00, 0x00, 0x5D])


if __name__ == "__main__":
    unittest.main()
