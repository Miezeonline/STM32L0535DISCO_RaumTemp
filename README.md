# STM32L0535DISCO_RaumTemp
STM32L538 Disco-Board mit drei externen Umweltboards

Boards:
STM32L0538-DISCO
STEVAL-MKI198V1 (STTS) auf STEVAL-MKIGIBV1
STEVAL-MKI183VI (HTS)
STEVAL-MKI142V2 (LPS)

Aufbau auf Lochraster, Pins vom DISCO-Board: 3V; GND; PB9; PB8

Es gibt drei Seiten auf dem EPaper die mit dem blauen User-Button durchgeschaltet werden:
- aktuelle Temperatur (STTS), Luftfeuchtigkeit (HTS) und Temperatur
- alle Werte aus allen Sensore
- min/max-Werte seit letztem Reset

in der Datei font20epd.c ist das ~ geändert, damit wird das °C auf der Startseite erzeugt. Der Font bietet leider kein ° an.

Softwareversionen:
STM32CUbeIDE: Version: 2.3.0
STMCubeMX: Version 4.21.0
