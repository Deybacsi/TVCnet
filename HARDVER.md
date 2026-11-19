# TVCnet: összeszerelés

## Alkatrészek

- ESP32-S3-Zero (Waveshare vagy klón; ESP32-S3FH4R2, 4 MB flash)
- 5 pólusú, 180°-os DIN dugó (apa); a 240°-os nem jó
- 4,7 kΩ ellenállás
- 1 kΩ ellenállás
- opcionális: Schottky dióda (1N5819 vagy SS14)

## Bekötés

| DIN láb | ESP32-S3-Zero | Közte |
|---|---|---|
| 5 | GPIO1 | 4,7 kΩ a GPIO1 és a 3V3 közé |
| 3 | GPIO2 | 1 kΩ sorosan |
| 4 | 5V | opcionális dióda, anód a DIN felé |
| 2 | GND | |
| 1 | - | nincs bekötve |

A DIN dugó betétjén a lábak számozva vannak, ahhoz igazodj; a forrasztási oldalon a sorrend tükrözött.

## Táp

- A TVCnet a TVC-ről kapja a tápot (4-es láb). WiFi forgalom közben kb. 350 mA-t vesz fel.
- **Ne legyen egyszerre a TVC-ben és USB-n:** a modul 5V tüskéje közvetlenül az USB-re megy, így a PC és a TVC tápja összekötődne. Feltöltéshez húzd ki a TVC-ből.
- Ha mégis kell mindkettő (pl. USB napló a TVC-n), tegyél diódát a 4-es láb és az 5V tüske közé.

## Használat

- A TVC hátulján két magnó aljzat van; a TVCnet-et abba dugd, amelyiket a TVC program használja. A másik aljzatban ne legyen magnó.
- Az USB soros naplóban (115200 baud) minden kérés és válasz látszik.
