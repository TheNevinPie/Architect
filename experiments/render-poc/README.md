# Render PoC — isolated experiment (NOT part of the mod build)

## What this is

Proof that a cube at a known world position stays grid-aligned while the
player/camera moves, per the first engineering gate. Two stages:

- **PoC-1 (gating):** DXGI `Present`-hook overlay drawing one hardcoded
  3×3 wireframe cube from a cached camera snapshot + the projection in
  `w2s_math.h`. Always-on-top (no depth test) is acceptable here.
- **PoC-2 (follow-up):** `LevelRenderer::renderLevel`-hook cuboid with a
  selection/wireframe material — depth-correct. Starts only if PoC-1 passes.

Nothing in this directory is compiled into `architect` and nothing here
may include `src/` game adapters. Promotion requires passing the gate
below, then a design review.

## What exists in M1

- `w2s_math.h` — pure projection math (lookAt/projection/project/cube
  corners), engine-free. Cross-checked by `tests/python/test_w2s.py`.
- This procedure. No hook code is checked in yet: writing hook code
  against headers that are not present locally would be speculation
  (see docs/RENDER_POC.md).

## Procedure (execute once toolchain + game are present)

- **P0 setup.** Install xmake + LLVM 22 + "C++ Clang tools for Windows"
  (docs/TOOLCHAIN.md), configure
  `xmake f -p windows -a x64 -m release --target_type=client -y`, record
  resolved versions. Install the GDK client via LeviLauncher + lip.
- **P1 camera capture.** Determine how the PoC reads, on the GAME THREAD
  each tick: camera eye position, forward (or yaw/pitch + the exact
  degree→vector mapping), vertical FOV, and viewport size. Candidates
  from feasibility §6 (all REVERIFY in headers of the pinned build):
  `GameRenderer::mLastLevelViewMatrix/mLastLevelProjMatrix`,
  `LevelRendererPlayer::setupCamera`, `mWorldSpaceCamera`. If a cached
  view/proj matrix can be read, use it directly and skip yaw/pitch math.
- **P2 hook skeleton.** New scratch target (never the `architect`
  target): MinHook `IDXGISwapChain::Present` (+`Present1`,
  `ResizeBuffers[1]` invalidation), D3D11-vs-D3D12 per-frame `GetDevice`
  discrimination, ImGui or plain line draw of the projected cube from the
  latest snapshot. Double-buffer the snapshot (game thread publishes,
  Present thread draws); no game-thread-assumed calls on the render
  thread; release GPU state on world exit + device loss.
- **P3 alignment runs.** Hardcoded cube near spawn. Record: rotation-only
  camera sweep, translation strafe, window resize, world exit/rejoin,
  10-minute session. Screenshot-diff against expected projection.
- **P4 verdict.** Promote, pivot to in-pipeline, or fall back to HUD-only.

## Gate (all must hold for PoC-1 PASS)

1. Cube tracks camera rotation AND translation at 60 fps (no swim/lag).
2. Survives window resize + world exit/rejoin without artifacts.
3. Zero `0xC0000005` across a 10-minute session.
4. Overlay cleanup releases all GPU resources (no leak across rejoin).

## Threading & lifecycle rules (non-negotiable)

Game state reads on the game thread only; render from cached snapshots;
release on `ClientExitLevelEvent`/`ClientCancelJoinLevelEvent` and
device-loss paths; never call game-thread-assumed functions from `Present`.
