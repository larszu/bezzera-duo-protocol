"""Tests fuer `tools/duo_sniff.py`, das Auswerteskript fuer
Mitschnitte der Leitung Mainboard <-> Display der Bezzera Duo.

Die Gicar-Beispiele stammen aus der Protokolldoku von antondlr/gicar-serial
(Ascaso Baby T, Gicar 3d5): `w005600010164` schaltet dort den Dampfkessel
ein, `w00560001OK9D` ist die Quittung. Ob die Duo dasselbe spricht, ist
offen. Diese Tests pruefen nur, dass das Werkzeug es erkennt, WENN sie es tut.

Lauf: `python3 -m unittest discover -s tests`.
"""

import io
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import duo_sniff as ds  # noqa: E402


def zeile(t, r, daten: bytes) -> str:
    return f"{t} {r} " + " ".join(f"{b:02x}" for b in daten)


class GicarFormat(unittest.TestCase):
    def test_bekannte_beispiele_haben_gueltige_pruefsumme(self):
        for text in ("w005600010164", "w00560001OK9D"):
            g = ds.parse_gicar(text)
            self.assertIsNotNone(g, text)
            self.assertTrue(g.pruefsumme_ok, text)
            self.assertEqual(g.offset, 0x56)
            self.assertEqual(g.laenge, 1)

    def test_pruefsumme_anhaengen(self):
        self.assertEqual(ds.gicar_checksumme("w0056000101"), "w005600010164")

    def test_falsche_pruefsumme_wird_erkannt(self):
        self.assertFalse(ds.parse_gicar("w005600010165").pruefsumme_ok)

    def test_binaerdaten_sind_kein_gicar(self):
        self.assertIsNone(ds.parse_gicar("\x80\x11\x17\x00\x28"))


class GicarSpeicherabbild(unittest.TestCase):
    def test_aenderung_zwischen_zwei_lesungen_wird_gemeldet(self):
        a = ds.gicar_checksumme("r00350002" + "A703")  # 0x03A7 = 935 -> 93.5 Grad
        b = ds.gicar_checksumme("r00350002" + "AE03")  # 0x03AE = 942 -> 94.2 Grad
        log = "\n".join(
            [zeile(100, "A", a.encode()), "# am Display 93.5 -> 94.2", zeile(900, "A", b.encode())]
        )
        out = io.StringIO()
        speicher = ds.cmd_gicar(list(ds.lese_log(log.splitlines())), out=out)
        self.assertEqual(speicher[0x35], 0xAE)
        self.assertEqual(speicher[0x36], 0x03)
        text = out.getvalue()
        self.assertIn("0x0035 (  53): a7 -> ae", text)
        self.assertIn("-- am Display 93.5 -> 94.2", text)
        self.assertNotIn("0x0036", text)  # das hohe Byte blieb 03

    def test_schreibbefehl_setzt_speicher(self):
        log = zeile(5, "B", b"w005600010164")
        out = io.StringIO()
        speicher = ds.cmd_gicar(list(ds.lese_log([log])), out=out)
        self.assertEqual(speicher[0x56], 0x01)
        self.assertIn("SCHREIBT @0x0056", out.getvalue())


class Pruefsummen(unittest.TestCase):
    def test_lelit_bianca_mod128_wird_gefunden(self):
        # Paket Display -> Steuerplatine aus magnusnordlander/lelit-bianca-protocol
        treffer = dict(ds.pruefsummen_treffer([ds.Frame(0, "A", bytes.fromhex("8011170028"))]))
        self.assertEqual(treffer["sum7 (mod 128)"], 1.0)

    def test_gicar_ascii_wird_als_ascii_hex_sum_erkannt(self):
        fs = [ds.Frame(i, "A", ds.gicar_checksumme(f"r{i:04X}0001{i:02X}").encode()) for i in range(10)]
        beste, quote = ds.pruefsummen_treffer(fs)[0]
        self.assertEqual(beste, "ascii-hex sum8 (Gicar 3d5)")
        self.assertEqual(quote, 1.0)

    def test_crc16_modbus_referenzwert(self):
        # Standard-Pruefwert fuer CRC-16/MODBUS ueber "123456789"
        self.assertEqual(ds._crc16_modbus(b"123456789"), 0x4B37)


class LogEinlesen(unittest.TestCase):
    def test_bootmeldungen_werden_uebersprungen(self):
        zeilen = ["ets Jun  8 2016 00:22:57", "", "12 A 80 11", "# Markierung", "13 b 81"]
        e = list(ds.lese_log(zeilen))
        self.assertEqual(len(e), 3)
        self.assertEqual(e[0].daten, b"\x80\x11")
        self.assertEqual(e[1], "Markierung")
        self.assertEqual(e[2].richtung, "B")


class Diff(unittest.TestCase):
    def test_nur_die_veraenderliche_position_erscheint(self):
        log = [zeile(t, "A", bytes([0x81, 0x00, v, 0x7F])) for t, v in ((0, 0x5D), (10, 0x5D), (20, 0x5C))]
        out = io.StringIO()
        verlauf = ds.cmd_diff(list(ds.lese_log(log)), "A", None, None, out=out)
        self.assertEqual(verlauf, {2: [(20, 0x5C)]})
        self.assertIn("[0]=81", out.getvalue())


class Stats(unittest.TestCase):
    def test_ascii_protokoll_wird_benannt(self):
        fs = [ds.Frame(i * 100, "A", ds.gicar_checksumme("r000500D7").encode()) for i in range(5)]
        out = io.StringIO()
        ds.cmd_stats(fs, out=out)
        self.assertIn("vermutlich ASCII-Protokoll", out.getvalue())
        self.assertIn("ascii-hex sum8", out.getvalue())


class Dgus(unittest.TestCase):
    """Rahmen im Format der DWIN-M-Serie ("Mini DGUS"), wie sie ADVi3++ fuer
    das DMT48270M043 im Wanhao i3 Plus sendet und empfaengt."""

    def rahmen(self, zeilen):
        return list(ds.dgus_rahmen(ds.nur_frames(ds.lese_log(zeilen))))

    def test_seitenwechsel_ueber_register_03(self):
        r = self.rahmen(["0 A 5a a5 04 80 03 00 05"])
        self.assertEqual(len(r), 1)
        self.assertEqual(ds.dgus_beschreibung(r[0])[0], "Register schreiben PIC_ID (Seite) = Seite 5")

    def test_vp_schreiben_und_tastendruck(self):
        r = self.rahmen([
            "10 A 5a a5 05 82 10 00 03 a7",        # Mainboard: VP 0x1000 = 935
            "20 B 5a a5 06 83 20 00 01 00 02",     # Display: Taste an VP 0x2000, Code 2
        ])
        text0, vp0, d0 = ds.dgus_beschreibung(r[0])
        self.assertEqual((vp0, d0), (0x1000, b"\x03\xa7"))
        self.assertIn("03A7(935)", text0)
        text1, vp1, _ = ds.dgus_beschreibung(r[1])
        self.assertEqual(vp1, 0x2000)
        self.assertIn("meldet 0002(2)", text1)

    def test_lese_anfrage_ist_keine_variable(self):
        r = self.rahmen(["0 A 5a a5 04 83 10 00 02"])
        text, vp, _ = ds.dgus_beschreibung(r[0])
        self.assertIsNone(vp)
        self.assertEqual(text, "VP lesen 0x1000, 2 Wort")

    def test_rahmen_ueber_zwei_snifferzeilen_und_mehrere_in_einer(self):
        r = self.rahmen([
            "0 A 5a a5 05 82 10",
            "1 A 00 00 01 5a a5 04 80 03 00 02 ff",   # Rest, dann ganzer Rahmen, dann Muell
        ])
        self.assertEqual([x.cmd for x in r], [0x82, 0x80])
        self.assertEqual(r[0].nutz, b"\x10\x00\x00\x01")

    def test_crc_wird_erkannt_und_abgeschnitten(self):
        koerper = bytes.fromhex("82100003a7")
        crc = ds._crc16_modbus(koerper).to_bytes(2, "little")
        roh = b"\x5a\xa5" + bytes([len(koerper) + 2]) + koerper + crc
        r = self.rahmen(["0 A " + roh.hex(" ")])
        self.assertTrue(r[0].crc)
        self.assertEqual(r[0].nutz, b"\x10\x00\x03\xa7")

    def test_dgus2_quittung(self):
        r = self.rahmen(["0 B 5a a5 03 82 4f 4b"])
        self.assertEqual(ds.dgus_beschreibung(r[0])[0], "Quittung OK")

    def test_aenderungen_mit_markierung(self):
        log = [
            "0 A 5a a5 05 82 10 00 03 a7",
            "100 A 5a a5 05 82 10 00 03 a7",   # unveraendert: bei --changes still
            "# bruehtemp 93.5 -> 94.0",
            "200 A 5a a5 05 82 10 00 03 ac",
        ]
        out = io.StringIO()
        vps = ds.cmd_dgus(list(ds.lese_log(log)), nur_aenderungen=True, out=out)
        zeilen = out.getvalue().splitlines()
        self.assertEqual(vps[0x1000], b"\x03\xac")
        self.assertEqual(sum("VP schreiben 0x1000" in z for z in zeilen), 2)
        i = zeilen.index("-- bruehtemp 93.5 -> 94.0")
        self.assertIn("03AC(940)", zeilen[i + 1])

    def test_markierung_bleibt_hinter_frames_mit_gleicher_zeit(self):
        # Zwei Rahmen in EINER Snifferzeile, danach die Markierung. Frueher
        # wurde nach Zeit einsortiert und die Markierung landete davor.
        log = ["150 A 5a a5 05 82 10 00 03 a2 5a a5 05 82 10 02 04 e2", "# danach", "151 A 5a a5 05 82 10 00 03 a7"]
        out = io.StringIO()
        ds.cmd_dgus(list(ds.lese_log(log)), out=out)
        zeilen = out.getvalue().splitlines()
        i = zeilen.index("-- danach")
        self.assertIn("0x1002", zeilen[i - 1])
        self.assertIn("03A7", zeilen[i + 1])

    def test_eigener_kopf_wird_erkannt(self):
        # Bezzera Duo: Kopf C6 A5 statt 5A A5 (R3 in CONFIG.txt umgestellt).
        log = ["0 B c6 a5 04 80 03 00 5a c6 a5 04 83 00 00 01", "3 A c6 a5 06 83 00 00 01 00 01"]
        out = io.StringIO()
        vps = ds.cmd_dgus(list(ds.lese_log(log)), out=out)
        self.assertIn("Rahmenkopf C6 A5", out.getvalue())
        self.assertIn("Seite 90", out.getvalue())
        self.assertEqual(vps[0x0000], b"\x00\x01")

    def test_stats_verweist_auf_dgus(self):
        out = io.StringIO()
        ds.cmd_stats([ds.Frame(0, "A", bytes.fromhex("5aa50480030005"))], out=out)
        self.assertIn("`dgus` benutzen", out.getvalue())


if __name__ == "__main__":
    unittest.main()



class RocketR58(unittest.TestCase):
    """Rocket R58 (Kaffee-Netz, HanDeKe): DWIN mit Kopf 5A A5 und CRC16 (Modbus)."""

    def test_rahmen_mit_crc(self):
        frames = [ds.Frame(0, "RX", bytes([90, 165, 6, 131, 16, 0, 3, 104, 100]))]
        self.assertEqual(ds.dgus_kopf_erkennen(frames), b"\x5a\xa5")
        r = list(ds.dgus_rahmen(frames))
        self.assertEqual(len(r), 1)
        self.assertTrue(r[0].crc)
        self.assertEqual(ds.dgus_beschreibung(r[0])[0], "VP lesen 0x1000, 3 Wort")
