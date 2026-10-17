# -*- coding: utf-8 -*-
"""从反编译 C 提取 TIML 参数注册表 (name → 运行时槽位 → 类型码).

原理: 每个 TIML 类别有一个注册函数 (如 PlMotionCommon = FUN_14191f7d0),
逐项调用分配器 FUN_142171fe0 生成描述项 {名字串VA, 类型码, owner, owner+槽位, ...}。
名字串在 .rdata 明文; 槽位 = 该类别采样值运行时块的偏移。

用法: python extract_timl_registry.py [dump.c 路径]
默认: E:/MHW-arts/Reverse/MonsterHunterWorld_dump.exe.c
已知注册函数: FUN_14191f7d0 = PlMotionCommon (65 项, 行 5174891-5176565);
其余类别 (PI/Visual/Collision/GameParameter) 函数待定位 —
线索: 全部共用分配器 FUN_142171fe0, 按 xref 扫即可枚举。
"""
import re
import sys

C_PATH = sys.argv[1] if len(sys.argv) > 1 else r"E:/MHW-arts/Reverse/MonsterHunterWorld_dump.exe.c"
RDATA_FILE_DELTA = 0x1A00  # .rdata: VA - 0x140000000 - 0x1A00 = file offset
IMAGE_BASE = 0x140000000

FUNCS = {
    "FUN_14191f7d0": ("PlMotionCommon", 5174891, 5176565),
}


def main() -> None:
    data = open(r"E:/MHW-arts/Reverse/MonsterHunterWorld_dump.exe", "rb").read()
    lines = open(C_PATH, encoding="utf-8", errors="replace").readlines()

    def rd_str(va: int) -> str:
        o = va - IMAGE_BASE - RDATA_FILE_DELTA
        return data[o:o + 48].split(b"\0")[0].decode(errors="replace")

    for fn, (cat, a, b) in FUNCS.items():
        seg = lines[a - 1:b - 1]
        cur_n = cur_name = None
        out = []
        for ln in seg:
            m = re.search(r"uStack_68 = CONCAT44\(uStack_68\._4_4_,(0x[0-9a-fA-F]+|\d+)\)", ln)
            if m:
                cur_n, cur_name = int(m.group(1), 0), None
                continue
            m = re.search(r"\*puVar4 = (0x[0-9a-fA-F]{9,});", ln)
            if m:
                cur_name = int(m.group(1), 16)
                continue
            m = re.search(r"puVar4\[4\] = param_1 \+ (0x[0-9a-fA-F]+|\d+);", ln)
            if m and cur_name:
                out.append((int(m.group(1), 0), rd_str(cur_name), cur_n))
                cur_n = cur_name = None
        print("== %s (%s): %d 项" % (fn, cat, len(out)))
        for slot, name, tc in sorted(out):
            print("  owner+0x%-5x type=%-3d %s" % (slot, tc, name))


if __name__ == "__main__":
    main()
