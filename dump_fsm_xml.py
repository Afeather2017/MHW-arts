# Dump a cFSMPl XML export (XFsm) into a human node/link map.
# Usage: python dump_fsm_xml.py [wp03_action.xml] > wp03_fsm_map.md
import xml.etree.ElementTree as ET
import sys, io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

path = sys.argv[1] if len(sys.argv) > 1 else r'E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\wp03_action.xml'
tree = ET.parse(path)
root = tree.getroot()

NOTES = {
 '45':'气刃突刺 (LMT170)。入边来自几乎所有地面招式/见切/居合链',
 '55':'气刃兜割起手·跳起 (LMT171)',
 '56':'兜割下落段 (LMT172)',
 '57':'必殺着地 = 兜割攻击段 (LMT174)',
 '58':'必殺空中攻撃 (空中接敌)',
 '64':'居合斬り = 小居合 (LMT307)。特殊纳刀后拔刀斩',
 '65':'気刃居合斬り(大居合)。唯一出边→中转节点76→87',
 '61':'居合斬り開始 = 状态标记 (act=-10, 非真实动作)',
 '28':'見切り斬り (LMT140)',
}

def links_of(n):
    out=[]
    ll=n.find('./array[@name="mpLinkList"]')
    if ll is not None:
        for l in ll.findall('./classref'):
            dst=l.find('./u32[@name="mDestinationNodeId"]').get('value')
            nm=l.find('./string[@name="mName"]')
            cond=l.find('./u32[@name="mConditionId"]').get('value')
            out.append((dst,nm.get('value') if nm is not None else '?',cond))
    return out

def ano_of(n):
    e=n.find('./array[@name="mpProcessList"]/classref/classref[@name="mpParameter"]/class[@name="ActionNo"]/s32[@name="EnumValue"]')
    return e.get('value') if e is not None else None

nodes={}
order=[]
for n in root.findall('./classref[@name="mpRootCluster"]/array[@name="mpNodeList"]/classref'):
    nid=n.find('./u32[@name="mId"]').get('value')
    name=n.find('./string[@name="mName"]').get('value')
    nodes[nid]=(name,ano_of(n),links_of(n))
    order.append(nid)

inb={}
for nid in order:
    for dst,_,_ in nodes[nid][2]:
        inb.setdefault(dst,[]).append(nid)

def nm(i):
    a=nodes.get(i)
    return f'{a[0]}(act={a[1] or "-"})' if a else '??'

def kind(nid):
    name,ano,_=nodes[nid]
    if ano is None: return '中转节点(无动作,只做路由)'
    v=int(ano)
    if v>=1000: return '系统状态(不走DoAction,不进动作日志)'
    if v<0: return '标记节点(负act,非真实动作)'
    return '动作节点'

print(f'# wp03_action FSM 节点地图\n')
print(f'- 来源: `{path}` (与 nativePC 部署版 md5 一致: 374396775f304208c1aa0ffededcd6fd)')
print('- 节点名 ≠ 招式名: 必殺攻撃=87气刃突刺, 必殺駆け上がり=89兜割起手。识别节点请看 ActionNo,不是名字。')
print(f'- 共 {len(order)} 节点, 全部在根 cluster (无子 cluster)。\n')
print('## 节点总表\n')
print('| id | ActionNo | 节点名 | 入边 | 出边 | 类型/备注 |')
print('|---:|---:|---|---:|---:|---|')
for nid in order:
    name,ano,links=nodes[nid]
    n_in=len(inb.get(nid,[]))
    note=NOTES.get(nid,'')
    print(f'| {nid} | {ano or "-"} | {name} | {n_in} | {len(links)} | {kind(nid)}{("；"+note) if note else ""} |')

print('\n## 兜割(登龙)链\n')
print('```')
print('必殺攻撃(87) --cond158--> 必殺駆け上がり(89) --cond159--> 必殺駆け上がりジャンプ(90)')
print('   90 --cond183--> 必殺空中攻撃(91) --cond185--> 必殺着地(92)')
print('   90 --cond184--------------------------------^')
print('87 的其它出边: ->見切り(86,cond198) ->右移動斬り(83,cond200) ->左移動斬り(82,cond201) ->斬り下がり(84,cond202)')
print('```\n')
print('## 87(气刃突刺) 的进入路线\n')
for src in inb.get('45',[]):
    for dst,lname,cond in nodes[src][2]:
        if dst=='45':
            print(f'- {nm(src)} --"{lname}" cond={cond}--> 87')
print()
print('## 居合系路线\n')
print('```')
for k in ('64','65'):
    print(f'{nm(k)}:')
    for dst,lname,cond in nodes[k][2]:
        tgt=nm(dst)
        sub=''
        if nodes[dst][1] is None:  # junction: follow one hop
            outs=nodes[dst][2]
            sub=' -> '+' / '.join(f'{nm(d)}(cond{c})' for d,_,c in outs)
        print(f'  --cond{cond}--> {tgt}{sub}')
print('```\n')
print('## 系统状态节点 (1000+, 永不出现在 DoAction 捕获日志)\n')
print('| ActionNo | 节点 | 含义推断 |')
print('|---:|---|---|')
for nid in order:
    name,ano,_=nodes[nid]
    if ano and ano.isdigit() and int(ano)>=1000:
        print(f'| {ano} | {name} | {NOTES.get(nid,"-")} |')
print('\n(捕获工具 hook 的是动作请求路径 ActionController::DoAction,即改派器写入的 {set,id};')
print(' 这些节点由 FSM 状态切换进入,不发起动作请求,因此永远不会被记录。)')
