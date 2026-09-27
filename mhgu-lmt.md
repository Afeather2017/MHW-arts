总结

一、做了什么

整个工作是把 MHGU（怪物猎人XX/GU）的 LMT 动画格式 的解析能力，集成到 Blender 的 freekinetics 插件中，目标是让 MHGU 动画能被导入 Blender，再通过现有的 Rig Transfer 流程迁移/导出为 MHW（怪物猎人世界）的 LMT v95 格式。

具体完成了以下步骤：

阶段 内容

格式逆向分析（Task #1） 交叉对比了三个信息源：① MT Framework.mzp（3ds Max 脚本）、② RevilLib（C++ 权威实现，MHGULmtTool 已在使用）、③ 真实 MHGU 文件（w07.lmt、co_0.lmt 等 hexdump）。确认 MHGU = LMT version 67，32 位偏移、60→64 字节对齐的 Action Header、36 字节 Bone Header 等结构差异。写成了 MHGU_LMT_FORMAT.md 文档。

架构决策 不另起炉灶 —— MHGU 解析器解析后直接填充现有的 MHW 对象模型（LMT / LMTActionHeader / LMTBoneDataHeader + 已有的 codec 类），这样下游的 Blender 导入、节点树、Rig Transfer、MHW 导出全部不用改。

解析器实现（Task #2） 在 Blender 插件中新建了 struct/MhguLmt.py（213 行），实现了完整的 MHGU LMT 二进制解析。修改了 struct/Lmt.py 添加版本嗅探（version ≤ 90 → 走 MHGU 解析器）。

独立测试验证 写了 test_mhgu.py + mathutils stub，在 CPython 中脱离 Blender 运行。发现并修复了一个游标未恢复的 bug。三个真实文件全部解析成功，与 RevilLib 结构计数完全一致，四元数最大偏差仅 0.0004。

值级交叉验证（进行中） 给 MHGULmtTool（C++）添加了 dump 子命令，通过 RevilLib 输出每个关键帧的评估值，用来和 Python 解析器的输出做逐值对比。修复了编译问题（RTTI 冲突改用 static_cast、API 差异、Evaluate 传参错误等），dump 工具已基本能跑出数据。

二、工作路径

类别 路径

Blender 插件（主要代码目录） C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons\MHW-FreeKinetics\

研究/资源根目录 E:\MHW-arts\research\

MHGU 解包 LMT 样本 E:\MHW-arts\research\mhgu\extracted\

格式分析文档 E:\MHW-arts\research\mhgu\MHGU_LMT_FORMAT.md

独立测试脚本 E:\MHW-arts\research\mhgu\parsetest\test_mhgu.py

MT Framework.mzp 解压 E:\MHW-arts\research\MTFramework-mzp\

RevilLib（权威参考） E:\MHW-arts\research\RevilLib-current\ 和 E:\MHW-arts\research\RevilMax\3rd_party\RevilLib\

MHGULmtTool（C++ 工具） E:\MHW-arts\MHGULmtTool\

三、下一步是啥

当前在做的值级验证还没彻底跑通 —— dump 工具最后一个 key 的 Evaluate 调用疑似有越界崩溃（Claude 在加调试输出时撞上了 API 限速 429，工作被迫中断）。

接下来应该按这个顺序推进：

1. 完成 dump 工具调试 —— 解决 Evaluate 的崩溃（可能是 vendored RevilLib 的 frame-vs-index 语义问题），让 dump 能完整输出所有 track 的所有关键帧评估值。
2. 写对比脚本 —— 用 RevilLib 的 dump 输出作为 ground truth，逐 key 对比 Python 解析器的值（重点验证 bit-order 没有 w↔x 之类的交换错误）。
3. Blender 内实际导入测试 —— 在 Blender 中通过 freekinetics 的 UI 导入一个 MHGU .lmt 文件，检查动画是否正确显示在节点树上。
4. 端到端迁移测试 —— 用 Rig Transfer + bmaps 把 MHGU 动画迁移到 MHW 骨骼，再导出为 MHW v95 LMT，在游戏中验证。
5. 后续增强（已记录但 v1 跳过）：events（MHGU 事件系统 → MHW TIML 重新制作）和 floats（FloatTracks → MhBone_030/047 scale 通道桥接）。

按你之前说的节奏 —— 每一步做完停下来报告，你过一遍再继续。当前状态是 Task #2 的解析器代码已写完且结构测试通过，值级验证进行到一半。
