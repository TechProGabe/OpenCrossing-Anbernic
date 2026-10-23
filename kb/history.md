# Project history & decisions

- **2026-07-12** — Project started as a port attempt of flyngmt/ACGC-PC-Port
  (Animal Crossing GC decomp PC port) to the RG-34XX SP. Chose the
  **Dia2809 fork** as base: it already had Linux, GLES 3.2 renderer
  (`-DPC_USE_GLES=ON`), and arm64/armhf branches. Not in the PortMaster
  catalog (checked all 1326 ports). Private mirror created (not a GitHub
  fork — forks of public repos can't be private).
- **2026-07-12** — Test harness built (Docker smoke tests arm64+armhf via
  colima/qemu, macOS native run). macOS build fixes (Apple ld guards, cKF
  stubs). armhf emu64 fix: `mw_data` → `moveword->data` (emu64.c:5537).
- **2026-07-13 (morning)** — First device boot: rendering, controls, saves
  worked; audio dead. Root cause: 32-bit PipeWire couldn't find SPA plugins;
  ALSA fallback routed through the same broken plugin. Fixed via launcher env
  (SPA_PLUGIN_DIR/PIPEWIRE_MODULE_DIR → /usr/lib32, SDL_AUDIODRIVER list).
- **2026-07-13** — **Pivot decision (maintainer)**: this is its own project now —
  "native Animal Crossing build for Anbernic" — not a tracking fork.
  Upstream/Dia credited in README but development is independent.
- **2026-07-13** — Repo consolidation: renamed to
  `GabeConway/animal-crossing-anbernic`; branches arm64-local/armhf-local/
  master merged into single `main` (harness/portmaster/macOS fixes harvested
  from arm64-local; armhf-local was the base since the device build is the
  product); worktree removed; local dir renamed. `dia`/`upstream` remotes
  kept read-only. Later same day: `dev` branch created — main = releases
  (CI on v* tags), dev = daily work, to conserve Actions minutes.
- **2026-07-13** — Perf pass 1 (uniform-loc cache, texture hash index,
  shader binary disk cache + boot preload, boot splash). Device feedback:
  audio good, minor first-boot menu stutter that self-resolved (cache
  warming). FPS counter option existed but hid in DEBUG tab → moved to VIDEO.
- **2026-07-13** — Perf pass 2 (the -O0 discovery + -O2/NEON flags, NEON
  decoders, decode budget, draw-call merging), CI release workflow,
  noob-friendly README, BUILDING.md, kb/ knowledge split.
- **2026-07-13 (later)** — Perf pass 3 shipped as **v0.2.0** (GX state
  dedup, per-program uniform shadowing, dynamic-fps upward probe, seed
  regrown to 101 configs). Post-release: streaming VBO (P3) v1
  black-screened the Mali blob (SubData + per-batch attrib respec —
  silent, fine on Mesa); v2 rework (glMapBufferRange UNSYNC + BaseVertex
  draws + glGetError probe with auto-fallback) device-verified. Lesson:
  Mali blob tolerates zero per-draw attrib-pointer churn; always ship a
  runtime kill switch + self-healing fallback with renderer experiments.
  Measurement across passes: per-draw cost pinned at ~30µs regardless of
  buffer strategy → future perf work targets draw COUNT. README gained
  the H700 "likely working devices" table.

- **2026-07-15** — README condensed ~255→160 lines and reordered
  (about → install → troubleshooting → devices → FAQ → the rest) after the
  r/ANBERNIC thread brought a wave of new users; install steps kept intact
  (community-praised). Cut content archived here so nothing is lost:
  - **"Why this project exists" section** (full text): the ac-decomp
    decompilation made a PC port possible; the ports this project descends
    from target x86 desktops with desktop OpenGL, and none run on the
    low-power ARM handhelds the game feels made for — a pocket town to
    check in on anywhere. Getting there required work beyond a recompile:
    (1) 32-bit ARM (armhf) build for the H700's Cortex-A53 cores via a
    Debian bookworm Docker toolchain; (2) OpenGL ES-only rendering path
    for the Mali-G31 (no desktop GL on device), with runtime-specialized
    branchless TEV fragment shaders — Mali GPUs are extremely slow at the
    dynamic branching an "uber-shader" needs; (3) shader binary disk cache
    with boot-time preload so compile hitches only ever happen the first
    time a material is seen; (4) handheld performance features: dynamic
    FPS target, render scale, frame pacing, distance culling,
    shadow/particle/acre quality, all in a gamepad-first in-game menu;
    (5) muOS/PortMaster integration: launcher script, bundled 32-bit
    fallback libraries, workarounds for the device's 32-bit PipeWire audio
    stack; (6) 480p-friendly UI: aspect handling, letterbox/stretch modes,
    overlay menu readable on a 3.4" screen. Upstream continues to target
    desktops; this project is its own thing, focused on making the game
    genuinely good on handheld hardware.
  - **Performance detail** (was in status blockquote): v0.3.0 on RG-34XX
    SP — ~56 fps average, median ~60, 78% of samples ≥55, ~100% game
    speed, low-40s dips during heaviest acre streaming (full data
    kb/perf.md).
  - **Verbose save-management how-to** (now three lines in README):
    back up = copy `.gci` out of `save/card_a/` before updating; delete
    town = remove `.gci`, game creates fresh save on boot like a new
    memory card; import = drop a Dolphin GCI export into `save/card_a/`;
    restore = copy backup over. Always with the game closed.
  - Device-table footnotes ("thanks to the RG35XX SP tester" etc.) and the
    per-device tip attributions — testers: RG35XX SP (dpad tip), RG35XX H
    modded stock (Select/Menu swap, issue #1).

- **2026-07-15** — Big fix day, all four changes DEVICE-VERIFIED same day
  on the RG-34XX SP (details kb/issues.md Resolved):
  (1) in-game clock freeze (u64 overflow in osGetTime, 455.5s wrap) —
  from an RG28XX community report; (2) shovel save-while-holding dupe
  (game-start stow+clear in pc_m_card.c; the long-suspected GC pipeline
  stow never existed — m_card.c is excluded from the PC build);
  (3) resolution auto-detect (launcher no longer hardcodes 720×480);
  (4) dpad-as-joystick auto-enable on stickless pads (tri-state setting).
  GitHub issue #1 (RG35XX H modded stock OS works) closed with docs.
  r/ANBERNIC showcase thread (~600 upvotes/44K views) mined for docs:
  RG40XX V + RG35XX Knulli confirmations, FAQ added, README condensed.
  Process notes: first local armhf build via colima (needed
  `tonistiigi/binfmt --install arm` once; CI builds now user-approval
  only to save Actions minutes); smoke harness can exercise the
  auto-detect path (kb/build-test.md); device log stdout tail is lost on
  unclean unmount — see kb/device.md "Reading device logs".

## Standing decisions

- Device build (armhf) is the product; desktop builds exist for development.
- Never commit ROM material (`harness/rom/`, card contents).
- README carries "not tested end-to-end" status until a full playthrough
  happens on device.
- AI notice stays in README (platform code only, not decomp game code).

## v0.5.0 (2026-07-19)

- Save validation (GC-style checksum + ID on both card paths; garbage →
  backups, Card B → abort travel; PC_NO_SAVE_VALIDATE=1 triage), Resetti
  fix (armed reset_code persisted at game start, GC save-param semantics),
  boot error screen (missing/unreadable/wrong-region disc; GAFE01
  enforced). All device-verified before release. Diagnostics: special
  event schedule, wade placement, empty-TLUT draws (verbose=1 only).
- Docs: NES limitation (PPC-asm core), camper move-in by design, PAL FAQ
  (issue #3), RG40XX H community-tested (issue #2).
- Pre-release matrix (8 tests) all green; zip sanity-checked post-CI.
- Open after release: tourney fishing-on-land (retest + wade log), green
  diary magenta (empty-TLUT log), dock 1.4s hang ([PROF] awaiting log),
  inventory aspect flicker.

## Post-v0.5.0 (dev)

- **2026-07-29** — Issue #6 ("save never completes", RG35XX Pro/Knulli)
  answered as NOT A BUG: the door gyroid's intro refusal message reads like a
  progress message. Investigation method worth keeping: decode the game's own
  text bank (kb/game.md "Reading the game's own dialogue") instead of guessing
  at dialogue, then trace the actor talk state machine by its order codes.
  Shipped a `[PC] Door gyroid:` branch log so the next such report is
  answerable from log.txt; README FAQ + kb entries added. Also found this way:
  a user-visible refusal message with no logging is indistinguishable from a
  silent failure — prefer logging *decision branches*, not just errors.
- **2026-07-29** — kb/build-test.md corrected: it claimed game code was capped
  at -O1 (it carries no -O at all) and pointed at a `build_pc.sh` that does not
  exist. Tier-1 native macOS build confirmed dead on Apple Silicon (32-bit
  guard, pc/CMakeLists.txt:41) — armhf Docker is the only compile check here.
- **2026-10-23** — Upstream backport round (flyngmt/ACGC-PC-Port to v0.9.3,
  Dia2809 branches, OpenCrossing-Xbox NES fixes), all compile-checked only,
  **none device-tested yet**:
  - Windows cross-build: `python pc/tools/build_windows.py` (Arm GCC 12.2 +
    Debian bookworm armhf SDL2/GLES, same glibc as CI). Compile checks no
    longer need Docker.
  - Clean upstream fixes: inventory UB, decomp UB batch, Nook softlock
    return, island house door, weather on restart, radio palette, sounds
    after travel, mailbox black screen; Dia's villager-house NULL guard.
  - NES furniture on fixNES (kb/game.md).
  - Save hand-port: Wisp/Money Rock cleanup on final save, copy-protect +
    travel clock on every save, card A written on returning home.
  - `BUGFIXES` defined (own commit; revert alone if feel changes).
  - Skipped on purpose: upstream delta-time work (we run fixed 60 Hz
    logic), desktop renderer/shader work, pause menu/keybind editor,
    borderless acres, Dia's LP64 port. Still open from the triage: Dia's
    crash backtrace (needs -funwind-tables), post-office invisible mail
    (repro first), BCn texture-pack decode, rspsim envParam swap
    (f3134b5), D-pad QoL (clashes with dpad_as_stick).
  - Pre-existing -Warray-bounds in pc_m_card.c (l_keepMail/l_keepDiary
    memcpy at :169/:707/:725, 28-30 bytes past the object) worth a look.
    (Fixed in the second round below.)
- **2026-10-23 (second round)** — also compile-checked only:
  - Endless acres (upstream borderless acres), on by default.
  - Card B mail/diary buffer overrun fixed; envmixer reverb/right ramp
    swap fixed (clamp kept); crash report to log.txt + crash.log.
  - 15 byte-reversed palettes (pink diaries) fixed, verified against
    the disc.
  - GAME settings tab: Endless Acres, Bells to Wallet, Shop Upgrade,
    Disable Resetti (DEBUG tab folded into GAME/PERF). D-pad works in
    menus and text.
  - Code review caught a real NES audio race (game thread vs
    AudioProducer on one ring), fixed with a separate NES ring.
  - Still needs a device: fishing-tourney placement (retest), one-time
    dock/home-menu stalls, inventory flicker, NES fps/audio, endless
    acres fps.
- **2026-10-23 hardware test 1** (RG-34XX SP, 100% town save): NES runs
  fine, Clu Clu Land D works, NES audio slightly choppy, two settings
  menus confusing, ~40 fps dip for a moment on walking out of a house
  (also in v0.5.0). log.txt had no perf lines (verbose was 0).
  Round 3 answer: NES mix queue primed + rate control + linear
  resample; one Options page (Xbox style) from title and Select; title
  Start Game / Options / Exit to OS; loading screen with logo and
  progress; Bells to Wallet on by default. House-exit dip: texture
  cache ruled out (only cleared at shutdown); needs a verbose log
  ([STUTTER] tex/gl/work split, [PROF] phase) before changing code.
