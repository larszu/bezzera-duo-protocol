#!/usr/bin/env python3
"""Simulierte Bridge fuer `web_lokal.py --demo`: Maschine, Verlauf und Waage
ohne ESP32, damit sich die Weboberflaeche am Rechner pruefen laesst.

Die Maschine heizt auf, Wort 1 in VP 0x0050 spielt den Pumpendruck (nur in
der Simulation). Alle 45 s laeuft ein Bezug auf der Waage; der Stopp kommt wie
in der Firmware bei Gewicht + Vorlauf >= Ziel.
"""

from __future__ import annotations

import json
import math
import threading
import time
import urllib.parse


class Demo:
    def __init__(self):
        self.start = time.time()
        self.an = True
        self.seite = 101
        self.ereignisse: list[str] = []
        self.cfg = {"mqtt_uri": "", "mqtt_user": "", "mqtt_pass_gesetzt": False, "ha_prefix": "homeassistant",
                    "waage_art": 1, "waage_url": "", "waage_topic": "", "waage_ble": "", "ziel": 36.0,
                    "vorlauf": 2.0, "lernen": True, "stopp_pin": -1, "stopp_high": True, "stopp_halten": 15,
                    "druck_p_wort": -1, "druck_p_teil": 10, "druck_k_wort": -1, "druck_k_teil": 10}
        self.verlauf: list[tuple[float, list[int]]] = []
        self.bezug: list[list] = []
        self.bezug_nr = 0
        self.liste: list[dict] = []
        self.laeuft = False
        self.g = 0.0
        self.fluss = 0.0
        self.stopp = False
        for i in range(3600, 0, -1):  # eine Stunde Vorgeschichte
            self.verlauf.append((time.time() - i, self._worte(3600 - i, i < 2400 and i % 600 < 28)))
        threading.Thread(target=self._takt, daemon=True).start()

    def _worte(self, t: float, pumpe: bool) -> list[int]:
        k = round(93 - 70 * math.exp(-t / 600) + math.sin(t / 40)) if self.an else 25
        d = round(122 - 95 * math.exp(-t / 700) + 0.8 * math.sin(t / 55)) if self.an else 25
        p = round(90 + 5 * math.sin(t)) if pumpe and self.an else 0
        return [0, p, 1, k, d, 0, 0, 3, 0]

    def _takt(self):
        t_bezug = None
        while True:
            t = time.time() - self.start + 3600
            phase = (time.time() - self.start) % 45
            pumpe = self.an and phase < 30
            w = self._worte(t, pumpe)
            if not self.verlauf or time.time() - self.verlauf[-1][0] >= 1:
                self.verlauf.append((time.time(), w))
                self.verlauf = self.verlauf[-86400:]
            # Bezug: 6 s Vorbruehen, dann ~1,8 g/s, Stopp bei Ziel - Vorlauf, Nachtropfen
            if pumpe and phase > 6 and not self.laeuft and t_bezug is None:
                self.laeuft, self.g, self.stopp, self.bezug, t_bezug = True, 0.0, False, [], time.time()
                self.bezug_nr += 1
                self.ereignisse.append(f"{int(time.time() * 1000)} - Bezug erkannt")
            if self.laeuft:
                dt = time.time() - t_bezug
                if not self.stopp:
                    self.fluss = 1.8 + 0.3 * math.sin(dt)
                    if self.g + self.cfg["vorlauf"] >= self.cfg["ziel"]:
                        self.stopp = True
                        t_stopp = time.time()
                else:
                    self.fluss = max(0.0, 1.2 * math.exp(-(time.time() - t_stopp) * 1.5))
                self.g += self.fluss * 0.1
                self.bezug.append([round(dt, 1), round(self.g, 1), round(self.fluss, 2), round(w[1] / 10, 1), w[3]])
                if self.stopp and time.time() - t_stopp > 4:
                    self.laeuft = False
                    fehler = self.g - self.cfg["ziel"]
                    if self.cfg["lernen"]:
                        self.cfg["vorlauf"] = round(min(8, max(0, self.cfg["vorlauf"] + 0.5 * fehler)), 1)
                    self.liste.insert(0, {"ende": time.time(), "g": round(self.g, 1), "ziel": self.cfg["ziel"],
                                          "dauer": round(dt, 1), "gestoppt": True})
                    self.liste = self.liste[:10]
            elif phase < 1:
                t_bezug = None
            if not self.laeuft:
                self.fluss = 0.0
            time.sleep(0.1)

    # ── Schnittstelle wie Usb in web_lokal.py ──────────────────────────────
    def senden(self, zeile: str):
        teile = zeile.split()
        if teile and teile[0] == "p" and len(teile) > 1:
            self.seite = int(teile[1], 0)
        self.ereignisse.append(f"{int(time.time() * 1000)} - Web: {zeile}")

    def status(self, seit: str) -> bytes:
        w = self.verlauf[-1][1]
        nr = len(self.ereignisse)
        ab = max(int(seit or 0), nr - 40)
        j = {"ms": int((time.time() - self.start) * 1000), "seite": self.seite,
             "rtc": time.strftime("%Y-%m-%d %H:%M:%S"),
             "display": {"rahmen": 1000, "still_ms": 50}, "mainboard": {"rahmen": 1000, "still_ms": 50},
             "vps": [{"vp": 0x50, "alter_ms": 100, "w": w}, {"vp": 0, "alter_ms": 100, "w": [1 if self.an else 0]}],
             "overrides": [], "ereignis_nr": nr, "ereignisse": self.ereignisse[ab:], "emulation": 0}
        return json.dumps(j).encode()

    def zusatz(self, pfad: str, rumpf: str = "") -> bytes:
        u = urllib.parse.urlparse(pfad)
        q = dict(urllib.parse.parse_qsl(u.query))
        c = self.cfg
        if u.path == "/api/verlauf":
            sek, mx = int(q.get("sek", 3600)), int(q.get("max", 720))
            jetzt = time.time()
            p = [(t, w) for t, w in self.verlauf if jetzt - t <= sek]
            schritt = max(1, math.ceil(len(p) / mx))
            return json.dumps({"jetzt": 0, "groesse": 86400,
                               "p": [[int(jetzt - t), self.seite] + w for t, w in p[::schritt]]}).encode()
        if u.path == "/api/bezug":
            return json.dumps({"nr": self.bezug_nr, "p": self.bezug}).encode()
        if u.path == "/api/aktion":
            a = rumpf.strip()
            if a in ("an", "aus"):
                self.an = a == "an"
                self.seite = 101 if self.an else 100
            elif a.startswith("ziel "):
                c["ziel"] = float(a[5:])
            self.ereignisse.append(f"{int(time.time() * 1000)} - Web: {a}")
            return b"ok"
        if u.path == "/api/einstellungen":
            for k, v in urllib.parse.parse_qsl(rumpf):
                if k == "mqtt_pass":
                    c["mqtt_pass_gesetzt"] = v != "-"
                elif k in ("lernen", "stopp_high"):
                    c[k] = v == "1"
                elif k in c:
                    c[k] = type(c[k])(float(v)) if isinstance(c[k], (int, float)) and not isinstance(c[k], bool) else v
            return b"ok"
        w = self.verlauf[-1][1]
        druck = w[c["druck_p_wort"]] / c["druck_p_teil"] if c["druck_p_wort"] >= 0 else None
        letzter = self.liste[0] if self.liste else None
        werte = {"kaffee": w[3], "service": w[4], "druck_pumpe": druck, "druck_kessel": None,
                 "status": "an" if self.an else "Standby", "alarm": "OFF", "alarm_text": "",
                 "an": "ON" if self.an else "OFF", "gewicht": round(self.g, 1), "durchfluss": round(self.fluss, 1),
                 "bezug": "ON" if self.laeuft else "OFF", "letzter_g": letzter and letzter["g"],
                 "letzter_s": letzter and letzter["dauer"], "bezuege": 1200 + len(self.liste), "ziel": c["ziel"],
                 "waage": "Bluetooth: BOOKOO_SC (Simulation)"}
        j = {"maschine": {"status": werte["status"], "an": self.an}, "werte": werte,
             "waage": {"status": werte["waage"], "alter_ms": 100},
             "bezug": {"laeuft": self.laeuft, "nr": self.bezug_nr,
                       "t": self.bezug[-1][0] if self.laeuft and self.bezug else None, "g": round(self.g, 1),
                       "stopp_gesendet": self.stopp, "stopp_aktiv": False,
                       "liste": [{"alter_s": int(time.time() - x["ende"]), **{k: x[k] for k in ("g", "ziel", "dauer", "gestoppt")}}
                                 for x in self.liste]},
             "mqtt": "verbunden" if c["mqtt_uri"] else "aus", "mqtt_basis": "duo/abc123", "psram": True, "cfg": c}
        return json.dumps(j).encode()
