<p align="right"><b>Deutsch</b> · <a href="links.en.md">English</a></p>

# Nützliche Links

Gesammelt beim Entschlüsseln der Duo und beim Bau von Brew by Weight. Die
Links waren beim Eintragen (September 2026) erreichbar.

## Bezzera Duo / Matrix

- [Bedienungsanleitung „Matrix Duo“ (Whole Latte Love, PDF)](https://www.wholelattelove.com/cdn/shop/files/Bezzera_DUO_Matrix_Manual.pdf) — IT/EN/FR/DE/ES/ZH, mit Bildschirmfotos; Tastenfeld der DE (3.2.1), Dosier-Programmierung (5.4.5), Ausgabezähler (5.4.4)
- [Bedienungsanleitung Duo MN (kaffee24.de, PDF)](https://www.kaffee24.de/media/e8/95/94/1677585579/W904059%20Bedienungsanleitung%20Bezzera%20Duo%20MN.pdf?ts=1677585579)
- [Clive Coffee: Technician Menu and Reset](https://support.clivecoffee.com/en/articles/16425965-bezzera-duo-de-mn-accessing-the-technician-menu-and-resetting-the-machine) — Weg ins Technikmenü, Werkspasswort
- [1st-line: Matrix/Duo Software-Kompatibilität](https://www.1st-line.com/technical-support/bezzera-technical-support/bezzera-matrix-duo-software-compatibility-changes/)
- [Whole Latte Love: Matrix MN](https://www.wholelattelove.com/products/bezzera-matrix-mn-dual-boiler-espresso-machine) — Händlerangabe „Gicar PID controller“ (bei der Duo DE 2018 nicht zutreffend)

## DWIN-Display und Mini DGUS

- [andrivet/ADVi3pp](https://github.com/andrivet/ADVi3pp) — Mini-DGUS-Befehle und Register (`Marlin/src/advi3pp/core/dgus.h`)
- [Sébastien Andrivet: DWIN Mini DGUS Display Development Guide (non-official)](https://sebastien.andrivet.com/en/posts/dwin-mini-dgus-display-development-guide-non-official/)
- DWIN DGUS Development Guide [v4.0 (2014)](https://cdn.papouch.com/data/user-content/old_eshop/files/DIS_DMT48270T043_3WT/dwin-dgus-dev-guide_v40_2014.pdf), [v4.3 (2015)](https://whiteelectronics.pl/img/cms/DWIN_DGUS_DEV_GUIDE_V43_2015.pdf) — Register, LibOP, Touch-Konfiguration
- [dwinhmi/DWIN_DGUS_HMI](https://github.com/dwinhmi/DWIN_DGUS_HMI) — offizielle Bibliothek für DGUS II
- [DWIN: Namensschema der Typnummern](https://www.dwin-global.com/naming-convention/)

## Bauteile

- [LQ035NC111, Datenblatt (Data Modul, PDF)](https://www.data-modul.com/sites/default/files/products/LQ035NC111_specification_12007119.pdf) — vermutetes LCD-Panel
- [SiS9252](https://www.sis.com/Product_9252_PH.aspx) — Touch-Controller
- [Finder 34.51.7.005.0010 (Farnell)](https://uk.farnell.com/finder/34-51-7-005-0010/relay-spdt-250vac-6a/dp/1169338) — Relais mit 5-V-Spule, 6 A / 250 V

## Andere Espressomaschinen-Protokolle

- [antondlr/gicar-serial](https://github.com/antondlr/gicar-serial) — Gicar-Steuerungen
- [magnusnordlander/lelit-bianca-protocol](https://github.com/magnusnordlander/lelit-bianca-protocol) — Lelit Bianca
- [brewos-io/firmware](https://github.com/brewos-io/firmware) — ersetzt die ganze Steuerung, Duo/Matrix ungetestet
- [Gaggiuino](https://github.com/Zer0-bit/gaggiuino) — Steuerung für die Gaggia Classic mit Mikrocontroller
- [hellgelino/bezzera-bb005-digital-timer](https://github.com/hellgelino/bezzera-bb005-digital-timer), [hcrohland/bezzi-tank](https://github.com/hcrohland/bezzi-tank) — weitere Bezzera-Projekte

## Waagen und Brew by Weight

- [tatemazer/AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE) — BLE-Protokolle für Acaia, Bookoo und Felicita; Vorlage für `waage.h`
- [BooKooCode/OpenSource](https://github.com/BooKooCode/OpenSource) — BOOKOO-Protokoll
- [Decent Scale API](https://decentespresso.com/decentscale_api) und [Programmers guide to the Half Decent Scale](https://decentespresso.com/docs/programmers_guide_to_the_half_decent_scale)
- [decentespresso/openscale](https://github.com/decentespresso/openscale) — offene Hard- und Software der Half Decent Scale (ESP32, Bluetooth, WLAN)
- [Beanconqueror](https://github.com/graphefruit/Beanconqueror) — offene App, unterstützt viele Bluetooth-Waagen; nützlich zum Vergleich der Protokolle

## Grind by Weight

- [Guillaume Besson: Coffee Grinder Smart Scale](https://besson.co/projects/coffee-grinder-smart-scale) — der Ursprung: ESP32-Waage schaltet eine Eureka Mignon per Relais ab
- [jb-xyz/openGBW](https://github.com/jb-xyz/openGBW) — Weiterentwicklung: ESP32, HX711, OLED, Drehgeber, Tastendruck-Modus
- [jaapp/smart-grind-by-weight](https://github.com/jaapp/smart-grind-by-weight) — ESP32-S3 mit Touch-AMOLED, lernt Nachlauf, mahlt in Stößen nach
- [preetpatel/smart-grind-by-weight](https://github.com/preetpatel/smart-grind-by-weight) — Fork mit Diagnose und stabilerem Motor-/BLE-Verhalten

## Werkzeuge

- [sigrok / PulseView](https://sigrok.org/wiki/Downloads) — Logic Analyzer
