import struct, re

path = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col"
data = open(path,'rb').read()

# find all UTF-8 katakana/kanji name-like strings with offsets
name_re = re.compile(rb'(?:[\xe3\xe4\xe5\xe6\xe7\xe8\xe9][\x80-\xbf]{2}){2,}')
strs = [(m.start(), m.end(), m.group()) for m in name_re.finditer(data)]
print(f"{len(strs)} name strings found")
for off, end, s in strs[:8]:
    print(f"  @0x{off:04x}-0x{end:04x}: {s.decode('utf-8',errors='replace')}")
print("  ...")
for off, end, s in strs[-4:]:
    print(f"  @0x{off:04x}-0x{end:04x}: {s.decode('utf-8',errors='replace')}")

# what follows the last name?
last_end = strs[-1][1]
print(f"\nlast name ends @0x{last_end:04x}, file len 0x{len(data):04x}, tail = {len(data)-last_end} bytes")
print("tail hexdump (first 256):")
tail = data[last_end:last_end+256]
for i in range(0, len(tail), 16):
    chunk = tail[i:i+16]
    hexs = ' '.join(f'{b:02x}' for b in chunk)
    print(f"  0x{last_end+i:04x}: {hexs}")
