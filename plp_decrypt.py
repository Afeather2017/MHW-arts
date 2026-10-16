# -*- coding: utf-8 -*-
"""MHW pl_param.plp (Iceborne 加密) 解密 + 玩家耐力参数表读取.

算法来源: Synthlight/MHW-Editor Util/EncryptionHelper.cs —
Blowfish ECB, 数据按 4 字节字序交换后解密、解完再换回 (BSwap), key 为文件模型内的定数。
上游 Generated/MHW_Editor/Structs/PlData/PlPlayerParam.cs = 权威字段结构
(本地安装版 MHW-Template.dll 编译较早不含 plp, 故走本脚本)。

用法:
  python plp_decrypt.py                      # 默认解密合并提取版并打印耐力表
  python plp_decrypt.py <in.plp> [out.plp]   # 指定输入/输出

偏移 (0x...) 为当前 plp 布局实测值; 模板参照
col_research/mhw-editor-src/010 Templates/plp.bt (struct 12 字节步长, 4 对齐)。
"""
import struct
import sys

KEY = "j1P15gEkgVa7NdFxiqwCPitykHctY2nZPjSaElvqb0eSwcLO1cOlTqqv"

DEFAULT_IN = r"chunk-extracted\chunk\common\pl\pl_param.plp"
DEFAULT_OUT = "_pl_param_decrypted.plp"

# 已验证偏移 (含 Health/Stamina 块; 详见 AGENTS 2026-10-17(2))
STAMINA_CONSUMPTION = (0x208, [
    "Dodge", "LS_Counter", "SA_Wild_Swing", "Unk1", "Unk2", "Unk3", "Bow_Shoot",
    "Unk4", "Unk5", "Unk6", "Bow_Charge_Step", "Unk7", "Claw_Attack_1",
    "Claw_Attack_2", "Claw_Attack_3", "Claw_Attack_4", "Unk8", "Unk9",
])
STAMINA_TIME_REDUCE_MCORE = (0x250, [
    "Unk1", "Unk2", "Unk3", "Hammer/Bow_Charge_Drain", "Unk4", "Unk5", "Unk6",
    "Unk7", "Lance_s_Power_Guard", "Unk8", "Clutch_Drain_Whilst_Grappled_",
    "Clutch_Counter", "Unk9", "Unk10", "Unk11", "Unk12", "Unk13", "Unk14",
])


def bswap(data: bytes) -> bytes:
    out = bytearray(len(data))
    out[0::4], out[1::4], out[2::4], out[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
    return bytes(out)


def decrypt_plp(data: bytes) -> bytes:
    from Crypto.Cipher import Blowfish
    cipher = Blowfish.new(KEY.encode("utf-8"), Blowfish.MODE_ECB)
    return bswap(cipher.decrypt(bswap(data)))


def dump_table(dec: bytes, off: int, names, title: str) -> None:
    print("== %s @0x%X (1-based 行号, 单位=耐力/秒)" % (title, off))
    for i, nm in enumerate(names):
        print("   [%2d] %-32s %.1f" % (i + 1, nm, struct.unpack_from("<f", dec, off + i * 4)[0]))


def main() -> None:
    inp = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IN
    outp = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    dec = decrypt_plp(open(inp, "rb").read())
    if dec[:4] != b"PLP\x00":
        raise SystemExit("解密失败: magic=%r (key 或文件不对)" % dec[:4])
    print("magic:", dec[:8], "| len:", len(dec))
    if outp:
        open(outp, "wb").write(dec)
        print("[ok] 明文已写 %s" % outp)
    dump_table(dec, *STAMINA_TIME_REDUCE_MCORE, title="Stamina Time Reduce mCore (強G掉耐力档位表)")
    dump_table(dec, *STAMINA_CONSUMPTION, title="Stamina Consumption (按次消耗)")


if __name__ == "__main__":
    main()
