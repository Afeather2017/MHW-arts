# -*- coding: utf-8 -*-
"""Compact ATK damage table for the moves currently in play (deployed CTO routing)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import col_tool

COL = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\collision\wp03.col"

d, info = col_tool.parse(COL)
names = info["names"]
atks = info["atks"]
by_node = {n["unk4"]: n for n in names}

FIELDS = ["Motion_Value", "Fixed_Damage", "Part_Modifier", "Elem_Modifier",
          "Status_Modifier", "Stun", "Exhaust", "Mount", "Soften", "MindsEye",
          "HitLag_a", "Energy_Gen", "Cond_MV", "Cond_Elem",
          "Lingering", "Frame_Delay", "No_Sharpness"]

# label -> (LMT entry, deployed CTO node) ; None node = no CTO in deployed file
MOVES = [
    ("拔刀纵斩 101",      101, 44),
    ("踏步斩 102",        102, 43),
    ("斬り下がり 103",    103, 58),
    ("气刃斩1 104",       104, 56),
    ("突刺 105",          105, 42),
    ("气刃斩2 (原版106)", 106, 54),
    ("106 当前(部署)",    106, 63),
    ("斩り上げ 107",      107, 40),
    ("气刃3一段 108",     108, 53),
    ("气刃3二段 108",     108, 59),
    ("气刃3三段 108",     108, 62),
    ("大回旋 109",        109, 52),
    ("气刃突刺 110",      110, 46),
    ("左右移动斩 127",    127, 57),
    ("刚1/见切反击行",    140, 63),
    ("divide141/142",     141, 56),
    ("divide143 窗1",     143, 84),
    ("divide143 窗2",     143, 101),
    ("一文字短 339",      339, 63),
    ("一文字长 340",      340, 63),
    ("无双斩1 343",       343, 63),
    ("无双斩2 343",       343, 63),
    ("强化射击槽 346",    346, 54),
    ("居合1 307",         307, 84),
    ("居合2 307",         307, 101),
    ("大居合(无) 308",    308, 86),
    ("大居合(白) 309",    309, 87),
    ("大居合(黄) 310",    310, 88),
    ("大居合(赤) 311",    311, 89),
]

hdr = "%-16s %4s %-14s %4s %6s %6s %6s %5s %5s %5s %5s %5s %6s %5s %6s %6s" % (
    "move", "row", "col名", "bank", "MV", "Fixed", "PartM", "Elem", "Stat",
    "Stun", "Exh", "Soft", "HitLag", "EGen", "CondMV", "Lng/FD")
print(hdr)
print("-" * len(hdr))
seen = set()
for label, entry, node in MOVES:
    n = by_node.get(node)
    if not n:
        print("%-16s %4s %-14s  (node %d 不在名字表)" % (label, "-", "node%d" % node, node))
        continue
    row = n["move"]
    a = atks[row]
    print("%-16s %4d %-14s %4d %6g %6g %6g %5g %5g %5g %5g %5g %6.2f %5g %6g %s" % (
        label, row, n["name"][:14], n["cgp"],
        a["Motion_Value"], a["Fixed_Damage"], a["Part_Modifier"], a["Elem_Modifier"],
        a["Status_Modifier"], a["Stun"], a["Exhaust"], a["Soften"],
        a["HitLag_a"], a["Energy_Gen"], a["Cond_MV"],
        "%d/%g" % (a["Lingering"], a["Frame_Delay"])))

print()
print("强化射击族 332-349: 刀身 CTO = 无(原版/部署一致, 除 346 = 气刃斩2的CTO)")
print("攻击类型全部 Attack_Type=1; 详细字段见 col_tool.py atk <row>")
