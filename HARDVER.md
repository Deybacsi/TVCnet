# TVCnet: összeszerelés

A TVCnet egy kis ESP32-S3 modul. A TVC magnó aljzatába dugva WiFi-t ad a gépnek. A TVC a magnó vonalain beszélget vele: a motorvezérlő lábon küldi a kérést, és a magnó bemenetén kapja a választ. Hogy mi megy a vonalakon, azt a [PROTOKOLL.md](PROTOKOLL.md) írja le.

A gépet nem kell megbontani. Elég egy DIN dugó, egy ESP modul és két ellenállás.

## Alkatrészek

| Alkatrész | Mire kell |
|---|---|
| ESP32-S3-Zero (Waveshare vagy klón; ESP32-S3FH4R2, 4 MB flash) | a modul maga; a firmware erre a lapra készült |
| 5 pólusú, 180°-os DIN dugó (apa) | a TVC magnó aljzata ilyen; a 240°-os 5 pólusú dugó nem illik bele |
| 4,7 kΩ ellenállás | felhúzó ellenállás a motor vonalon |
| 1 kΩ ellenállás | védő ellenállás a magnó bemenet felé |
| Schottky dióda (1N5819 vagy SS14), opcionális | csak akkor kell, ha a modul egyszerre lesz a TVC-ben és USB-n (lásd: Táp) |
| vezeték, kis doboz | a modul nem fér el a dugóházban: dugó, 5-10 cm kábel, a modul egy kis dobozban |

## Bekötés

| DIN láb | Jel a TVC-n | ESP32-S3-Zero | Közte |
|---|---|---|---|
| 5 | motorvezérlés (a TVC küld) | GPIO1 | 4,7 kΩ a GPIO1 és a 3V3 tüske közé |
| 3 | magnó bemenet (a TVC fogad) | GPIO2 | 1 kΩ sorosan |
| 4 | +5 V | 5V | opcionális dióda, anód a DIN felé |
| 2 | föld | GND | |
| 1 | magnó kimenet | - | nincs bekötve |

Mit miért:

- **Motor vonal (5-ös láb):** a TVC egy tranzisztorral kapcsolja a magnó motorját (75451, nyitott kollektor). Ez a tranzisztor a vonalat csak a földre tudja húzni, magas szintet nem ad. A 4,7 kΩ húzza fel a vonalat, így a GPIO1 tiszta 0 / 3,3 V-os jelet kap.
- **A felhúzás a 3V3 tüskére menjen, ne az 5V-ra!** Az ESP32-S3 lábai nem bírják az 5 V-ot.
- **Magnó bemenet (3-as láb):** a GPIO2 közvetlenül hajtja a TVC bemenetét. Az 1 kΩ csak védelem: áramot korlátoz, ha valami rossz helyre kerül (rövidzár, bekötési hiba, a másik aljzatba dugott magnó).
- **1-es láb:** ezen a TVC a magnónak küld hangot; a TVCnet nem használja.

### A DIN dugó lábai

- Az íven a lábak sorrendje **3 - 5 - 2 - 4 - 1**; a 2-es (föld) van középen.
- A számok a dugó betétjén is rajta vannak, forrasztáskor ahhoz igazodj. A forrasztási oldalról nézve a sorrend tükrözött, ezért könnyű eltéveszteni.

## Táp

- A TVCnet a TVC-ről kapja a tápot a 4-es lábon. WiFi forgalom közben kb. 350 mA-t vesz fel, üresjáratban kb. 40-100 mA-t.
- **Ne legyen egyszerre a TVC-ben és USB-n!** A modul 5V tüskéje a lapon közvetlenül az USB-C aljzat tápjára megy, dióda nélkül. Ha a modul a TVC-ben van, és közben a PC-hez is csatlakozik, a két gép 5 V-os tápja összekötődik.
- Firmware feltöltéshez húzd ki a TVC-ből, és csak utána dugd a PC-be.
- Ha mégis kell mindkettő (pl. a soros naplót akarod nézni, miközben a TVC használja), tegyél Schottky diódát a 4-es láb és az 5V tüske közé. Az anódja legyen a DIN felé, a csíkos vége az ESP felé. Így a PC nem táplál vissza a TVC-be. A diódán kb. 0,3-0,4 V esik, ez nem gond.

## Első próba

1. Töltsd fel a firmware-t USB-n (lásd [README.md](README.md)), aztán húzd ki a PC-ből.
2. Kapcsold ki a TVC-t, és dugd a TVCnet-et a magnó aljzatba. A TVC hátulján két magnó aljzat van, mindegyik a saját motorbitjéhez tartozik:
   - CH12: 05h b7;
   - CH13: 05h b6.

   A `tvc/` mappa példái a CH12-t használják.
3. A másik aljzatban ne legyen magnó: a két aljzat bemenete közös, egy lejátszó magnó jele összekeveredne a TVCnet válaszával.
4. Kapcsold be a TVC-t, és töltsd be a `tvc/basic/` mappából a `PING` programot. Ha kiírja a protokoll verzióját, a bekötés jó.
5. Utána a `WIFI` programmal add meg a WiFi hálózat nevét és jelszavát. A TVCnet elmenti ezeket, és bekapcsoláskor magától csatlakozik.

Ha nem tudod, melyik fizikai aljzat a CH12, mérd meg TVCnet nélkül:

- Tegyél feszültségmérőt az aljzat 4-es (+) és 5-ös (-) lába közé, és indíts egy BASIC `LOAD`-ot.
- Annál az aljzatnál, amelyiknek a motorját a ROM bekapcsolja, kb. 5 V-ot mérsz, a másiknál kb. 0 V-ot.

Az USB soros naplóban (115200 baud) minden kérés és válasz látszik. Ezt csak diódával szerelt modulon nézheted a TVC-n, vagy USB-ről táplálva, TVC nélkül.
