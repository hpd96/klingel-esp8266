# klingel-esp8266

aus Bauvorschlag c't 

https://www.heise.de/ct/ausgabe/2017-17-Die-Tuerklingel-im-Heimnetz-3787241.html

> Die Klingel überhören nervt. Mit Raspberry Pi und Fritzbox klingelts nicht nur an der Tür, sondern auch auf Telefonen und Smartphones.

hier aber mit ESP8266 und MQTT und mit openHAB 2.5 und Fritz!Box via SIPCMD

WLAN im ESP8266 immer aktiv, sonst dauert es zu lange bis DECT-Telefone klingeln. Hier aktuell <1 Sekunde Verzögerung

## openhab

see klingel.rules klingel.items

part of klingel-sitemap:

Frame label="Klingel" {
				Group item=HausTorKlingelStatus { Frame {
				Text item=HausTorKlingelVersion
				Text item=HausTorKlingelWiFi
				}}
                                Text item=HausTorKlingel_wann
				Switch item=HausTorKlingelAktiv mappings=[ON=Aktiv,OFF=Leise]
}

## SIP CMD

OH rule ruft unix script auf:
n3a-dect-gruppe.sh

Telefongruppe 701 wird über Kode c**701 aufgerufen

für Befehl sipcmd siehe: ...
