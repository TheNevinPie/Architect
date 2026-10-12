# Architect — Rendering Assessment (M1)

## Status: PoC NOT executable in M1 — documented blocker, not a failure

The first engineering gate asks for a cube that stays aligned while the
camera moves. That experiment needs three things this machine does not
have (evidence in `docs/TOOLCHAIN.md`):

1. **A C++ toolchain that can build the mod** — xmake, clang-cl/LLVM 22,
   and the LeviLamina headers (fetched at configure time) are all absent.
2. **A game to run it against** — no LeviLauncher/lip binary on PATH, no
   GDK client install found.
3. **SDK headers to verify hook signatures against** — `renderLevel`'s
   exact parameter list, `FrameRenderObject`/`ViewRenderObject` layout,
   and view/proj matrix getters were already UNRESOLVED in the
   feasibility report (§6) and cannot be resolved without the pinned
   26.51 headers.

Writing hook code now — without headers to compile against or a game to
run it in — would be exactly the speculative-API work the project rules
forbid. So M1 delivers the parts of the PoC that are provable today, and
a written execution procedure for the rest.

## What M1 establishes (evidence, not claims)

- **Projection math is implemented and tested.**
  `experiments/render-poc/w2s_math.h` (lookAt LH, perspective LH,
  row-vector project, cube corners) is engine-free; `tests/python/test_w2s.py`
  (COMPLETED, 10/10 pass) independently reimplements the pipeline and
  asserts centering, edge hits, behind-camera rejection, aspect/resize
  behavior, eye-strafe response, and full-cube projection. When the PoC
  runs, misalignment will point at camera-input capture, not the math.
- **The camera-input question is scoped to 4 measurements** (yaw/pitch →
  forward mapping, FOV verticality/source, aspect source on resize, NDC
  z-range), listed as PoC tasks P1–P3 in `experiments/render-poc/README.md`.
- **Threading/lifecycle contract is written** (game-thread publish →
  snapshot → Present-thread draw; cleanup on exit/device-loss) before any
  hook code exists, so the PoC author inherits constraints instead of
  discovering them in crashes.
- **Approach ranking from feasibility §6 stands uncontradicted:**
  Present-hook overlay first (lowest 3D risk, proven ChiyanMap pattern on
  this stack), `renderLevel` wireframes second, native ghost models
  deferred. No new evidence for or against; no alternative proposed.

## Investigated gate dimensions (from the assignment)

| Dimension | M1 finding |
|---|---|
| Camera transforms | Math ready + tested; game-value mapping reserved as measurements (a)–(d) in `w2s_math.h` |
| Projection | LH perspective implemented; NDC z-range (0..1 vs −1..1) must be confirmed against the GDK build |
| Depth occlusion | Known limitation accepted: overlay is always-on-top; depth-correct rendering is PoC-2's job |
| Resize behavior | Aspect/viewport sourcing is a listed measurement; hook must handle `ResizeBuffers[1]` invalidation (GDK ≥ 1.21.120 per feasibility) |
| Render-thread safety | Contract written (double-buffered snapshot, no game calls on render thread) |
| Resource cleanup | Gate item 4 + exit/device-loss release rules written |

## Update (M2, 2026-10-13): Playback study resolves the open measurements

`docs/PLAYBACK_STUDY.md` (Playback `v0.3.3-mc26.50`, same 26.51 line)
answers M1 measurements (a)–(d) with working-mod evidence:

- **(a) yaw/pitch → forward: RESOLVED.** `forward =
  (−sinYaw·cosPitch, −sinPitch, cosYaw·cosPitch)`; yaw 0 = +Z (south),
  +90 = −X (west); negative pitch looks up. Prefer the matrix over
  angles anyway (next point).
- **(b)+(d) FOV/aspect/NDC: RESOLVED conceptually.** Do not reconstruct
  them: consume `mWorldSpaceCamera` view + `getProjectionMatrix()` per
  submitted frame (`bgfx::Context::frame` queue pattern). Near plane is
  recovered from the matrix (reverse-Z aware); `mZNear` is undependable.
- **(c) resize: procedure known.** Detour `ResizeBuffers[1]` +
  `CreateSwapChain*` variants, rebind swapchain↔queue, invalidate
  size-dependent resources (Playback `D3D12Hooks.h` pattern).
- **Recommended first-3D target revised:** `LevelRendererPlayer::
  setupCamera` hook + ImGui screen-space cube from the game's own
  matrix, graduating to frame-tagged projections. The Present-hook
  overlay drops to fallback status; the swap chain sees composed UI,
  not the world scene (Playback CHANGELOG v0.2.1 evidence).
- **Still required:** Architect's own live run (headers, alignment,
  stability gate). Playback raises confidence; it is not our experiment.

## Licensing note

Playback is AGPL-3.0: learn patterns only, no code copied (see
`docs/PLAYBACK_STUDY.md`). If reuse is ever approved, record
attribution + source-disclosure obligations before merging.
