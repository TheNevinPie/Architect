# Architect — LeviLamina Feasibility Report

**Project:** Architect — a Litematica-inspired schematic blueprint mod for Minecraft Bedrock (Windows client), built as a native LeviLamina mod in C++20.
**Type:** Feasibility audit. No production code was written or modified for this investigation.
**Date:** 2026-10-12. **Status:** No acceptance tests executed; all tests in §10 are proposed.

---

## 1. Executive verdict

**Feasible with significant custom engine work.**

Split by subsystem:

| Subsystem | Verdict |
|---|---|
| Schematic parsing (`.litematic`, `.schem`, `.mcstructure`) | **Feasible** — formats are documented/implemented in open source; one small NBT+gzip dependency needed |
| Internal block model + placement/transform math | **Feasible** — game ships reusable helpers (`BlockPos::transform`, `VanillaBlockStateTransformUtils`, `Rotation`/`Mirror` enums) |
| Client world reading + verifier (missing/correct/incorrect) | **Feasible** — `BlockSource::getBlock` + layers + chunk-gating all exist; thread marshaling required |
| Hotkeys, chat feedback, config, lifecycle cleanup | **Feasible** — `ll::input::KeyRegistry`, `displayClientMessage`, `ClientJoin/ExitLevelEvent` all verified |
| World-space ghost rendering | **Feasible with custom engine work** — no public render-stage API exists; requires a DirectX `Present`-hook overlay (proven pattern) first, in-pipeline geometry later |
| Native block-model ghosts, auto-paste, server-side placement | **Not practical for MVP** — fragile, version-sensitive, or blocked by server authority |

**Recommendation:** proceed to an MVP scoped to *parse → verify → guide* (no auto-place), gated on one render proof-of-concept (Phase 1 in §6.4). Details in §11.

---

## 2. Version and environment (as discovered, not assumed)

| Item | Exact value | Source |
|---|---|---|
| Repo / mod | `pocket-on-pc`, xmake target `pocket-on-pc`, `target_type` default `client` | `xmake.lua:5-9,22`, `src/mod/` (`MyMod.h/.cpp`, `ReachHook.cpp`, `MemoryOperators.cpp`) |
| LeviLamina headers actually compiled against | **26.51.6**, `bedrock-runtime-data v26.51.1-client.7` | CI client build log, run `38156298991`, job `114519868064` (2026-10-11): `levilamina\26.51.6\source\...`, `version = "v26.51.1-client.7"` |
| LeviLamina pin in `xmake.lua` | **Unpinned** (`add_requires("levilamina", …)`) — floats to latest at build time | `xmake.lua:14` |
| LeviLamina pin in `tooth.json` (lip) | `26.20.*` / `26.20.*#client` — **STALE, mismatches the 26.51.6 build** | `tooth.json` |
| Game (Bedrock client) | Release compatible with LeviLamina client **26.51.1** (≈ Bedrock 26.51, Sep–Oct 2026) | `lamina.levimc.org/versions/` table |
| Launcher | LeviLauncher (Windows, GDK) + `lip` packages | `lamina.levimc.org/user_guides/install_on_client/` |
| Toolchain | C++20, `clang-cl`, `/EHa`, `set_exceptions("none")`, `MD` runtime; CI `windows-latest` + LLVM 22 | `xmake.lua:27-42`, `.github/workflows/build.yml` |
| Upstream headers researched against | `LiteLDev/LeviLamina@main` (≈ 26.51.x era) — consistent with the 26.51.6 build headers | `raw.githubusercontent.com`, `api.github.com` contents API, `lamina.levimc.org/api` |
| Existing hook precedent | `LL_AUTO_TYPE_INSTANCE_HOOK(PocketReachPickRangeHook, …, GameMode, &GameMode::$getPickRange, …)` | `src/mod/ReachHook.cpp:15-25` |

**Action item before Architect work starts:** pin `xmake.lua` to an explicit LeviLamina version (e.g. `26.51.*`) and bump `tooth.json` off `26.20.*`, or every future build silently moves the header baseline out from under the code.

---

## 3. Feature feasibility matrix

### A. Schematic loading

| Feature | Classification | Evidence / limitation |
|---|---|---|
| `.litematic` read (primary) | **Feasible** | No official spec (author: "there isn't really any proper documentation"), but format is fully determined from `LitematicaSchematic.java` (branch `fabric/1.21.1-masa`) + clean-room `SmylerMC/litemapy`. gzip + big-endian NBT; `MinecraftDataVersion`, `Version` (accept 1–7, current 7/sub 1), `Regions` compound; signed `Size`; palette list with air always id 0; `BlockStates` long-array, bits/entry = max(2, ⌈log2(palette)⌉), length ⌈volume·bits/64⌉, straddling entries; index `y·(W·L)+z·W+x`; region-relative tile entities; pending ticks gated on Version ≥3/≥5. Reject v1 and Version >7 with clear errors. |
| `.schem` v2/v3 read | **Feasible** | Authoritative spec repo `SpongePowered/Schematic-Specification` (`versions/schematic-3.md`, `-2.md`). gzip + BE NBT. v2 root-is-schematic; v3 nests under `"Schematic"` with `Blocks:{Palette,Data,BlockEntities}` + `Biomes`. Palette `{state-string: int}`; `BlockData` varint byte array, count = W·H·L; index `x+z·W+y·W·L`. Decoders: WorldEdit `SpongeSchematicV2/V3Reader.java`. Support v3 first, v2 falls out of the same decoder. |
| `.mcstructure` read (bonus) | **Feasible** | `wiki.bedrock.dev/nbt/mcstructure` (+ gist `tryashtar/87ad…`). **Raw uncompressed little-endian NBT**; `format_version=1`, `size[3]`, `structure{block_indices[2 layers], entities, palette{default{block_palette, block_position_data}}}`; `-1` = leave-existing; palette entries `{name, states (typed NBT), version≈18168865}`. Zero ID-mapping cost. **Unresolved:** flat-index axis order conflict (wiki/gist/crate say z-fastest, one editor page implies y-fastest) — resolve with a 2×2×2 empirical file before freezing the decoder. |
| `.schematic` legacy read | **Not practical for MVP** | `Width/Height/Length` shorts, `Blocks`+`AddBlocks`+`Data`, numeric pre-flattening IDs with no `DataVersion` → version-ambiguous mapping tables, 4096 cap, no post-1.13 states. Modern tools emit `.schem`/`.litematic`. Revisit only on user demand. (Also noted: `AddBlocks` nibble-order disagreement between MCEdit-family and WorldEdit code — do not implement from memory.) |
| NBT + gzip infrastructure | **Feasible** | Nothing suitable in-project today (`xmake.lua` has only `levilamina`+`levibuildscript`; `zlib` not among LeviLamina deps). Recommendation: vendor header-only `JaderoChan/mcnbt` (BE+LE, `MCNBT_DISABLE_EXCEPTION` fits the no-exceptions build) behind a 3-function facade, plus `add_requires("zlib")` from xmake-repo; fallback `handtruth/nbt-cpp`. `nlohmann/json.hpp` is re-exported public by the levilamina package for the ID-map asset (confirm once via `xmake require --info levilamina`). |
| Java→Bedrock ID/state mapping | **Feasible with custom engine work** (dominant cost) | Three mismatch axes: renames, value domains (`facing` strings ↔ int/direction/bit states, `rotation:0-15`), structural (`waterlogged=true` → layer-1 water entry; `shape` inner/outer has no Bedrock twin). Data source: PrismarineJS `minecraft-data` `blocksJ2B.json` (≈5.2 MB @ bedrock 1.21.60, pin one game version). MVP: prune to a curated ~200–400-entry map embedded as JSON; exact-hit → use; name-hit → allowlisted states only; miss → visible placeholder + counted warning (never silent air). Defer DFU-style `DataVersion` upgrading. |

**Format recommendation for first release:** `.litematic` (primary) + `.mcstructure` (cheap, exact) + `.schem` v3/v2 (shares the NBT layer). Drop `.schematic`.

### B. Placement and transformation

| Feature | Classification | Evidence / limitation |
|---|---|---|
| Anchor + XYZ move | **Feasible** | Pure integer math on `BlockPos`; `BlockPos::transform(Rotation, Mirror, Vec3 const& pivot)` verified verbatim in `src/mc/world/level/BlockPos.h`. Convention: normalize signed litematic `Size` with `abs()`, preserve `Position` offset, transform = rotate-about-pivot → translate-to-anchor. |
| 90° rotation | **Feasible** | `Rotation` enum (`src/mc/util/Rotation.h`), `Mirror` enum (`src/mc/util/Mirror.h`) verified; `BlockPos::transform` reuses game logic. |
| Directional state rotation | **Feasible with custom engine work** | No `Block::rotate` exists — but `VanillaBlockStateTransformUtils::transformBlock(Block, Rotation, Mirror)` (+ `_rotate`/`_mirror` direction cyclers, `hasTransformableConnectionStates`) verified in `src/mc/world/level/block/states/VanillaBlockStateTransformUtils.h`, and `StructureTemplate::_mapToRotation` in `src/mc/world/level/levelgen/structure/StructureTemplate.h`. Strategy: reuse these; stair `shape`/wall/fence connection flags get a second recompute pass later — MVP degrades `shape→straight` (usually visually fine). Mirror flips `hinge`; yaw-invariant states (`half`, slab `type`, `open_bit`) need no work. `rotation:0-15` cycles +4 mod 16 per 90° CW. |
| Mirroring | **Feasible** | Same helpers; Litematica order to mirror: `mirror(main)` → `mirror(sub)` → `rotate(combined)`, with axis swap at ±90° (`LitematicaSchematic.java: placeBlocksToWorld`). |
| Save/restore placements, multiple schematics | **Feasible** | `Mod::getDataDir/getConfigDir` + `ll/api/Config.h` reflection JSON verified (`src/ll/api/mod/Mod.h`, `src/ll/api/Config.h`). Per-world state keyed by `ClientStartJoinLevelEvent` world/server name. |
| In-game structure place (`StructureManager`) | **Feasible with custom engine work, local-only** | `StructureManager::importStructure/tryPlaceStructureInWorld`, `StructureTemplate::fillFromWorld/placeInWorld` verified (`src/mc/world/level/levelgen/structure/`). Assumes `Level`+`LevelStorage`+placement queue — viable for local Creative worlds only, not remote servers. Out of MVP scope by design (no auto-paste). |

### C. World-space rendering (highest priority)

| Approach | Classification | Evidence / limitation |
|---|---|---|
| 1 — Native ghost block models | **Not practical for MVP** | `BlockTessellator` (35 KB header, `src-client/mc/client/renderer/block/`) is a full chunk-pipeline tessellator (lighting, AO, atlas UVs, transparency sorting). No prior art for injecting ghost chunks; fragile across versions. Defer until Approach 2 lands. |
| 2 — Custom wireframe/box overlay, in-pipeline | **Feasible with custom engine work** | Hook target verified present: `LevelRenderer::renderLevel` in `src-client/mc/client/renderer/game/LevelRenderer.h` (Horion hooks exactly this: `Hooks::LevelRenderer_renderLevel → onLevelRender`). Materials exist (`mOutlineSelectionMaterial`, `wireframeMaterial`, selection materials in `LevelRendererPlayer.h`); `Tessellator::{begin,vertex,color,quad,end}` verified in `src-client/mc/client/renderer/Tessellator.h`. **Gaps (UNRESOLVED):** exact `renderLevel` parameter list (doxygen reports `(ScreenContext&, FrameRenderObject const&)` — reverify in PoC), `FrameRenderObject`/`ViewRenderObject` field layout, `MeshContext::renderMesh` signature, client `DebugRenderer` draw surface (opaque `TypedStorage`). |
| 3 — Hybrid (cheap far, detailed near) | **Feasible with custom engine work** | Same infrastructure as 2 + distance-bucketed draw lists. Optimization, not a separate feasibility question. Plan from day one (per-chunk buckets), implement after 2. |
| 4 — HUD-only fallback | **Feasible** | Only verified render API: `Before/AfterUIRenderEvent` (`src-client/ll/api/event/render/` — **the entire directory is 3 files; no world-stage event exists**, verified via contents API). `MinecraftUIRenderContext::{drawRectangle,fillRectangle,drawText}` verified (`src-client/mc/client/renderer/screen/MinecraftUIRenderContext.h`, 2D only). Ship regardless as fallback. |
| 2b — DX `Present`-hook overlay (first 3D step) | **Feasible with custom engine work (lowest 3D risk)** | Proven on this exact stack: `chiyan1/ChiyanMap` (LeviLamina client mod, `Present/Present1`+`ResizeBuffers`+`ExecuteCommandLists` MinHook, ImGui over `D3D11On12`) + BNM DX-hook wiki. Must handle: D3D11-vs-D3D12 per-frame `GetDevice`, `ResizeBuffers[1]` invalidation (GDK ≥1.21.120), render-thread vs game-thread sync (atomics/double-buffered blueprint, mutex), no game-thread-assumed calls on the render thread. Overlay is always-on-top (no depth test) — acceptable for wireframes, not for solid ghosts. |

Matrix access: `GameRenderer::mLastLevelViewMatrix/mLastLevelProjMatrix` members verified present (`src-client/mc/client/renderer/game/GameRenderer.h`) but **no public getter found**; `LevelRendererPlayer::setupCamera(mce::Camera&, float)` + `mWorldSpaceCamera` verified — hook-to-capture is the Horion-analogous route. Exact view/proj getters: **UNRESOLVED**, to be settled by the PoC.

### D. World comparison (verifier)

| Feature | Classification | Evidence / limitation |
|---|---|---|
| Read block at pos | **Feasible** | `BlockSource::getBlock(pos)` + `getBlock(pos, layer)` + `getExtraBlock` + `getLiquidBlock` + `hasBlock` verified (`src/mc/world/level/BlockSource.h`). Reach via `Actor::getDimensionBlockSourceConst()` (`src/mc/world/actor/Actor.h`) or `Dimension::getBlockSourceFromMainChunkSource()` (`src/mc/world/level/dimension/Dimension.h`). Entry: `ll::service::bedrock::getClientInstance()` (`src-client/ll/api/service/TargetedBedrock.h`, file+symbol verified) → `IClientInstance::getLocalPlayer/getLevel` (`src-client/mc/client/game/ClientInstance.h`). **Must marshal to game thread** (`ClientLevelTickEvent` in `src-client/ll/api/event/world/`, or `ClientThreadExecutor` in `src-client/ll/api/thread/`) — `BlockSource` is not thread-safe (`mOwnerThreadID`, chunk cache). No render-thread reads. |
| Name + state compare | **Feasible** | `Block::isAir/getTypeName/getData/getState<T>` + `Block::tryGetFromRegistry` verified (`src/mc/world/level/block/Block.h`); `BlockType::getTypeName/tryGetFromRegistry/getBlockState/hasState`, `mCanBeExtraBlock` verified (`src/mc/world/level/block/BlockType.h`, ex-`BlockLegacy` — old name 404s, do not use). |
| Missing/correct/incorrect/extra/unchecked taxonomy | **Feasible** | Gate every read on `hasChunk/hasChunksAt/areChunksFullyLoaded/hasBlock` (all verified in `BlockSource.h`); unloaded → `unchecked` (never silently `correct`). Air-vs-expect-air → correct; `-1`/void layer (mcstructure) → skip. |
| Fluids / waterlog layers | **Feasible with custom engine work** | Both layers readable (`getBlock(pos,0/1)`, `getExtraBlock`, `getLiquidBlock`) + `Material::{mLiquid,mSolid}` (`src/mc/world/level/material/Material.h`); `BlockType::mCanBeExtraBlock`, `StructureTemplate::isWaterlogged`. No `BlockSource::isWaterlogged` — compare both layers explicitly. Liquid-blocking edge semantics need per-build testing. |
| Block entities | **Feasible with custom engine work** | `BlockSource::getBlockEntity` + `BlockActor::{getPosition,getType,load/save}` verified (`src/mc/world/level/block/actor/BlockActor.h`); `SignBlockActor::getMessage`, `BannerBlockActor::getPatterns`, `ChestBlockActor::getContainer` verified. **Limitation:** client actors exist only after `BlockActorDataPacket` sync; chest contents are server-authoritative and likely unreadable on remote servers — null-check everything, compare sign/banner where synced, skip container contents remotely. |
| Y limits, dimensions | **Feasible** | Never hardcode: `DimensionHeightRange{mMin,mMax}`, `BlockSource::getMin/MaxHeight`, `Dimension::getHeightInSubchunks` verified. (External ref: Overworld −64…319 placeable.) Per-dimension (Nether/End) values queried at runtime. Compass mapping of `east()` impl: **UNRESOLVED** — one in-game check needed. |

### E. Building assistance (post-MVP)

Missing-block highlight, layer filter, next-missing navigation, material counts, placement preview near target: all **Feasible** given C+D (same data, HUD or overlay presentation). Manual-placement assistance using normal game interactions: **Feasible** (no packets forged, no validation bypassed). **Automatic pasting / placement automation: explicitly out of scope** — remote servers are authoritative and this project will not attempt bypasses.

---

## 4. API audit (verified declarations only)

Lifecycle & plumbing — `src/ll/api/mod/Mod.h` (`getModDir/getDataDir/getConfigDir/getWorldDataDir/getWorldConfigDir`), `src/ll/api/mod/NativeMod.h` (`current()`), `src/ll/api/Config.h` (`loadConfig/saveConfig`), `ll/api/memory/Hook.h` (`LL_AUTO_TYPE_INSTANCE_HOOK`, verified pattern in `src/mod/ReachHook.cpp:15-25`).

Client events — `src-client/ll/api/event/client/` (5 pairs, contents-API verified): `ClientStartJoinLevelEvent` (world/server name, local-vs-remote), `ClientJoinLevelEvent` (`player()`), `ClientExitLevelEvent`, `ClientCancelJoinLevelEvent`. Tick: `src-client/ll/api/event/world/ClientLevelTickEvent.h`. Threading: `src-client/ll/api/thread/ClientThreadExecutor.h`.

Input — `src-client/ll/api/input/KeyRegistry.h` (`getInstance`, `getOrCreateKey(name, defaultKeyCodes, allowRemap, mod)`) and `KeyHandle.h` (`registerButtonDown/UpHandler`) — files+symbols verified via code search; raw key sniffing via `src-client/ll/api/event/input/KeyInputEvent.h` (`keyCode`, `action`). Settings-screen remap UI: **UNRESOLVED** (assume custom remap UI).

World read — `BlockSource.h` (`getBlock(pos[, layer])`, `getExtraBlock`, `getLiquidBlock`, `hasBlock`, `hasChunk(s)`, `areChunksFullyLoaded`, `getBlockEntity`, min/max height); `Dimension.h` (`getBlockSourceFromMainChunkSource`); `Actor.h` (`getDimension`, `getDimensionBlockSourceConst`, `isSneaking` inline); `TargetedBedrock.h` (`getClientInstance`, `getMultiPlayerLevel`); `ClientInstance.h` (`getLocalPlayer`, `getLevel`).

Blocks/states — `Block.h`, `BlockType.h`, `BlockState.h` (`src/mc/world/level/block[/states]/`), `Material.h`, `LiquidBlock.h`, `BlockActor.h` + `Sign/Chest/BannerBlockActor.h`.

Transforms — `Rotation.h` + `Mirror.h` (`src/mc/util/`), `Flip.h` (`src/mc/world/`), `BlockPos::transform` (`BlockPos.h`, verbatim-verified), `VanillaBlockStateTransformUtils.h` (`transformBlock`, `_rotate`, `_mirror`, `hasTransformableConnectionStates` — file verified), `StructureTemplate.h` / `StructureSettings.h` / `StructureManager.h` (`src/mc/world/level/levelgen/structure/`).

Render — `render/` events (3 files only, verified — no world stage); `LevelRenderer.h` (`renderLevel` present, exact signature reverify in PoC); `LevelRendererPlayer.h` (`setupCamera`, `getFov`, selection/wireframe materials); `LevelRendererCamera.h` (frustum, `mWorldSpaceCamera`); `GameRenderer.h` (cached view/proj matrices, no getter found); `Tessellator.h` (immediate-mode builder); `MinecraftUIRenderContext.h` (2D only); `ClientInstance.h:475-508`-region (`getLocalPlayer`, `getLevel`, per doxygen).

UI feedback — `LocalPlayer::displayClientMessage` override + `GuiData::displayClientMessage/displayChatMessage/setTitle` (`src-client/mc/client/player/LocalPlayer.h`, `src-client/mc/client/gui/GuiData.h`) verified. `ll::form::*::sendTo(Player&)` exists but **client-side delivery UNRESOLVED** (`LocalPlayer::sendNetworkPacket` sends *to* the server, not to self — do not assume Forms work on client). Toast/actionbar: **UNRESOLVED**.

Candidate hooks (with stage/risk): `LevelRenderer::renderLevel` (per-frame world stage, in-pipeline draw after `origin()`; risk: signature/field RE per game update); `LevelRendererPlayer::setupCamera` (matrix capture; low risk); `ScreenView::render` (already used by `UIRenderEvent` hooks — UI only); `IDXGISwapChain::Present/ResizeBuffers` (process-wide vtable MinHook, version-resilient; risk: D3D11/12 dual path + device-loss handling); `GameMode::$getPickRange`-style hooks as the established in-project pattern (low risk).

---

## 5. Schematic format analysis (summary)

`.litematic` primary (multi-region, self-describing, bit-packed like vanilla chunks), `.schem` v3-first/v2-free secondary (simpler varint streams, same traversal), `.mcstructure` cheap exact bonus (LE-NBT, pending one 2×2×2 index-order check), `.schematic` out (version-ambiguous numerics). NBT: vendor `mcnbt`-style header-only lib + xmake `zlib`; facade it for swap-ability. Mapping Java→Bedrock is the dominant cost: curated pruned `blocksJ2B.json` (pin game version), exact-hit/state-allowlist/placeholder-fallback decode policy, waterlog→layer-1 promotion. Rotation: Litematica order (mirror, mirror, rotate-combined), Bedrock-side state cycling via game helpers, connections recomputed or degraded in MVP. Full per-format evidence in §3 table and agent notes retained in project history.

Authoritative links: Litematica format discussion `github.com/maruohon/litematica/issues/53`, reader `litematica/.../LitematicaSchematic.java`, `github.com/SmylerMC/litemapy`, `github.com/SpongePowered/Schematic-Specification` (`versions/schematic-3.md`, `-2.md`), WorldEdit `SpongeSchematicV2/V3Reader.java`, `wiki.bedrock.dev/nbt/mcstructure`, PrismarineJS `minecraft-data` (`blocksJ2B.json`), NBT candidates `github.com/JaderoChan/mcnbt`, `github.com/handtruth/nbt-cpp`.

---

## 6. Rendering analysis

### 6.1 Why this is the crux

Everything else in Architect reads verified APIs. Rendering a 3D-anchored ghost has **no public API**: the render-event directory contains only UI events, matrix getters are absent, and the debug-draw surface is opaque in headers. Any world-space preview is custom engine work by definition.

### 6.2 Ranked approaches (for this stack)

1. **HUD-only (fallback, ship regardless)** — verified APIs only; no 3D anchoring.
2. **DX `Present` overlay with own math (first 3D)** — proven on identical stack (`ChiyanMap`); vtable hook is version-resilient; needs own world→screen from player pos/yaw+pitch+FOV; always-on-top; thread discipline required.
3. **In-pipeline wireframes (`LevelRenderer::renderLevel` hook)** — depth-correct, Horion-proven target; requires resolving `FrameRenderObject` layout + material plumbing per build.
4. **Native block-model ghosts** — deferred; chunk-pipeline injection with no prior art.

### 6.3 Selected candidate stages

PoC-1: `IDXGISwapChain::Present` (+`Present1`/`ResizeBuffers[1]` invalidation, D3D11/D3D12 discrimination) with ImGui wireframe of one hardcoded 3×3 cube from `LocalPlayer` pos/yaw/pitch. PoC-2: `LevelRenderer::renderLevel` post-`origin()` single `Tessellator` cuboid with selection/wireframe material, matrices from hooked `setupCamera`/`ViewRenderObject`.

### 6.4 Proof-of-concept plan (isolated; do NOT touch `src/`)

Create scratch mod/branch `architect-render-poc` containing only: MinHook `Present` hook, hardcoded cube, own W2S math, cleanup on `ClientExitLevelEvent`. Success criteria: cube tracks camera rotation/translation at 60 fps; survives window resize + world exit/rejoin; zero `0xC0000005` across a 10-minute session; then promote to `renderLevel`-hook cube occluded correctly by terrain and grid-aligned. If `FrameRenderObject`/cached matrices yield view/proj during PoC-1, fast-track PoC-2; else matrices stay **UNRESOLVED** pending deeper RE and the project ships overlay+HUD for MVP.

### 6.5 Threading & lifecycle rules for any renderer

Game state reads on game thread only (marshal via tick event/executor); render from cached snapshots (double-buffered, atomics/mutex); release GPU resources on `ClientExitLevelEvent`/`ClientCancelJoinLevelEvent` and device-loss paths (`onDeviceLost`/`onAppSuspended` exist on renderer classes); never call game-thread-assumed functions from `Present`.

---

## 7. Architecture proposal

```
Architect/
  schematic/      Format readers (litematic/schem/mcstructure) -> common Model
  model/          Blocks: pos (region-local) + bedrock name + states map + layer flags;
                  palette dedup; per-region containers; placement (anchor/rot/mirror/pivot)
  transform/      Wraps BlockPos::transform + VanillaBlockStateTransformUtils;
                  connection-state second pass (post-MVP); mirror-then-rotate order
  mapping/        Curated blocksJ2B-derived JSON + decode policy (exact/allowlist/placeholder)
  world/          Game-thread verifier: chunk-gated BlockSource reads, 5-state taxonomy
                  (missing/correct/incorrect/extra/unchecked), incremental per-tick budget
  render/         HUD (Phase 0) -> DX overlay (Phase 1) -> in-pipeline wireframes (Phase 2)
  ui/             Hotkeys (KeyRegistry) + chat feedback; layer filter; material counts;
                  DDUI/custom screen only if Forms-on-client proves out (currently UNRESOLVED)
  config/         ll/api/Config.h JSON + getDataDir blueprint library + per-world placements
```

Data flow: file → format reader → common model → transform → per-tick verifier diff → render snapshot → overlay/HUD. World is never written by the verifier.

---

## 8. Performance risks

* **Verify loop:** a 100³ build = 10⁶ positions; full re-scan per frame is impossible. Design: chunk-bucketed dirty tracking (recheck on `ClientLevelTickEvent` with a per-tick budget, e.g. N×1000 reads/tick; prioritize near-player and on-screen chunks). `getBlock` is a cache-friendly const read — fine at budgeted rates.
* **Render:** per-block draw calls do not scale; batch into per-chunk line lists (overlay) or single `Tessellator` mesh rebuilds on diff change (in-pipeline). Cap ghost volume for MVP (warn above e.g. 128³) with distance culling.
* **Memory:** model ≈ tens of bytes/block (name ref + sparse states) → ~10–50 MB per 10⁶-block schematic plus render buffers; acceptable, but stream per-region and drop far-chunk render data.
* **Mapping table:** curated JSON is KBs; full 5 MB table is a fallback, load-once at startup.
* **Profiling methodology:** frame-time via in-mod rolling histogram (game-thread tick + Present durations), memory via task-manager deltas per milestone schematic (small multi-type → 64³ → 128³), acceptance gates in §10.

---

## 9. Compatibility and failure modes

* **Game updates:** pins are mandatory (`xmake.lua` + `tooth.json` to the same LeviLamina line; game minor bumps re-run the PoC matrix checks). Vtable hooks (`Present`) survive better than offset hooks (`origin`/refdef math does not).
* **Unsupported blocks/states:** placeholder + counted warning, never silent air; schematic `DataVersion` newer than the map warns-and-best-efforts.
* **Missing chunks / Y limits / dimensions:** `unchecked` taxonomy + runtime `DimensionHeightRange` queries; Nether/End bounds queried, not hardcoded.
* **Lifecycle:** all caches keyed to world session; `Exit`/`Cancel` events release render + verifier state; device-loss paths release GPU objects.
* **Multiplayer:** verifier is read-only client-side and works on servers; placements are never forced; Creative 12-block vs Survival 6-block realities (cf. pocket-on-pc) illustrate why visual guidance ≠ placement authority. Remote chest contents and full actor NBT are unavailable — verifier degrades gracefully.
* **Build skew (found live):** repo currently compiles 26.51.6 while lip pins 26.20.* — fix before Architect starts or testers will install a mismatched loader.

---

## 10. Proposed acceptance tests (all PROPOSED — none executed)

1. Load a small multi-type `.litematic`; assert dimensions, positions, palette refs, orientation states decode exactly.
2. Load equivalent `.schem` v3 + `.mcstructure`; assert identical decoded models.
3. Anchor/rotate (0/90/180/270) a schematic beside a known reference; assert block-for-block placement mapping incl. stairs/doors/signs facings.
4. Mirror on each axis; assert facing/axis/hinge tables.
5. Move camera/player; assert overlay stays grid-aligned (screenshot-diff vs expected projection).
6. Classify a test rig of missing/correct/incorrect/extra/unchecked positions; assert taxonomy incl. waterlogged and unloaded-chunk cases.
7. Toggle visibility; assert zero world modification (hash chunk snapshots before/after).
8. World unload/reload; assert no stale callbacks, no leaked GPU resources, clean re-init.
9. Invalid file / unsupported state / oversized / out-of-loaded-chunks schematic; assert clear errors, no crash.
10. 128³ schematic soak: memory delta + 95th-percentile frame-time impact within budget; HUD-only path as control.
11. Remote-server session: verifier runs, no packets forged, no kicks across 30 minutes.

---

## 11. Recommendation and roadmap

**Proceed — with the MVP scoped as follows:**

* **MVP (ship):** `.litematic` + `.mcstructure` (+`.schem` if free) parsing → curated ID map → verifier taxonomy → hotkeys + chat/HUD guidance → DX-overlay wireframes → layer filter + material counts. Local and remote read-only use. No auto-place, no block-model ghosts, no Forms dependency.
* **Demonstrate FIRST (gating):** PoC-1 overlay cube (go/no-go for any 3D), then PoC-2 depth-correct cube (go/no-go for in-pipeline). If PoC-1 fails, fall back to HUD-only and re-scope.
* **Milestones:** M0 pins + NBT facade + format readers + model (2–3 wks) → M1 transform+mapping+verifier on game thread (2–3 wks) → M2 HUD + hotkeys + PoC-1 overlay (2 wks) → M3 PoC-2 + perf budgets + 128³ soak (3–4 wks) → M4 release (`v0.1.0`-style lip path as proven by pocket-on-pc).
* **Explicitly deferred:** native ghost models, auto-paste/placement automation, full-table ID mapping, `DataVersion` upgrading, container-content verification, settings-screen remap UI, `.schematic` legacy support.

**The defining Litematica experience (ghost in the world you build against) is achievable on Bedrock via overlay-first rendering; the pixel-perfect translucent-block dream is not an MVP bet.** Start with M0 + PoC-1 in parallel.
