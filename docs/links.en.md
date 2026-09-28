<p align="right"><a href="links.md">Deutsch</a> · <b>English</b></p>

# Useful links

Collected while decoding the Duo and building brew by weight. The links were
reachable when added (September 2026).

## Bezzera Duo / Matrix

- [“Matrix Duo” user manual (Whole Latte Love, PDF)](https://www.wholelattelove.com/cdn/shop/files/Bezzera_DUO_Matrix_Manual.pdf) — IT/EN/FR/DE/ES/ZH with screenshots; DE keypad (3.2.1), dose programming (5.4.5), dispensing counter (5.4.4)
- [Duo MN user manual (kaffee24.de, PDF, German)](https://www.kaffee24.de/media/e8/95/94/1677585579/W904059%20Bedienungsanleitung%20Bezzera%20Duo%20MN.pdf?ts=1677585579)
- [Clive Coffee: Technician Menu and Reset](https://support.clivecoffee.com/en/articles/16425965-bezzera-duo-de-mn-accessing-the-technician-menu-and-resetting-the-machine) — how to enter the technician menu, factory password
- [1st-line: Matrix/Duo software compatibility](https://www.1st-line.com/technical-support/bezzera-technical-support/bezzera-matrix-duo-software-compatibility-changes/)
- [Whole Latte Love: Matrix MN](https://www.wholelattelove.com/products/bezzera-matrix-mn-dual-boiler-espresso-machine) — dealer states “Gicar PID controller” (not true for the 2018 Duo DE)

## DWIN display and Mini DGUS

- [andrivet/ADVi3pp](https://github.com/andrivet/ADVi3pp) — Mini DGUS commands and registers (`Marlin/src/advi3pp/core/dgus.h`)
- [Sébastien Andrivet: DWIN Mini DGUS Display Development Guide (non-official)](https://sebastien.andrivet.com/en/posts/dwin-mini-dgus-display-development-guide-non-official/)
- DWIN DGUS Development Guide [v4.0 (2014)](https://cdn.papouch.com/data/user-content/old_eshop/files/DIS_DMT48270T043_3WT/dwin-dgus-dev-guide_v40_2014.pdf), [v4.3 (2015)](https://whiteelectronics.pl/img/cms/DWIN_DGUS_DEV_GUIDE_V43_2015.pdf) — registers, LibOP, touch configuration
- [dwinhmi/DWIN_DGUS_HMI](https://github.com/dwinhmi/DWIN_DGUS_HMI) — official DGUS II library
- [DWIN: part number naming convention](https://www.dwin-global.com/naming-convention/)

## Parts

- [LQ035NC111 datasheet (Data Modul, PDF)](https://www.data-modul.com/sites/default/files/products/LQ035NC111_specification_12007119.pdf) — assumed LCD panel
- [SiS9252](https://www.sis.com/Product_9252_PH.aspx) — touch controller
- [Finder 34.51.7.005.0010 (Farnell)](https://uk.farnell.com/finder/34-51-7-005-0010/relay-spdt-250vac-6a/dp/1169338) — relay with 5 V coil, 6 A / 250 V

## Other espresso machine protocols

- [antondlr/gicar-serial](https://github.com/antondlr/gicar-serial) — Gicar controllers
- [magnusnordlander/lelit-bianca-protocol](https://github.com/magnusnordlander/lelit-bianca-protocol) — Lelit Bianca
- [brewos-io/firmware](https://github.com/brewos-io/firmware) — replaces the whole controller, Duo/Matrix untested
- [Gaggiuino](https://github.com/Zer0-bit/gaggiuino) — microcontroller control for the Gaggia Classic
- [hellgelino/bezzera-bb005-digital-timer](https://github.com/hellgelino/bezzera-bb005-digital-timer), [hcrohland/bezzi-tank](https://github.com/hcrohland/bezzi-tank) — other Bezzera projects

## Scales and brew by weight

- [tatemazer/AcaiaArduinoBLE](https://github.com/tatemazer/AcaiaArduinoBLE) — BLE protocols for Acaia, Bookoo and Felicita; basis for `waage.h`
- [BooKooCode/OpenSource](https://github.com/BooKooCode/OpenSource) — BOOKOO protocol
- [Decent Scale API](https://decentespresso.com/decentscale_api) and [Programmers guide to the Half Decent Scale](https://decentespresso.com/docs/programmers_guide_to_the_half_decent_scale)
- [decentespresso/openscale](https://github.com/decentespresso/openscale) — open hardware and software of the Half Decent Scale (ESP32, Bluetooth, Wi-Fi)
- [Beanconqueror](https://github.com/graphefruit/Beanconqueror) — open app supporting many Bluetooth scales; useful for comparing protocols

## Grind by weight

- [Guillaume Besson: Coffee Grinder Smart Scale](https://besson.co/projects/coffee-grinder-smart-scale) — the origin: an ESP32 scale switches off a Eureka Mignon via relay
- [jb-xyz/openGBW](https://github.com/jb-xyz/openGBW) — further development: ESP32, HX711, OLED, rotary encoder, button-pulse mode
- [jaapp/smart-grind-by-weight](https://github.com/jaapp/smart-grind-by-weight) — ESP32-S3 with touch AMOLED, learns the drip after stop, tops up in pulses
- [preetpatel/smart-grind-by-weight](https://github.com/preetpatel/smart-grind-by-weight) — fork with diagnostics and more reliable motor/BLE handling

## Tools

- [sigrok / PulseView](https://sigrok.org/wiki/Downloads) — logic analyzer
