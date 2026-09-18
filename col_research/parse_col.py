import struct, sys

def parse(path):
    data = open(path,'rb').read()
    print(f"=== {path} ({len(data)} bytes) ===")
    magic = data[4:8]
    print("magic:", magic)
    h = struct.unpack_from('<4I', data, 8)
    print("header u32 @8:", h)  # guesses: count, ?, ?, ?
    return data

if __name__ == '__main__':
    parse(sys.argv[1] if len(sys.argv)>1 else r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col")
