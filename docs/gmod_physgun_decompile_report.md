# GMod 物理枪全量反编译报告（2026-10-09）

反编译归档：`D:\tmp\dec\server.dll\`（88 函数）+ `D:\tmp\dec\client.dll\`（127 函数），
全部按 AGENTS 规范（`<DLL>\<符号名>.c` + `_index.txt`），证据与本文一一对应。
工具：Ghidra headless（proj=suit_server / projg=client64）+ `_gmod_rtti/vtdump/physgun_*` 脚本。

---

## 1. 类结构总览

| 类 | DLL | RTTI | vtable | 类大小 | 说明 |
|---|---|---|---|---|---|
| CWeaponPhysGun | server | `.?AVCWeaponPhysGun@@` | 0x1808bb378（340 槽） | 0x1910 | 武器本体 |
| CPhysBeam | server | `.?AVCPhysBeam@@` | 0x1808ba8c8 | 0x1578 | 光束实体，classname `physgun_beam` |
| CPhysGunControllerPoint | server | `.?AVCPhysGunControllerPoint@@` | 0x1808bb1c8（单槽=IMotionEvent::Simulate） | 0x38 | 运动控制器，**内嵌武器 +0x18b0** |
| C_WeaponPhysGun | client | `.?AVC_WeaponPhysGun@@` | 0x180665208 主表（430 槽） | - | 客户端镜像（注意 C_ 前缀） |
| C_PhysBeam | client | `.?AVC_PhysBeam@@` | 204 槽 + 3 secondary | - | 继承 C_Beam |

武器成员偏移表（server，基址=this）：
`+0x1728/0x1730` NextPrimary/SecondaryAttack(double) · `+0x1878` m_hTarget · `+0x187c` m_hBeam ·
`+0x1880` 抓取偏移（无旋转=hit-origin，有旋转向量=VectorIRotate 局部） · `+0x188c` 持距 ·
`+0x1890` Reload-hook 10ms 闸 · `+0x1894` 上次 yaw · `+0x1898` 目标世界位 ·
`+0x18a4` 目标角（yaw 分量随玩家转身累加于 `+0x18a8`） · `+0x18b0` 内嵌控制器 ·
`+0x18e4` 控制器 target 句柄 · `+0x1908` IPhysicsObject*

---

## 2. Server 端全链路

### 2.1 抓取（vtable 槽 331 PrimaryAttack = 0x180143000 StartGrab）
1. 闸：`m_flNextPrimaryAttack(+0x1728) <= curtime`、owner 是玩家、当前无 target
2. trace：`eye + GetAimDirection() * physgun_maxrange`，**enginetrace TraceRay mask 0x600400f**，
   CTraceFilterSimple 过滤玩家；`DAT_180eaca90+0x68` 条件成立再补一发 hull trace（±(hull+0.1)）
3. 命中非世界实体 → **hook id 77 `PhysgunPickup(ply, ent)` 返回 true 才接受** → 写 `+0x1878`、`+0x1908`
4. 接受后立刻派发 **hook id 78 `OnPhysgunPickup(ply, ent)`**
5. 记录：抓取偏移 `+0x1880`、距离 `+0x188c`、目标 abs 角 `+0x18a4`
6. **NPC/bone 分支**（movetype==6 判定）：GetAttachment 矩阵反变换抓点，physobj/bone 二选一进控制器
7. 控制器 `FUN_180141940` Attach：CreateMotionController + AttachObject + **SetMass(45678.0)**
8. 光束：`m_hBeam(+0x187c)` SetActive(1) + AttachToTarget
9. IN_ATTACK(+0x24ec) 置位时 `vtable+0x930(0xb5)` = SendWeaponAnim(ACT_VM_PRIMARYATTACK=181)

### 2.2 持握（槽 317 ItemPostFrame = 0x180142860，每帧）
- IN_RELOAD(0x2000) → Reload；IN_ATTACK2(0x800) → SecondaryAttack（冻结）
- pressed IN_ATTACK(1) → PrimaryAttack（GMod 物理枪**无 punt**，按一下=重新抓取）
- **IN_USE(0x20) 按下 → 旋转块**：AngleMatrix/VectorRotate 绕视轴，
  灵敏度 = 客户端 convar `physgun_rotation_sensitivity`（GetClientConVarValue，默认 0.05，钳 [-1000,1000]），
  输入取玩家 mouse dx/dy（plVar7[0x51f]+0x34/+0x36 的 short）
- **滚轮 = vtable+0x10a8 × `physgun_wheelspeed`**（默认 10.0，下限 0.1）调 `+0x188c` 持距，
  钳 [physgun_minrange, physgun_maxrange]
- 末尾 `eye + fwd*dist` 写 `+0x1898` → FUN_180144100 + FUN_180143b90

### 2.3 持握移动两个函数
- `FUN_180143b90`（无 physobj = NPC 目标）：**目标点扫掠门控**——CTraceFilterEntity + TraceRay
  **mask 0x400B**，6 连 trace（原地探针、目标点探针、origin→target、三轴分量分离各一发），
  通则 SetAbsOrigin(0x1802e2be0) —— 与 fork 已移植的 HL2SB_PhysgunGateTarget 同构
- `FUN_180144100`：yaw 变化累加进 `+0x18a8`（物体随玩家转身）；
  USE+SPEED 按下/松开组合时按 `gm_snapangles` 量化旋转（非 physobj 目标走 SetAbsAngles 0x1802e2820）

### 2.4 冻结（槽 332 SecondaryAttack = 0x180143990）
1. 闸：`m_flNextSecondaryAttack(+0x1730) = curtime + 0.5`，同时回写 NextPrimaryAttack
2. 清孤儿 physobj
3. **hook id 70 `OnPhysgunFreeze(weapon, physobj, ent, ply)` 4 参派发——冻结本体完全在 Lua**
   （GMod base player.lua：EnableMotion(false) + AddFrozenPhysicsObject）
4. SendWeaponAnim(0xB5) → DropHeld
5. **服务端全程零 EmitSound（冻结无声实锤）**

### 2.5 丢下（FUN_1801424e0 DropHeld，Holster/UpdateOnRemove/slot44/冻结 共用）
1. **hook id 76 `PhysgunDrop(ply, ent)`**
2. beam SetActive(0) → 控制器 Detach（质量恢复；`IsHeldByAnyPlayer`(0x1806b7ec0) 查全玩家
   physcannon+physgun 都不持才清 EFlags bit4）
3. player vtable+0x758 通知 → 清 m_hTarget/m_vecGrabOffset

### 2.6 生命周期
- **Deploy（槽 306）就 CreateBeam**——光束实体是武器级常驻（不是抓取时才建），Holster(284)/
  UpdateOnRemove(107)/槽 307 时 RemoveBeam；Equip 类槽 44（持枪人变更）= DropHeld+CreateBeam
- Deploy 特判（0x1806b49c0）：玩家 +0x2dda 且 SelectWeightedSequence(0xCC)!=-1 → 自管动画，否则基类
- IdleAnim（槽 320）：SendWeaponAnim(0xAE=174) / 0xCC=204 二态（时间闸 +0x186c）
- Reload（槽 328）：`+0x1890 = curtime+0.01` 的 10ms 闸 → **hook id 71 `OnPhysgunReload(weapon, ply)`**

### 2.7 CPhysBeam（server）
- 字段：`+0x1538` attachIdx · `+0x153c` m_vecTargetPos · `+0x1548` EHANDLE m_hTarget ·
  `+0x154c` byte active · `+0x1568` 端点（DT_PhysBeam 网络化）；武器侧 SendProp `m_hPhysBeam`
- Create(0x1801414e0) = CreateEntityByName("physgun_beam") + 无固体 + OBB ±64 + DispatchSpawn
- Update(0x180141030)：target 是玩家 → 从其 eye 做 **physgun_maxrange trace mask 0x600400b** 定端点；
  有 target handle → 端点 = 目标 origin

### 2.8 CPhysGunControllerPoint（IMotionEvent，Simulate=0x180141f60）
- 字段：`+0x08` IPhysicsObject* · `+0x1c` targetPosition · `+0x28` targetRotation · `+0x34` EHANDLE target ·
  `+0x3c` savedMass · `+0x40` maxAngular · `+0x44` **IsVehicle（classname=="prop_vehicle_jeep" 字符串比对！）** ·
  `+0x48` EHANDLE player · `+0x50` motion controller
- Simulate：读 6 个 physgun_* convar 后 ComputeShadowControl；
  **mass < 45678.0 且非载具/APC → SetMass(45678)，detach 恢复；载具 savedMass=-1**（与 fork"载具可冻结无特判"结论吻合）

### 2.9 convar 全表（注册桩 0x1800084xx-0x180008a51）
| convar | 默认 | 备注 |
|---|---|---|
| physgun_maxrange | 4096 | trace 距离上限 |
| physgun_minrange | 40 | |
| physgun_maxAngular | 5000 | 控制器 |
| physgun_maxAngularDamping | 10000 | |
| physgun_maxSpeed | 5000 | |
| physgun_maxSpeedDamping | 10000 | |
| physgun_DampingFactor | 0.8 | |
| physgun_teleportDistance | 0 | |
| physgun_timeToArrive | 0.05 | |
| physgun_timeToArriveRagdoll | 0.1 | |
| physgun_drawbeams | 1 | client 侧 |
| gm_snapgrid / gm_snapangles / physgun_rotation_sensitivity / physgun_wheelspeed | 0..4096 / ±360 / 0.05 / 10.0 | **玩家级**，server 经 GetClientConVarValue 读 |

### 2.10 Lua hook id 表（server 派发点，id=lua_shared 名称表下标）
19=DrawPhysgunBeam · 40-43=GravGunOnDropped/OnPickedUp/PickupAllowed/Punt（physcannon 族）·
64=OnCrazyPhysics · **70=OnPhysgunFreeze · 71=OnPhysgunReload · 76=PhysgunDrop · 77=PhysgunPickup · 78=OnPhysgunPickup**

---

## 3. Client 端全链路

### 3.1 网络字段（DT 表）
- `DT_WeaponPhysGun` = **m_hGrabbedEntity@0x1CA8 + m_vHitPosLocal@0x1CB0 + m_hPhysBeam@0x1CAC**
- C_PhysBeam 复用共享 **DT_Beam**（25 属性：m_hAttachEntity[10]/m_vecEndPos@0x174C/...），
  **DT_Beam 没有 m_hTargetEntity**（该字符串唯一消费者是 DT_EnvProjectedTexture，已存档排除）——
  physbeam 靠 **moveparent 绑定 owner 玩家**

### 3.2 动画状态机（client）
- StartGrab（槽 312=0x1800AB3A0）尾部 `+0x890(slot274=SendWeaponAnim)(0xB5)`
- ReturnToIdleThink（槽 300=0x180357080）：`IsViewModelSequenceFinished` 后——
  未持物 → SendWeaponAnim(**0xAE=174**)；持物且活动 ∈ {0xCB, 0xCC, 2} → SendWeaponAnim(**0xCC=204**)
- 槽 400/401：检查活动 0xCD/0xCC 是否有序列 → 置 flag+0x1C98 / +0x1C9C=now+0.5
- 槽 313：punt 后掉落 SendWeaponAnim(0xB5)+清理（对应 SWEP 覆写场景）
- 视模型：本地玩家 = `models/weapons/c_superphyscannon.mdl`（GiveViewModels 写 2），其他人 = 脚本 vm（写 0）

### 3.3 滚轮的真实通道（重要）
**client 槽 389（0x1800AB1C0）**：MOUSE_WHEEL_UP=112 → usercmd 置 **0x100000（IN_WEAPON1，1<<20）**、
MOUSE_WHEEL_DOWN=113 → **0x200000（IN_WEAPON2，1<<21）**，持物时消费（返回 0）。
滚轮距离/旋转的数值消费**全在 server**（ItemPostFrame 读这些位）；`physgun_wheelspeed`/
`physgun_rotation_sensitivity` 在 client 只是注册+NaN 回退校验。

### 3.4 枪口灯真相（**四轮终审，结案**）：**GMod 物理枪没有任何专用动态光**。"双 CLightHandle 枪口灯"是把休眠引擎代码 C_DynamicLight::ClientThink（0x1801CB700）误读为物理枪代码后的连环虚构——该实体在物理枪使用期间根本不存在：client.dll 无 "light_dynamic"/"dynamic_light" 字符串（无法按类名创建）、server.dll 零 CreateEntityByName + CDynamicLight RTTI 零代码引用、65 个 CLightHandle 分配点穷举完毕与物理枪零交集（唯一武器相关=共享枪口闪光灯 0x1801A25F0，橙色硬编码、muzzleflash_light 门控、物理枪不触发）、"武器色→dlight" 代码路径（call [reg+0xC40] 与 65 分配点）零交集。完整证据链：`PHYGUN_LIGHT.final_verdict.txt`。**物理枪客户端视觉足迹终版**：① C_PhysBeam 贝塞尔缎带（武器色、两端黑淡出）+ physbeama/physbeam 材质 pass + 目标端 2/7 精灵簇（physg_glow1/2）；② 视模型 c_superphyscannon（slot16/32 状态机）+ RECOIL1 持握 idle；③ 滚轮 IN_WEAPON1/2 位 + 视角冻结（slot387 + CInput.MouseApply 回写）；④ 持物辉光只能来自 Lua（halo）或精灵加法混合，**不是 dlight**。
- **主光**（key=entindex）：色 ColorRGBExp32@+0x169C、半径+0x16A4、origin=render origin、
  **die=now+1000000（常亮）**，radius>0 时 flags|=1；`+0x1698` bit1 时熄灭（die=now）
- **副光**（key=**-entindex**）：主光激活时沿 muzzle→目标扫掠，engine TraceLine **mask 0x2000B**，
  颜色 = 武器色字节 × (1-fraction)²（fraction@+0xCC），flags=(bits&0xC)|2

### 3.5 精灵簇（0x1800AA4B0，DrawModel 两处调用）
- **空闲：3 张**，alpha=**RandomInt(120,255)**，size=**sin(curtime*5.0+i*15.0)*8.0+48.0**，pos+=dir*5.0
- **持物：7 张**（未持物时另一路 2 张），alpha=**RandomInt(100,255)**，size=**RandomFloat(16.0,64.0)**，pos+=dir*2.0
- 色 = owner(+0x744) GetRenderColor（无 owner=0xFFFFFF）；材质 bind **physbeama**（簇本体）/
  **physbeam**（目标端 glow pass，FUN_180181840 = 相机朝向 billboard quad）
- ⚠️ **修正旧记录**：AGENTS 里"持物 7 张尺寸 100、alpha 255"有误——100/255 是 RandomInt 的下限/上限

### 3.6 光束渲染的真相（修正认知）
- OnDataChanged（0x1800AA900）惰性缓存 4 资源：glow1/glow2 材质@+0x16A0/+0x16A8、
  engine beam 句柄 `sprites/physbeam.vmt`@+0x16B0、`sprites/physbeama.vmt`@+0x16B8
- **engine beam 原语通道（FUN_180182C00）选 +0x16B8/0x16C0 两槽——全 .text 扫描证实恒 NULL、
  从无写入 → engine beam 原语实际从不渲染**
- 可见光束 = DrawModel（0x1800A9440）自绘：门控 = r_drawviewmodel 类全局 + 本地玩家 +
  **owner 活动武器类名必须是 "weapon_physgun"** + `physgun_drawbeams`
- 端点 = ComputeBeamEnd(0x1800A9070，trace mask 0x600400B)
- 持物画 **3 条贝塞尔缎带**（0x1800AA080：宽 2.0 / RandomFloat(2,5) / RandomFloat(2,6)，
  17 点二次贝塞尔，宽随 len*t*0.02 收窄）+ 2/7 精灵簇；空闲只画视模型 attachment 的 3 精灵簇

### 3.7 ghost 预览：**不存在**
client.dll 全量无 "ghost" 字符串，物理枪悬停/抓取的全部视觉 = 双 dlight + 贝塞尔缎带 + 精灵簇。
（半透明 ghost 预览是别的游戏/插件的印象，GMod 本体没有。）

---

## 4. 常量速查（重构对照）

| 项 | 值 | 出处 |
|---|---|---|
| 抓取 trace mask | 0x600400f | slot331 |
| 持握/NPC 扫掠 mask | 0x400B | FUN_180143b90 |
| beam 端点 mask | 0x600400B（server Update）/ 0x2000B（client 副光+ComputeBeamEnd） | 两端 |
| 抓距范围 | 40 ~ 4096（min/max convar） | slot317 |
| 滚轮步长 | wheelspeed(10.0) × vtable+0x10a8，下限 0.1 | slot317 |
| 旋转灵敏度 | 0.05（0x3d4ccccd），钳 ±1000 | slot317 |
| 冻结冷却 | curtime+0.5，同写 NextPrimaryAttack | slot332 |
| 控制器强设质量 | 45678.0（0x47326E00），载具 savedMass=-1 | Simulate |
| 抓取动画 | ACT_VM_PRIMARYATTACK=0xB5(181) | slot331/332 |
| idle/持物动画 | 0xAE=174 / 0xCC=204，序列集 {0xCB,0xCC,2} | slot320/300 |
| 滚轮位 | IN_WEAPON1=1<<20 / IN_WEAPON2=1<<21 | client slot389 |
| 缎带 | 3 条，宽 2.0 / RF(2,5) / RF(2,6)，17 点，宽×(1-t*0.02·len) | DrawModel |

---

## 5. 对 fork 重构的直接结论

> ⚠️ **第二轮修正（2026-10-09 晚）覆盖本节部分内容**：① 按钮双字 +0x24E8=m_nButtons（按住）/+0x24EC=Pressed/+0x24F0=Released（第一轮读反）——slot317 主闸 = `m_nButtons&(IN_ATTACK|IN_ATTACK2)==IN_ATTACK` 每帧跑；② `phys_spinspeed`(200) 是 **GMod 原版 convar**（E+LMB 按住时 A/D 转物体），不是 fork 自创；③ "hull trace 补发"实为 `r_visualizetraces` 调试画线，不存在；④ 抓取 Ray delta = **2×maxrange×dir**（默认实际可达 8192），CPhysBeam 端点 = maxrange×0.5（2048）——两端不对称；⑤ 滚轮 server 消费 = `CBasePlayer::GetMouseWheel()`（player+0x2ce0 累加器），+0x10a0 恒真桩/+0x758 空桩；⑥ 字段定名 +0x1878=m_hGrabbedEntity / +0x187c=m_hPhysBeam / +0x1880=**m_vHitPosLocal**（网络 prop）；⑦ 基类 = **CBaseHL2MPCombatWeapon**；⑧ ACT 定名 203=LOWERED_TO_IDLE / 204=RECOIL1（持物 idle 爪脉动）/ 205=RECOIL2；⑨ trace_t+0x54 = physicsbone；⑩ 新发现姊妹子系统 **player_pickup**（+use 拾取携带，hook 177=OnPlayerPhysicsPickup/178=OnPlayerPhysicsDrop，唯一 "Weapon_Physgun.Off" 声音串属于它）；⑪ **E 转物体时视角冻结在 CInput.MouseApply + 武器虚槽 387**（m_hGrabbedEntity 有效 && IN_USE+聚焦）：视角回写鼠标前原值、cmd.mousedx/mousedy 照常上行——fork 的 in_mouse 补丁必须保证 delta 进 cmd。⑫ 物理枪双端零音效终审定案。详见 COMPARISON.md 顶部修正节。

1. **GMod 物理枪没有 punt**（左键=抓/重抓；GravGunPunt 钩子族属于 HL2 physcannon 派发路径，供 SWEP 用）。
   fork 若有"左键点按发射"行为即与 GMod 不符，重构时删除或仅留给钩子。
2. **冻结 = hook OnPhysgunFreeze 后 SendWeaponAnim + DropHeld**；EnableMotion(false) 在 Lua 侧。
   fork 现行"hook + EnableMotion(false) 留在 C++"需对照——DropHeld 必须跟在冻结后。
3. **beam 实体是武器级常驻**（Deploy 建 / Holster+UpdateOnRemove 删 / SetActive 切换亮灭），
   fork 如果按"抓取时建"处理要改。
4. **engine beam 原语恒不渲染**——可见光束全是客户端自绘贝塞尔缎带+精灵簇。
   fork 不必给 beam 实体做 TE 渲染，直接客户端画。
5. **滚轮 = usercmd 的 IN_WEAPON1/2 位**，服务端统一消费——fork 若在 client 直接改持距是语义偏差
   （预测/回滚一致性差）。
6. 玩家级 convar（snapgrid/snapangles/rotation_sensitivity/wheelspeed）server 走
   GetClientConVarValue——fork 已有同款机制（GetInfo 修复线）。
7. 精灵簇公式按 §3.5 修正版实现；AGENTS 旧记录已过时。
8. NPC 目标（无 vphysics）走 6 连 trace 扫掠门控 + SetAbsOrigin，fork 的 HL2SB_PhysgunGateTarget 同构保留。
9. 控制器强设 mass=45678、载具（classname=="prop_vehicle_jeep"）特判 savedMass=-1——载具判定是
   **字符串比对**不是接口检查。
10. 声音：server 侧物理枪零 EmitSound（无抓取/冻结音），全部声音若 GMod 有的话在 client/共用文件——
    重构时别在 server 加冻结音。
