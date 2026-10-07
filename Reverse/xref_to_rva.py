# Scan the dump exe for E8/E9 rel32 calls/jmps targeting a given RVA.
import struct, sys

DUMP = r"E:\MHW-arts\Reverse\MonsterHunterWorld_dump.exe"
TEXT_VA = 0x140001000

buf = open(DUMP, "rb").read()
# .text section: RVA 0x1000, find file offset mapping used by disasm_rva (file off = rva - 0x600 observed)
# safer: brute force scan whole file and convert via the same rule disasm uses:
# disasm said RVA 0x2233d80 -> file offset 0x2233180 (delta 0xC00)
DELTA = 0xC00
targets = [int(x, 0) for x in sys.argv[1:]]
for t in targets:
    hits = 0
    for i in range(len(buf) - 5):
        if buf[i] in (0xE8, 0xE9):
            rel = struct.unpack_from("<i", buf, i + 1)[0]
            src_rva = i + DELTA
            dst = (src_rva + 5 + rel) & 0xFFFFFFFF
            if dst == t:
                print("0x%X %s from RVA 0x%X (VA 0x%X)" % (src_rva, "call" if buf[i] == 0xE8 else "jmp", src_rva, 0x140000000 + src_rva))
                hits += 1
    print("target RVA 0x%X: %d rel32 refs" % (t, hits))
