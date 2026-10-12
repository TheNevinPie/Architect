# Architect — Overlay Plan: camera matrix + screen-space boxes (P3, unexecuted)

Status: DESIGN ONLY. No hook code written, no game run performed.
Reference: `docs/PLAYBACK_STUDY.md` (patterns only — no Playback code
copied, no Playback dependency introduced, AGPL-3.0 constraint stands).
Parser separation: nothing in this plan touches `src/architect/`
decoders; the overlay consumes `RenderSnapshot` boxes + a matrix.

## Idea in one paragraph

Don't reconstruct the camera from angles. Each frame, take the game's
own view and projection matrices (the exact numbers the world was drawn
with), multiply each schematic box corner by view×projection, divide by
w, and draw the resulting 2D lines on top. If the matrix is the game's,
alignment is exact by construction; any drift points at stale matrices,
never at math.

## Where the matrix comes from (in order of preference)

1. **Frame-tagged submission** (Playback-proven on 26.51): hook
   `bgfx::Context::frame`, queue the `mWorldSpaceCamera` view +
   `getProjectionMatrix()` published for that frame, consume in
   submission order. Immune to mid-frame camera updates.
2. **setupCamera read** (simpler, first): hook
   `LevelRendererPlayer::setupCamera`, read the same two matrices after
   `origin()`. One frame of staleness at most; fine for the cube gate.
3. **Angles fallback** (never preferred): yaw/pitch/FOV → forward =
   (−sinYaw·cosP, −sinP, cosYaw·cosP) per Playback's confirmed
   convention; only if matrix members prove unreachable.

Near plane comes from the matrix itself (reverse-Z aware); `mZNear` is
undependable (Playback comment evidence). Points with w ≤ 0 are
behind-camera and skipped; near-plane clipping for crossing edges.

## What is drawn (and where)

- Stage A (gate): ONE hardcoded cube (unit box at a known world pos),
  12 edges, single color, ImGui screen-space draw list. Always-on-top
  (no depth) — acceptable for the gate, documented limitation.
- Stage B (later): per-`RenderSnapshot` boxes colored by `colorFor()`,
  chunk-bucketed; Stage C: depth-correct in-pipeline wires (PoC-2).
- HUD-only remains the fallback if Stage A fails.

## Threading, resize, cleanup (from the M1 contract + Playback)

- Game thread publishes snapshot + matrix per frame (double-buffered);
  Present/render thread draws only. No game calls on the render thread.
- Resize: `ResizeBuffers[1]` + `CreateSwapChain*` invalidate
  size-dependent resources; swapchain↔queue rebinding per D3D12Hooks pattern.
- Release everything on world exit + device loss; zero-tolerance gate:
  10-minute session, no `0xC0000005`, clean rejoin.

## Gate to pass (unchanged from experiments/render-poc/README.md)

Rotation sweep + strafe + resize + exit/rejoin; screenshot-diff vs the
`w2s_math.h` projection; 60 fps tracking; no leaks. Record camera
alignment, depth behavior, resizing, and cleanup in the PoC report —
only then design the production renderer.
