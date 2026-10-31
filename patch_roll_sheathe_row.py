# -*- coding: utf-8 -*-
"""给 LS set-3 请求分类器表的翻滚动作加勇気納刀候选行 (1,23,1,0,3,98)。

背景 (2026-10-31 SESSIONS 10-31(14)):
  勇気納刀切入(vanilla 特殊納刀)由 set-3 请求分类器驱动: 动作 id -> 子表,
  行 {type,输入组合id,匹配a,匹配b,set,目标id} (stride 0x18)。
  LS 表顶层 VA 0x143F4D6A0 (113 键=vanilla 动作 id 0-112; 文件里 count=0
  运行时回填, key/ptr 静态)。带 (1,23,...,98) 行的 51 个键 = 全部可切纳刀
  的动作 (含 key 11 = 気刃斬1/LMT104); 翻滚 18-22 没有。TIML/FSM 两轮实验
  判负 = 该表才是闸门。

补丁 (全在文件静态字节上, 5 行 x 24B, count 不动):
  key 18/19 共享子表 0x143F4EA50:
    row[13] (1,408,0,0,-1,-1) 占位行 -> (1,23,1,0,3,98)
      [该行原本用 -1 目标连禁 row14]
    row[14] (1,288,1,0,3,66)  -> (1,288,0,0,-1,-1)
      [row13 不再连禁后, 自禁用保持 vanilla 刻意关掉的 スリF]
  key 20/21/22 子表 (0x143F4EC30/ED70/EEB0):
    row[11] (1,278,0,1,1,532) 休眠行(目标 set1:532 不存在) -> (1,23,1,0,3,98)

用法: python patch_roll_sheathe_row.py <game exe> <out exe>
"""
import struct
import sys

# (row_va, expect_old, new_row)
PATCHES = [
    (0x143F4EB88, (1, 408, 0, 0, -1, -1), (1, 23, 1, 0, 3, 98)),   # key18/19 r13
    (0x143F4EBA0, (1, 288, 1, 0, 3, 66), (1, 288, 0, 0, -1, -1)),  # key18/19 r14 自禁用
    (0x143F4ED38, (1, 278, 0, 1, 1, 532), (1, 23, 1, 0, 3, 98)),   # key20 r11
    (0x143F4EE78, (1, 278, 0, 1, 1, 532), (1, 23, 1, 0, 3, 98)),   # key21 r11
    (0x143F4EFB8, (1, 278, 0, 1, 1, 532), (1, 23, 1, 0, 3, 98)),   # key22 r11
]

EXPECTED_SHEATHE_REF = (0x143F4F1F0, (1, 23, 1, 0, 3, 98))  # key11 原版行, 作存在性锚点


def sections(data):
    e_lfanew = struct.unpack_from('<I', data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b'PE\0\0', 'not PE'
    coff = e_lfanew + 4
    nsec, = struct.unpack_from('<H', data, coff + 2)
    optsz, = struct.unpack_from('<H', data, coff + 16)
    opt = coff + 20
    imagebase, = struct.unpack_from('<Q', data, opt + 24)
    secs = []
    so = opt + optsz
    for _ in range(nsec):
        vsz, va, rsz, ra = struct.unpack_from('<IIII', data, so + 8)
        secs.append((va, vsz, ra, rsz))
        so += 40
    return imagebase, secs


def va2off(data, va):
    imagebase, secs = sections(data)
    rva = va - imagebase
    for sva, vsz, ra, rsz in secs:
        if sva <= rva < sva + max(vsz, rsz):
            return ra + (rva - sva)
    raise KeyError(hex(va))


def main(src, dst):
    data = bytearray(open(src, 'rb').read())

    anchor_off = va2off(data, EXPECTED_SHEATHE_REF[0])
    anchor = struct.unpack_from('<6i', data, anchor_off)
    assert anchor == EXPECTED_SHEATHE_REF[1], f'anchor mismatch: {anchor}'
    print(f'anchor OK: key11 原版纳刀行 @ {EXPECTED_SHEATHE_REF[0]:#x}')

    for va, old, new in PATCHES:
        off = va2off(data, va)
        cur = struct.unpack_from('<6i', data, off)
        assert cur == old, f'{va:#x}: {cur} != {old}'
        print(f'{va:#x}: {cur} -> {new}')

    for va, old, new in PATCHES:
        off = va2off(data, va)
        data[off:off + 0x18] = struct.pack('<6i', *new)

    open(dst, 'wb').write(data)
    print(f'\nwrote {dst}')

    old_bytes = open(src, 'rb').read()
    diffs = [i for i in range(min(len(old_bytes), len(data))) if old_bytes[i] != data[i]]
    runs = []
    for i in diffs:
        if runs and i == runs[-1][1]:
            runs[-1][1] = i + 1
        else:
            runs.append([i, i + 1])
    print(f'byte diffs: {len(diffs)} in {len(runs)} runs (expected 24x{len(PATCHES)}={24*len(PATCHES)})')
    for a, b in runs:
        print(f'  {a:#x}..{b:#x} ({b-a}B)')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
