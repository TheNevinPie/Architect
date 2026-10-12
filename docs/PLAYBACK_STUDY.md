# Playback Study — what Architect can learn (M2 research, 2026-10-13)

Source: `github.com/wo55555/Playback`, release **`v0.3.3-mc26.50`**
(Minecraft/LeviLamina `26.51.*` — the same line Architect pins).
Read-only reference; **no Playback code was copied into Architect**.

## License (read first)

- Playback is **AGPL-3.0** (root `LICENSE`, README badge). Its notice
  states: distributed modifications must remain under AGPL-3.0 and
  provide corresponding source.
- Architect's template license is CC0-1.0. Copying substantial Playback
  code into Architect would attach AGPL-3.0 obligations (source
  disclosure, same-license distribution) that conflict with the current
  licensing posture. **Status: learn patterns only; no reuse without a
  licensing decision.** Third-party bits inside Playback (ImGui MIT,
  FFmpeg GPL, x264 GPL-2.0) are documented in its
  `THIRD_PARTY_NOTICES.md` + `licenses/` and were not evaluated further.
- Reusable without concern: *facts* about the engine (member names,
  hook targets, math conventions) are not copyrightable expression;
  the write-up below cites file/function so the team can re-derive.

## The three tasks are different (do not conflate)

| Task | What it means | Playback's answer |
|---|---|---|
| **Controlling** the camera | Making the game LOOK from a chosen pose | Overwrite pose inside render-stage hooks (below) + teleport the observer player |
| **Obtaining** camera transforms | Reading pos/orientation/matrix for YOUR math | Read `mWorldSpaceCamera` / camera ECS; capture the submitted view-projection matrix per frame |
| **Drawing** custom geometry | Putting YOUR lines/boxes on screen, aligned | ImGui overlay (screen-space UI) + world-anchored viewport overlays projected with the game's own matrix |

## Obtaining the camera (read path)

`src/playback/keyframe/ClientCameraCapture.cpp` —
`captureClientCamera()`:

- Position: `ClientInstance::getCameraRegistry()->mGameCamera`
  (`CameraRegistry.h`) → `MinecraftCamera::CameraComponent::mPosition`,
  falling back to `mce::Camera::mPosition`.
- Orientation: `CameraComponent::mOrientation` (quaternion) → basis;
  fallback `mSavedModelView` matrix; fallback `mce::Camera`
  `mRight`/`mUp`/`mForward` members.
- Preferred camera object: `ClientInstance::getCameraActor()`, else the
  replay player, else `getLocalPlayer()`; renderer camera preferred is
  `client->getLevelRenderer()->mLevelRendererPlayer->mWorldSpaceCamera`
  (`LevelRendererPlayer.h`), fallback `client->getCamera()`.
- FOV: `CameraComponent::mFieldOfView`, with a unit guard
  (`cameraFovDegrees`: value ≤ π is treated as radians, else degrees),
  fallback `mce::Camera::mFov`.

**Yaw/pitch convention (resolves Architect M1 measurement (a)):**

- `yaw = atan2(-forward.x, forward.z)`, `pitch = atan2(-forward.y,
  hypot(forward.x, forward.z))`, in degrees (`anglesFromBasis`).
- Inverse (`basisFromState`, also in `CameraRenderHooks.cpp`):
  `forward = (-sinYaw·cosPitch, -sinPitch, cosYaw·cosPitch)`,
  `right = (-cosYaw, 0, -sinYaw)`,
  `up = (-sinYaw·sinPitch, cosPitch, cosYaw·sinPitch)`.
- Plain reading: yaw 0 looks along +Z (south), yaw +90 looks along −X
  (west); **negative pitch looks up**. Architect's overlay math can use
  these formulas directly (math is not AGPL expression).

## Obtaining the matrix (the key finding for Architect)

`src/playback/editor/graphics/CameraRenderHooks.{h,cpp}`:

- `publishRenderCameraProjection(worldCamera, clientCamera, sampleTime)`
  builds the overlay matrix as
  **`transform = dmat4(projection) * dmat4(view)`**, where view comes
  from `worldCamera.viewMatrixStack->top()._m` and projection from
  `worldCamera.getProjectionMatrix()` (validated as perspective by
  `|m[2][3]| > 0.5`; falls back to the client camera, then to a
  synthesized `perspective(fov, aspect, zNear, 4096)`).
- Near plane is **recovered from the matrix itself**
  (`nearPlaneOfProjection`; handles reverse-Z / infinite-far); the
  comment states `mZNear` "is not a dependable member offset".
- Per-frame tagging: `ReplayCameraFrameScopeHook` on
  **`GameRenderer::renderCurrentFrame`** opens the frame scope;
  `ReplayCameraSubmitFrameHook` on **`bgfx::Context::frame`**
  (`mc/external/bgfx/Context.h`) queues that frame's projection and
  `useSubmittedCameraProjection(bgfx::Frame const*)` consumes it in
  submission order. `currentRenderCameraProjection()` prefers the
  frame-tagged projection, else the latest published one.
- Takeaway: Bedrock renders through **bgfx**; the aligned-overlay recipe
  is *hook the frame submission, grab view+projection, project your own
  boxes with the game's matrix*. This answers M1 measurements (b)+(d):
  no need to reconstruct FOV/aspect — consume the matrix.

## Controlling the camera (write path — Architect does NOT need this,
## but it proves the hooks are real and reversible)

Same file, `LL_TYPE_INSTANCE_HOOK` (Lowest priority), installed/removed
by `hookCameraRender(bool)`:

- `LevelRendererPlayer::setupCamera(mce::Camera&, float)` —
  `ReplayCameraSetupHook`: records the native pose, then overwrites via
  `applyTimelineCamera` / `applyObserverCamera`.
- `LevelRendererPlayer::createViewRenderObject(ScreenContext&,
  SubClientId)` — `ReplayCameraViewRenderObjectHook`: final overwrite of
  `ViewRenderObject::mViewData` (`mCameraPos`/`mCameraTargetPos`) for export.
- `LevelRendererPlayer::getFov(float, bool)` + `CameraAPI::$tryGetFOV`
  — FOV override (note: the `CameraAPI` path reports **radians**; the
  comment records that 26.40 inlined `getFovWithoutGameplay`).
- `writeCameraPose` writes `mce::Camera` (`viewMatrixStack` top +
  `_isDirty` + `updateViewMatrixDependencies()`, `mPosition/mRight/mUp/
  mForward`); `writeLevelCameraPose` writes `LevelRendererPlayer::
  mCameraPos/mCameraTargetPos/mCameraForward/mCameraUp`; `applyCameraEcs`
  writes ECS `CameraComponent::{mPosition, mOrientation, mFieldOfView,
  mSavedModelView}` on the game camera + active/render entities.
- Every write is **verified** (2 cm position / 0.002 direction
  tolerances) and logged once on failure — a pattern worth imitating.
- Observer positioning additionally uses player teleport
  (`ReplaySession::teleportReplayPlayer/teleport` + `updateObserverPreview`,
  `syncObserverServerPosition`; see `ReplaySession.h`).

## Drawing / capture paths (D3D11 + D3D12)

- `src/playback/editor/graphics/D3D12Hooks.h` — detours on
  `Present`/`Present1`/`ResizeBuffers`/`ResizeBuffers1` and all four
  `CreateSwapChain*` variants, plus swapchain↔command-queue binding
  (`bindSwapChainQueue`), SRV heap sized 512, 2 s GPU/detour timeouts.
- `D3D11FrameTapBackend` / `D3D12FrameTapBackend` (+`D3D12Downsampler`)
  — GPU readback into `FrameSubmission{exportResource, commandQueue,
  completionFence, ...}` → `CapturedFrame` pixels (`FrameCaptureTypes.h`).
- `FrameTap.h` — the handoff: backends `beginCapture/complete/fail`
  **never block the Present thread** (mutex + condition variable,
  bounded capacity) — same discipline as Architect's snapshot rule.
- `ImGuiRenderer` — ImGui (MIT, `imgui v1.92.7` with `dx11+dx12` in
  `xmake.lua`) draws the editor UI on both backends.
- CHANGELOG evidence on *where* to tap (v0.2.1): capture was reworked
  "around the **RenderDragon scene target** and semantic
  world-submission detection **instead of the swap-chain UI target**" —
  i.e. the swap chain sees composed UI (sky-color frames bug), the world
  scene target is the correct source. D3D11 stays a 1x path; D3D12 does
  up to 4x SSAA with GPU downsample.
- System links in `xmake.lua`: `d3d11 d3d12 dxgi dcomp d3dcompiler`
  (+`windowscodecs` …) — direct D3D/DXGI use, no MinHook package
  (hooks are LeviLamina `LL_*_HOOK`s).

## Toolchain cross-check (validates Architect's M1 choices)

Playback's `xmake.lua` on the same 26.51 line: `levilamina 26.51.*`,
client-only, `clang-cl`, `/EHa` + `set_exceptions("none")`, `MD`
runtime, `levibuildscript` linkrule/modpacker. Architect's build config
matches this shape exactly. (Playback additionally needs FFmpeg/MSYS2 —
Architect needs neither.)

## Recommendation for Architect's cube (simplest path)

1. **Keep PoC-1 (Present-hook overlay) only as a fallback.** Playback
   shows the better first-3D target: hook `LevelRendererPlayer::
   setupCamera` (lowest risk, Horion-analogous, already proven writable
   by Playback on 26.51), read `mWorldSpaceCamera` view +
   `getProjectionMatrix()`, and draw the cube with an ImGui draw list
   in screen space. No yaw/pitch/FOV reconstruction needed — the matrix
   IS the camera.
2. **Then graduate to the submitted-frame projection**
   (`bgfx::Context::frame` queue pattern) so the overlay uses the exact
   matrix each frame was submitted with — this is Playback's answer to
   "remains aligned while the camera moves".
3. **Still required as a live experiment:** confirming these member
   offsets/signatures in the pinned 26.51 headers, one alignment run
   (rotation sweep + strafe + resize + rejoin), and the 10-minute
   stability gate. Playback raises confidence substantially but does not
   replace Architect's own run — different code, different draw calls.
4. **Do not copy:** re-derive the ~20-line projection-consume logic in
   Architect's own style if/when the PoC passes, and record attribution.

## Files examined (all at tag v0.3.3-mc26.50)

- `src/playback/keyframe/ClientCameraCapture.cpp` (full) + `CameraRenderState.h` (full)
- `src/playback/editor/graphics/CameraRenderHooks.h` (full) + `CameraRenderHooks.cpp` (full)
- `src/playback/editor/graphics/D3D12Hooks.h` (full)
- `src/playback/visuals/FrameTap.h` (full) + `FrameCaptureTypes.h` (full)
- `src/playback/exporting/ExportKeyframeApplier.cpp` (full) + `OfflineRenderBoundary.h` (full)
- `src/playback/replay/ReplaySession.h` (full; `.cpp` 174 KB — signatures only)
- `src/playback/keyframe/CameraTimelineEvaluator.h` (full)
- root `README.md`, `CHANGELOG.md` (full), `xmake.lua` (full), `THIRD_PARTY_NOTICES.md` (full)
- directory listings: `src/`, `src/playback/`, `keyframe/`, `exporting/`, `visuals/`, `runtime/`, `integration/`, `editor/`, `editor/graphics/`, `action/`, `record/`, `utils/`
