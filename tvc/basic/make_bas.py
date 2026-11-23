# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the BASIC listings: src/NAME.bas + the shared subroutines + helper.asm as DATA lines -> NAME.bas.
# Usage: python3 make_bas.py SJASMPLUS
import pathlib
import subprocess
import sys
import tempfile

here = pathlib.Path(__file__).resolve().parent
LOMEM = 8192

with tempfile.TemporaryDirectory() as work:
    subprocess.run([sys.argv[1], "--nologo", "--msg=war", "--sym=helper.sym", str(here / "helper.asm")],
                   cwd=work, check=True)
    code = (pathlib.Path(work) / "helper.bin").read_bytes()
    symbols = {}
    for line in (pathlib.Path(work) / "helper.sym").read_text().splitlines():
        name, _, value = line.partition(": EQU ")
        if value:
            symbols[name] = int(value, 16)

start, reply, request = symbols["start"], symbols["tn_reply"], symbols["RQ"]
if request + 257 > LOMEM:
    sys.exit("the helper and its buffers do not fit under LOMEM")

shared = f"""1 LOMEM {LOMEM}
2 DIM D$*254
3 GOSUB 9000
8000 REM C: parancs, D$: adat -> Z: allapot (-1: nincs valasz)
8010 L=LEN(D$):POKE Q,C:POKE Q+1,L
8020 IF L THEN FOR I=1 TO L:POKE Q+1+I,ORD(D$(I:I)):NEXT I
8030 Z=USR(M)
8040 RETURN
8100 REM A valasz kiirasa
8110 R=PEEK(W):IF R=0 THEN RETURN
8120 FOR I=0 TO R-1:B=PEEK(W+2+I)
8130 IF B=13 OR (B=10 AND P<>13) THEN PRINT
8140 IF B>31 AND B<127 THEN PRINT CHR$(B);
8150 P=B:NEXT I:RETURN
8200 REM N kiirasa szokozok nelkul
8210 N$=STR$(N):IF N$(1:1)=" " THEN N$=N$(2:LEN(N$))
8220 PRINT N$;:RETURN
9000 REM A TVCnet gepi kod betoltese
9010 M={start}:Q={request}:W={reply}:S=0
9020 FOR I=0 TO {len(code) - 1}:READ B:POKE M+I,B:S=S+B:NEXT I
9030 IF S<>{sum(code)} THEN PRINT "Hibas DATA sor!":STOP
9040 C=1:D$="":GOSUB 8000
9050 IF Z THEN PRINT "A TVCnet nem valaszol:";Z:STOP
9060 RETURN
"""
data = [f"{9100 + 10 * n} DATA {','.join(str(b) for b in code[i:i + 16])}" for n, i in enumerate(range(0, len(code), 16))]

for source in sorted((here / "src").glob("*.bas")):
    lines = {}
    for line in (source.read_text().splitlines() + shared.splitlines() + data):
        number = int(line.split(" ", 1)[0])
        if number in lines:
            sys.exit(f"{source.name}: line {number} twice")
        lines[number] = line
    (here / source.name).write_text("".join(lines[n] + "\n" for n in sorted(lines)))
    print(f"{source.name}: {len(lines)} lines")
print(f"helper: {len(code)} bytes at {start}, request {request}, reply {reply}")
