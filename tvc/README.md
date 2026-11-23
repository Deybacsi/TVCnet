# TVC oldali programok

Z80 programok a TVC-re, [sjasmplus](https://github.com/z00m128/sjasmplus)-szal fordíthatók. A TVCnet a CH12 aljzatban van (05h b7), 2400 baud, T = 250 µs; a protokoll: [PROTOKOLL.md](../PROTOKOLL.md).

| Fájl | Tartalom |
|---|---|
| `tvcnet.inc` | a kérés-válasz rutinok: `tn_cmd` (A = parancs, HL = adat, B = hossz; vissza: A = állapot, CY = 8 próba után sincs jó válasz, a válaszkeret a `tn_reply`-ban: LEN, CTRL, adat, CHK). A váltakozó bitet és az ismétlést is intézi. |
| `common.inc` | a tesztek közös része: napló 7000h-tól, a napló vége 6FF0h-n, a sikertelen kérések száma 6FF2h-n |
| `ping.asm` | `PING` és `ECHO`; a napló: minden válasz állapota, hossza és adata |
| `get.asm` | `GET` egy URL-re, aztán `NEXT` a végéig; a napló a letöltött tartalom, az utolsó állapot 6FF4h-n |
| `tcp.asm` | `OPEN` egy echo szerverre, `SEND`, `NEXT`, amíg minden vissza nem jött, `CLOSE`; az állapotok 6FF8h-tól, a napló a visszakapott adat |
| `basic/` | TVC BASIC példák (`PING`, `WIFI`, `GET`, `TERM`) listaként és CAS fájlként, egy kis gépi kódú segédrutinnal; leírás: [basic/README.md](basic/README.md) |
| `hayes.asm` | egy Hayes munkamenet `TERM`-mel, egy forgatókönyv alapján (`ATE0`, `ATI4`, `ATDT`, `+++`, `ATO`, `ATH`, `AT&G`, egy elutasított kapcsolat, `ATV0`); a napló minden, amit a TVCnet kiírt |

- Fordítás: `sjasmplus --sym=ping.sym ping.asm`; a kimenet `ping.bin`, a betöltési cím 5000h.
- A programok DI-vel futnak, és a `done` címkén álló végtelen ciklusban állnak meg.
- A címek (`127.0.0.1:8765` HTTP, `127.0.0.1:8766` TCP echo) a tesztkörnyezethez valók; valódi géphez a programban kell átírni őket.
- A `tvcnet.inc` időzítése (`TN_BITN`, `TN_THRESH`) 3,125 MHz-re és a nem videó RAM-ban futó kódra van számolva.
- freeTVC-ben a `tools/tvcnet/run_checks.sh` mind a négyet lefuttatja helyi HTTP és TCP echo szerver ellen, és ellenőrzi a naplókat.
