# -*- coding: utf-8 -*-
"""
grip_sync_preview.py — 武器握持预览自动同步 (swo022) 的 Blender 插件版

功能与 11_sync_grip_preview.py (GUI 粘贴版) 完全一致, 只是包装成 addon:
装一次 + 启用后每个会话自动生效, 不用再往 Python Console 粘贴脚本
(粘贴大段脚本会触发 Blender 控制台的多行语法 bug)。

安装: Edit > Preferences > Add-ons > Install... 选本文件, 然后勾选启用。
      (或把本文件放进 addons 目录后刷新)
使用: 启用即自动同步当前预览动作; 之后切换预览动作/改握持键, frame_change_post
      回调按指纹自动重算 (回调逻辑随 addon 常驻, 重开 blend 不用重跑)。
按钮: 3D 视图侧栏 (N 键) > "Valor 工具" 标签:
        立即同步   — 按当前 active action 手动重算一次
        清扫全库旧键 — 遍历所有动作清除 GRIP influence 旧键 (平时不需要,
                       swo022 只播 FK_grip_preview_auto, 旧键不会生效)
        静态左手   — 源动作无 030/047 曲线时的兜底状态一键复位

规则回顾:
    047.scaleY==2 -> 右手插座 (双 2 时 047 优先); 030.scaleY==2 -> 左手;
    都为 1 (闲置) -> 保持上一帧状态 (游戏"不变表"语义);
    写 swo022 MhBone_000 的 GRIP_L_030/GRIP_R_047 influence CONSTANT 瞬切键。
    读键走 keyframe_points 原始 co, 不用 fcurve.evaluate() (LIMITS 会钳位)。
"""
import bisect
import math

import bpy

bl_info = {
    'name': 'Valor grip preview sync',
    'author': 'MHW-arts pipeline',
    'version': (1, 0, 0),
    'blender': (4, 3, 0),
    'location': 'View3D > Sidebar (N) > Valor 工具',
    'description': 'swo022 武器握持预览自动同步 (GRIP_L_030/GRIP_R_047)',
    'category': 'Animation',
}

# ---------------- 配置 (与 11_sync 一致) ----------------
PLAYER_OBJ = 'm_mesh Armature'
WEAPON_OBJ = 'swo022 Armature'
WEAPON_BONE = 'MhBone_000'
SOCKET_BONES = {'L': 'MhBone_030', 'R': 'MhBone_047'}
CONSTRAINT_NAMES = {'L': 'GRIP_L_030', 'R': 'GRIP_R_047'}
SCALE_PATHS = {s: 'pose.bones["%s"].scale' % b for s, b in SOCKET_BONES.items()}
INFL_PATHS = {s: 'pose.bones["%s"].constraints["%s"].influence' % (WEAPON_BONE, CONSTRAINT_NAMES[s])
              for s in SOCKET_BONES}
PREVIEW_ACT_NAME = 'FK_grip_preview_auto'
DEFAULT_SOCKET = 'L'
ACTIVE_THRESHOLD = 1.5

_state = {'fp': object()}   # 哨兵: 保证第一次 tick 必刷


def _log(msg):
    print('[grip-sync] ' + msg)


# ---------------- 源动作读取 (与 11_sync 相同逻辑) ----------------
def raw_keys(fcurve):
    keys = {}
    for kp in fcurve.keyframe_points:
        keys[kp.co[0]] = kp.co[1]
    return sorted(keys.items())


def make_sampler(keys):
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
        return v0 + (v1 - v0) * t
    return sample


def grip_segments(act):
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
            s = cur
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
    fc.keyframe_points.clear()
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
        _log('已清除 %d 条旧 GRIP influence 键' % removed)
    return removed


def sync_now(override=None, report=None):
    player = bpy.data.objects.get(PLAYER_OBJ)
    weapon = bpy.data.objects.get(WEAPON_OBJ)
    if player is None or weapon is None:
        msg = '找不到 %s / %s (本 blend 不是 MHW-Valor 工作文件?)' % (PLAYER_OBJ, WEAPON_OBJ)
        if report:
            report({'WARNING'}, msg)
        _log(msg)
        return False
    if override:
        act = bpy.data.actions.get(override)
        if act is None:
            msg = '找不到动作 %r' % override
            if report:
                report({'WARNING'}, msg)
            _log(msg)
            return False
    else:
        ad = player.animation_data
        act = ad.action if ad else None
    _state['fp'] = grip_fingerprint(act)
    rebuild(weapon, player, act)
    if report:
        report({'INFO'}, '握持预览已按 %s 重算' % (act.name if act else '无动作'))
    return True


# ---------------- 帧回调 ----------------
def grip_frame_handler(scene, depsgraph=None):
    try:
        player = bpy.data.objects.get(PLAYER_OBJ)
        weapon = bpy.data.objects.get(WEAPON_OBJ)
        if player is None or weapon is None:
            return  # 其他 blend / 临时文件: 静默待命, 不报错
        ad = player.animation_data
        act = ad.action if ad else None
        fp = grip_fingerprint(act)
        if fp != _state['fp']:
            _state['fp'] = fp
            rebuild(weapon, player, act)
    except Exception as e:
        _log('handler 异常: %r' % (e,))


# ---------------- Operators / Panel ----------------
class GRIPSYNC_OT_sync_now(bpy.types.Operator):
    bl_idname = 'gripsync.sync_now'
    bl_label = '立即同步握持'
    bl_description = '按玩家骨架当前 active action 重算 swo022 握持 influence'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        sync_now(None, self.report)
        return {'FINISHED'}


class GRIPSYNC_OT_clean_all(bpy.types.Operator):
    bl_idname = 'gripsync.clean_all'
    bl_label = '清扫全库旧 GRIP 键'
    bl_description = ('遍历所有动作清除 GRIP influence 旧键 (手搓预览残留)。'
                      '平时不需要: swo022 只播 FK_grip_preview_auto, 旧键不会生效')
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        n = neutralize_legacy_influence()
        self.report({'INFO'}, '已清除 %d 条旧 GRIP influence 键' % n)
        return {'FINISHED'}


class GRIPSYNC_PT_panel(bpy.types.Panel):
    bl_label = 'Valor 握持同步'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Valor 工具'

    def draw(self, context):
        col = self.layout.column()
        have_rig = (bpy.data.objects.get(PLAYER_OBJ) and bpy.data.objects.get(WEAPON_OBJ))
        col.enabled = bool(have_rig)
        col.operator(GRIPSYNC_OT_sync_now.bl_idname, icon='FILE_REFRESH')
        col.operator(GRIPSYNC_OT_clean_all.bl_idname, icon='BRUSH_DATA')
        if not have_rig:
            col.label(text='本文件无 m_mesh/swo022 骨架', icon='INFO')


_classes = (GRIPSYNC_OT_sync_now, GRIPSYNC_OT_clean_all, GRIPSYNC_PT_panel)


@bpy.app.handlers.persistent
def _on_blendfile_load_post(_none, _other_none):
    """register() 处于 _RestrictData 受限上下文碰不了 bpy.data, 每个文件的
    初始同步放在 load_post 里做 (官方 addon 惯例, 参考 copy_global_transform)。"""
    try:
        if bpy.data.objects.get(PLAYER_OBJ):
            sync_now(None)
        else:
            _state['fp'] = object()  # 无 rig 的 blend: 重置哨兵待命
    except Exception as e:
        _log('load_post 初始同步失败: %r' % (e,))


def register():
    for c in _classes:
        bpy.utils.register_class(c)
    h = bpy.app.handlers.frame_change_post
    if grip_frame_handler not in h:
        h.append(grip_frame_handler)
    lp = bpy.app.handlers.load_post
    if _on_blendfile_load_post not in lp:
        lp.append(_on_blendfile_load_post)
    _log('addon 已启用: frame_change_post 回调常驻 (切换预览动作自动重算)')


def unregister():
    h = bpy.app.handlers.frame_change_post
    if grip_frame_handler in h:
        h.remove(grip_frame_handler)
    lp = bpy.app.handlers.load_post
    if _on_blendfile_load_post in lp:
        lp.remove(_on_blendfile_load_post)
    for c in reversed(_classes):
        bpy.utils.unregister_class(c)
    _log('addon 已停用')


if __name__ == '__main__':
    register()
