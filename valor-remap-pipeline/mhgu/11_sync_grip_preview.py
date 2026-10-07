# -*- coding: utf-8 -*-
"""
11_sync_grip_preview.py — 武器握持预览自动同步 v2 (swo022)

规则(与游戏侧插座语义一致):
    对玩家动作的每一帧, 读 MhBone_030.scale[1] / MhBone_047.scale[1] 的原始键值:
      047.scaleY == 2  -> 右手插座  (两者同为 2 时 047 优先)
      否则 030.scaleY == 2 -> 左手插座
      两者都为 1(闲置)  -> 保持上一帧的状态 (游戏里"不变表"; 键只编到前几帧、
                            后面没有键的动作, 也就是一直保持最后的握持)
    然后在 swo022 MhBone_000 的 GRIP_L_030/GRIP_R_047 influence 上写 CONSTANT
    瞬切阶梯, 与 LMT 键同帧。

v2 修正(v1 的病根):
    v1 只在运行那一刻按"当时的 active action"生成一份全局 influence 键 ——
    之后你在 Dopesheet 切到别的动作预览, swo022 继续播旧键(或残留的手搓
    FK_grip_preview_104), 武器就在旧动作的切换帧上改握持(典型症状: 预览
    divide_valor_sheath 却在 f12/13 跳换手 —— 那是 104 预览键的切换帧, 不是
    本动作的)。v2 挂 frame_change_post 帧回调, 指纹(动作名+030/047 scale[1]
    原始键)一变就整份重算: 换预览动作、改握持键都自动跟上, 过期键不可能再生效。
    另: 启动时把所有动作里的 GRIP influence 键清掉(含手搓的 FK_grip_preview_104),
    旧键彻底退场。

用法:
  GUI:  Text Editor 运行本脚本一次即可(之后整个会话自动跟随)。每次重开 blend
        需要再跑一次 —— Blender 帧回调不随 .blend 保存。
  无头: blender -b MHW-Valor.blend --python valor-remap-pipeline/mhgu/11_sync_grip_preview.py -- "LMT::w03_00::109"
        (一次性同步指定动作, 不注册回调, 不保存 blend; 动作名缺省=玩家骨架
         当时的 active action)
  卸载: blender ... --python ... -- --remove   (GUI 里改 REMOVE=True 运行同理)
  彻底清扫全库旧 GRIP 键: 无头加 --clean-all (如 -- --clean-all "动作名");
        默认跳过 —— swo022 只播 FK_grip_preview_auto, 旧键不会生效, 扫全库纯耗时

注意:
  - 读键一律走 keyframe_points 原始 co, 不用 fcurve.evaluate() —— LIMITS
    (bufType 元数据)会钳位 evaluate 的返回值。
  - 本脚本只动 swo022 的预览动作 FK_grip_preview_auto, 不碰 LMT 动作本身。
"""
import bisect
import math
import sys

import bpy

# ---------------- 配置 ----------------
PLAYER_OBJ = 'm_mesh Armature'
WEAPON_OBJ = 'swo022 Armature'
WEAPON_BONE = 'MhBone_000'
SOCKET_BONES = {'L': 'MhBone_030', 'R': 'MhBone_047'}
CONSTRAINT_NAMES = {'L': 'GRIP_L_030', 'R': 'GRIP_R_047'}
SCALE_PATHS = {s: 'pose.bones["%s"].scale' % b for s, b in SOCKET_BONES.items()}
INFL_PATHS = {s: 'pose.bones["%s"].constraints["%s"].influence' % (WEAPON_BONE, CONSTRAINT_NAMES[s])
              for s in SOCKET_BONES}
PREVIEW_ACT_NAME = 'FK_grip_preview_auto'
DEFAULT_SOCKET = 'L'      # 动作开头没有任何插座激活时的初始状态
ACTIVE_THRESHOLD = 1.5    # scaleY >= 1.5 视为握持(2), < 1.5 视为闲置(1)
REMOVE = False            # GUI 里想卸载回调就改 True 运行
SOURCE_ACTION_OVERRIDE = None   # 想固定源动作就填动作名, 缺省=玩家骨架 active action
# 全动作旧 GRIP 键清扫默认关: swo022 每次都被指到 FK_grip_preview_auto, 其他动作里
# 的残留 influence 键永远不会播放, 扫它们只是防御。需要彻底清理时改 True 或无头
# 传 --clean-all (每次运行都要遍历全库 fcurve, 是旧的耗时大头)。
CLEAN_ALL_LEGACY = False

_state = {'fp': object()}   # 哨兵: 保证第一次 tick 必刷


def _log(msg):
    print('[grip-sync] ' + msg)


# ---------------- 源动作读取 ----------------
def raw_keys(fcurve):
    """[(frame, value)] 原始键, 同帧取后一个键, 按帧排序 (LIMITS 不影响)。"""
    keys = {}
    for kp in fcurve.keyframe_points:
        keys[kp.co[0]] = kp.co[1]
    return sorted(keys.items())


def make_sampler(keys):
    """键只编到前几帧时, 之后每帧保持最后一个键的值 (对应游戏"闲置=不变表")。"""
    if not keys:
        return lambda f: 1.0
    frames = [k[0] for k in keys]

    def sample(f):
        if f <= frames[0]:
            return keys[0][1]
        if f >= frames[-1]:
            return keys[-1][1]
        i = bisect.bisect_right(frames, f) - 1
        f0, v0 = keys[i]
        f1, v1 = keys[i + 1]
        if f1 == f0:
            return v0
        t = (f - f0) / (f1 - f0)
        return v0 + (v1 - v0) * t  # 阶梯键不受影响; 渐变键按线性近似判定
    return sample


def grip_segments(act):
    """动作 -> [(start, end, 'L'|'R')]; 无 030/047 scale fcurve 时返回 None。"""
    f30 = act.fcurves.find(SCALE_PATHS['L'], index=1)
    f47 = act.fcurves.find(SCALE_PATHS['R'], index=1)
    if f30 is None and f47 is None:
        return None
    s30 = make_sampler(raw_keys(f30) if f30 else [])
    s47 = make_sampler(raw_keys(f47) if f47 else [])
    f_start = int(round(act.frame_range[0]))
    f_end = int(math.ceil(act.frame_range[1]))
    segs = []
    cur = None
    seg_start = f_start
    for f in range(f_start, f_end + 1):
        if s47(f) >= ACTIVE_THRESHOLD:
            s = 'R'
        elif s30(f) >= ACTIVE_THRESHOLD:
            s = 'L'
        else:
            s = cur  # 闲置 -> 保持上一帧
        if cur is None:
            cur = s or DEFAULT_SOCKET
            seg_start = f
        elif s != cur:
            segs.append((seg_start, f - 1, cur))
            cur = s
            seg_start = f
    segs.append((seg_start, f_end, cur))
    return segs


def grip_fingerprint(act):
    """动作名 + 两条 scale[1] 的原始键 —— 换动作或改握持键都会变。"""
    if act is None:
        return None
    parts = [act.name]
    for side in ('L', 'R'):
        fc = act.fcurves.find(SCALE_PATHS[side], index=1)
        parts.append(None if fc is None else
                     tuple((kp.co[0], kp.co[1]) for kp in fc.keyframe_points))
    return tuple(parts)


# ---------------- influence 写入 ----------------
def ensure_grip_constraints(weapon_obj, player_obj):
    pb = weapon_obj.pose.bones[WEAPON_BONE]
    for side, sub in SOCKET_BONES.items():
        con = pb.constraints.get(CONSTRAINT_NAMES[side])
        if con is None:
            con = pb.constraints.new('COPY_TRANSFORMS')
            con.name = CONSTRAINT_NAMES[side]
        con.target = player_obj
        con.subtarget = sub
        con.target_space = 'WORLD'
        con.owner_space = 'WORLD'
        con.mix_mode = 'REPLACE'
        con.mute = False
    return pb


def write_influence(act, side, keys):
    fc = act.fcurves.find(INFL_PATHS[side], index=0)
    if fc is None:
        fc = act.fcurves.new(INFL_PATHS[side], index=0)
    fc.keyframe_points.clear()  # remove-in-loop 会炸, 必须清空重建
    for f, v in keys:
        fc.keyframe_points.add(1)
        kp = fc.keyframe_points[-1]
        kp.co = (float(f), v)
        kp.interpolation = 'CONSTANT'
    fc.update()
    return len(keys)


def preview_action(weapon_obj):
    act = bpy.data.actions.get(PREVIEW_ACT_NAME)
    if act is None:
        act = bpy.data.actions.new(PREVIEW_ACT_NAME)
    else:
        act.fcurves.clear()
    if weapon_obj.animation_data is None:
        weapon_obj.animation_data_create()
    weapon_obj.animation_data.action = act
    act.use_fake_user = True
    return act


def rebuild(weapon_obj, player_obj, act):
    """按源动作 act 整份重算 influence。act=None/无握持曲线 -> 静态左手。"""
    pb = ensure_grip_constraints(weapon_obj, player_obj)
    act_prev = preview_action(weapon_obj)

    segs = grip_segments(act) if act is not None else None
    if segs is None:
        for side in ('L', 'R'):
            fc = act_prev.fcurves.find(INFL_PATHS[side], index=0)
            if fc is not None:
                act_prev.fcurves.remove(fc)
        pb.constraints[CONSTRAINT_NAMES['L']].influence = 1.0
        pb.constraints[CONSTRAINT_NAMES['R']].influence = 0.0
        _log('源动作: %s (无 030/047 scale fcurve) -> 静态左手' % (act.name if act else '无'))
        return

    f_start, f_end = segs[0][0], segs[-1][1]
    keys = {'L': [], 'R': []}
    for i, (a, b, sock) in enumerate(segs):
        for side in ('L', 'R'):
            keys[side].append((a, 1.0 if sock == side else 0.0))
        if i == len(segs) - 1 and b < f_end:
            for side in ('L', 'R'):
                keys[side].append((f_end, 1.0 if sock == side else 0.0))
    n_l = write_influence(act_prev, 'L', keys['L'])
    n_r = write_influence(act_prev, 'R', keys['R'])
    _log('源动作: %s  [%d..%d]' % (act.name, f_start, f_end))
    _log('插座片段: ' + ' | '.join('%d..%d %s' % (a, b, s) for a, b, s in segs))
    _log('已重写 %s: GRIP_L_030 %d 键 / GRIP_R_047 %d 键 (CONSTANT)' % (PREVIEW_ACT_NAME, n_l, n_r))


def neutralize_legacy_influence():
    """把所有动作里(除预览动作)的 GRIP influence 键清掉 —— 旧手搓键(如
    FK_grip_preview_104 的 12/13 台阶)不再可能与当前预览打架。"""
    removed = 0
    for act in bpy.data.actions:
        if act.name == PREVIEW_ACT_NAME:
            continue
        doomed = [fc for fc in act.fcurves
                  if '.constraints["GRIP_' in fc.data_path and fc.data_path.endswith('.influence')]
        for fc in doomed:
            act.fcurves.remove(fc)
        removed += len(doomed)
    if removed:
        _log('已清除 %d 条旧 GRIP influence 键 (含手搓预览动作)' % removed)


# ---------------- 帧回调: 动作/键一变就重算 ----------------
def grip_frame_handler(scene, depsgraph=None):
    try:
        player = bpy.data.objects.get(PLAYER_OBJ)
        weapon = bpy.data.objects.get(WEAPON_OBJ)
        if player is None or weapon is None:
            return  # 其他 blend / 临时文件
        ad = player.animation_data
        act = ad.action if ad else None
        fp = grip_fingerprint(act)
        if fp != _state['fp']:
            _state['fp'] = fp
            rebuild(weapon, player, act)
    except Exception as e:
        _log('handler 异常: %r' % (e,))


def register_handler():
    h = bpy.app.handlers.frame_change_post
    if grip_frame_handler not in h:
        h.append(grip_frame_handler)
        _log('已挂 frame_change_post 回调: 预览动作切换/握持键修改后自动重算 '
             '(回调不随 blend 保存, 重开会话需重跑本脚本)')


def unregister_handler():
    h = bpy.app.handlers.frame_change_post
    if grip_frame_handler in h:
        h.remove(grip_frame_handler)
        _log('已卸载 frame_change_post 回调')


def sync_now(override=None):
    player = bpy.data.objects.get(PLAYER_OBJ)
    weapon = bpy.data.objects.get(WEAPON_OBJ)
    if player is None or weapon is None:
        raise SystemExit('[grip-sync] ERROR: 找不到 %s / %s' % (PLAYER_OBJ, WEAPON_OBJ))
    if override:
        act = bpy.data.actions.get(override)
        if act is None:
            raise SystemExit('[grip-sync] ERROR: 找不到动作 %r' % override)
    else:
        ad = player.animation_data
        act = ad.action if ad else None
    _state['fp'] = grip_fingerprint(act)
    rebuild(weapon, player, act)


# ---------------- 入口 ----------------
_argv = sys.argv
_extra = _argv[_argv.index('--') + 1:] if '--' in _argv else []
if _extra and _extra[0] == '--remove':
    REMOVE = True
if _extra and _extra[0] == '--clean-all':
    CLEAN_ALL_LEGACY = True
    _extra = _extra[1:] if len(_extra) > 1 and not _extra[1].startswith('--') else []

if CLEAN_ALL_LEGACY:
    neutralize_legacy_influence()
if REMOVE:
    unregister_handler()
    _log('完成: 已卸载自动同步 (influence 保持最后一次的计算结果)。')
elif _extra:  # 无头一次性
    sync_now(_extra[0])
    _log('完成 (无头一次性, blend 未保存)。')
else:
    sync_now(None)
    if bpy.app.background:
        _log('完成 (后台模式, 未注册回调; blend 未保存)。')
    else:
        register_handler()
        _log('完成: 之后预览任何动作都会自动同步握持。')
