# Known issues & leads

## Open

- **Scene-change hitch, 70-115 ms** (minor, 2026-10-23): walking out of a
  house or a title-demo cut builds the new scene between frames. v0.6.0
  logs a `[SCENE]` line per change (verbose) with the step split; build 5
  showed ct=67 ms with field 7 ms and actors 6 ms, so most of it is elsewhere
  in play_init or the first frame. Users report it as barely noticeable.
- **Villagers "fishing on land" during the fishing tournament**
  (2026-07-19 user report). Tourney flow: `anglingtournament_start`
  (ac_event_manager.c:2722) reserves the pool block and spawns 5 NPCs;
  placement goes through `search_free_unit` (:714) whose seed again uses
  live RTC (`month*day + sec + (hour+cur)*3 + seed*9`, :728), and
  `be_flat_unit` (:1586) converts block/unit → world pos WITHOUT
  validating the tile is water (GC never needed to — sane seeds kept
  positions in the reserved pool block). Same port suspect as the Redd
  entry: pre-v0.4.0 frozen/sawtoothing clock → degenerate seeds →
  positions GC could never roll. ACTION: retest on v0.4.0+ during a
  tournament; if it still happens, add a water-attribute check
  (mCoBG attribute lookup) before accepting a wade position.
  INSTRUMENTED 2026-07-19: `[PC] EvMgr wade place: event=N actor=0x....
  block=(x,z) unit=(x,z)` logs every wade placement — compare against
  the pool block on the town map.

- **Design Editor SIGSEGV (upstream Dia2809/ACGC-PC-Port#18) — our code
  path looks NOT affected; needs a device test to confirm** (2026-07-19,
  analysis corrected same day). Upstream gdb trace shows dl_G_LOADTLUT
  with tlut_name=198906/count=29732 indexing 16-entry TLUT arrays → OOB →
  prbuf corruption → crash in pc_gx_flush_vertices. In OUR tree that
  value is unreachable: the type==2 path reads tlut_name through the
  4-bit bitfield (gbi_extensions.h:455, TARGET_PC-reversed layout) so it
  can never exceed 15 (= NUM_TLUTS-1), and the N64 else-path both masks
  `& 0xF` (emu64.c:3897) and bounds-checks (:3924); GXLoadTlut also
  rejects idx>=16 (pc_gx_texture.c:1156 — the flush before it is
  harmless, it only drains pending vertices). The Design Editor's DL
  comes from gsDPLoadTLUT_Dolphin at m_design_ovl.c:2590 — well-formed.
  Upstream's crash is most plausibly their divergent DL parsing (their
  emu64.c differs; a raw-shift read without the bitfield/mask would
  produce exactly such garbage). Residual local risk is only the
  `*(u16*)tlut_addr` reuse-detection deref (emu64.c:3850) on a garbage DL
  address — no evidence of that here. DEVICE-VERIFIED 2026-07-19
  (RG-34XX SP): Design Editor opens, edits, and saves fine — upstream #18
  confirmed NOT present in our tree. Entry kept for the analysis; nothing
  to fix.

- **Inventory-open aspect flicker** (2026-07-13): opening the inventory makes
  the game EFB-capture the frame and redraw it as a background; during the
  handoff emu64's NOOP markers switch `g_pc_widescreen_stretch`
  (0=hor+ → 1=stretch for bg blits → 2=UI pillarbox; reset to 0 each frame
  in pc_gx_begin_frame). The captured letterboxed image and the blit mode
  disagree for 1-2 frames → visible width jump. Where to look:
  emu64.c `dl_G_NOOP` (marker handling), m_play.c ~688/738 (marker emission),
  pc_gx.c GXSetProjection/GXSetViewport mode-2 remap, EFB capture path in
  pc_gx_copy_tex_execute / GXLoadTexObj bypass. Fix needs device A/B.
## Port limitations (by design — answer reports with these, don't reopen)

- **Campsite (tent/igloo) villagers can never move in** — game design,
  same as real GC hardware; full source proof in kb/game.md. Not RNG,
  not village rating.

- **"Save never completes — door gyroid stuck on *I am currently
  processing data*"** (issue #6, 2026-07-29, RG35XX Pro/Knulli, NOT A
  BUG): that text is the gyroid's *refusal* message, not a progress
  message — MSG_2350 (message_data.bin entry 2350): "Welcome home, X! /
  I am currently processing data for X. / Good luck with your part-time
  job." Two A presses close it; no save is ever requested, which is why
  such a log.txt has zero save/card lines and card_a/ stays empty.
  Gate: `aHNW_decide_msg_idx_dance` (ac_haniwa_move.c_inc:169) picks
  `aHNW_MSG_NEED_FRIEND` while all three hold — `has_saved == FALSE`,
  `mEv_CheckFirstJob() == TRUE` (set when Nook hands out the job,
  ac_npc_guide_move.c_inc:796; cleared when the errands finish,
  ac_intro_demo_move.c_inc:197), and `mNpc_GetFriendAnimalNum() == 0`.
  Stock GC logic, no pc/ override. Answer: finish Nook's part-time job
  or talk to villagers until one remembers you.
  Save path for reference: gyroid → player walks in the door →
  SCENE_PLAYERSELECT_SAVE (m_scene.c:354) → restart NPC
  (ac_npc_restart_talk.c_inc:191) → `mCD_SaveHome_bg`
  (pc/src/pc_m_card.c:989). Only silent path through that writer is
  `!pc_save_ready`; every other outcome logs, so "no save lines at all"
  means the save scene was never entered.
  INSTRUMENTED 2026-07-29: `[PC] Door gyroid: <branch> | has_saved=N
  first_job=N friends=N` on every gyroid talk — makes the next such
  report answerable from log.txt alone.
  Aside from the same report: on Knulli the port lives at
  `/userdata/roms/ports/ac-gc` (not `/userdata/ports/...`) — harmless,
  all save paths are cwd-relative.

## Resolved (keep for pattern-matching)

- **Low fps in town / work-dominated stutters** (RESOLVED v0.6.0): actor
  culling had been forced off on PC since the first port; restoring the GC
  frustum test plus scoped LTO and -O2 math took town from ~51 to ~58.6 fps.
  Old per-draw/GL notes are in kb/perf.md.
- **Multi-second hangs (3.1 s in test 3, the July 1.4 s dock and 8.7 s menu
  cases)** (RESOLVED v0.6.0, likely all one cause): unbuffered verbose stdout
  blocking on SD writeback while the sound code printed several lines a
  frame. Prints gated (PC_AUDIO_DEBUG=1), stdout buffered.
- **Green diary renders hot pink** (RESOLVED v0.6.0): 15 u8-declared
  palettes loaded without the u16 swap Dolphin TLUTs need.
- **NES furniture said "I don't have any software"** (RESOLVED v0.6.0):
  fixNES backport, see kb/game.md.
- **toNextLand "aborting" log line on every scene change** (RESOLVED v0.6.0).

- **"Redd never sends the visit letter"** (reported + user-confirmed
  NOT A BUG same day, 2026-07-19): Redd showed up eventually. Special
  events are purely schedule-driven (init_special_event m_event.c:865,
  gap ≥2 days, seeded off RTC + player id; leaflet lands via
  ac_event_manager.c:130) — no player-action prerequisite, just
  patience. Scheduler log line kept
  (`[PC] Special event scheduled: ...`) — answers this class of report
  straight from log.txt next time.

- **Edited/foreign GCI saves loaded unvalidated → crash + home-save
  corruption** (RESOLVED on dev 2026-07-19, device-verify pending; from
  user report "save editor crash right after the enter-game dialogue" +
  upstream Dia2809/ACGC-PC-Port#28 "random gamefaqs GCI in card B → train
  crash → white face, empty inventory"). Root cause: m_card.c (and its
  `mFRm_CheckSaveData_common` + whole-entry checksum gating,
  m_card.c:3106/3357) is excluded from the PC build and pc_m_card.c
  replaced it with almost no validation — Card B checked only the
  `mLd_CheckId` land_id bitmask, Card A nothing (roundtrip return
  ignored). Corrupt data then flowed into `mCD_toNextLand` →
  `mFM_SetBlockKindLoadCombi` → OOB `Save_Get(fg[bz][bx])`
  (m_field_make.c:169-193) → segfault at exactly "after the enter-game
  dialogue" / mid-train-ride; the next save persisted the trashed state.
  Fix (pc_m_card.c): `pc_save_be_sum_ok()` — BE u16 sum over
  `sizeof(Save)` on the RAW image before bswap (struct-aware bswap does
  not preserve u16 sums; region matches GC entrysize and the PC writer's
  flat checksum, stamped since the first commit so every legit PC/GC/
  Dolphin save passes) + `mFRm_CheckSaveData_common()` after bswap; both
  card paths now try main copy then in-file backup, then refuse — Card A
  falls back to .bak rotations, Card B aborts the trip with
  TRANS_ERR_CORRUPT before anything is copied. ARAM blocks
  (mail/original/diary) get per-block sums like GC
  (m_card.c:6081-6098), bad block → empty block; enforced only for
  gc_order saves (legacy PC block order predates block stamping).
  Triage switch: `PC_NO_SAVE_VALIDATE=1`. Note for save-editor users:
  editors must recompute the AC checksum (most GCI editors do; raw hex
  edits now get rejected at load instead of crashing — game falls back
  to backups/new-game rather than booting garbage).
- **Resetti never appears after quitting without saving** (RESOLVED on
  dev 2026-07-19, device-verify pending; user report). Two port gaps
  vs GC (m_card.c:3329-3334 + the start-of-game card write): (a) the
  armed `reset_code` set at game start existed only in memory — PC wrote
  the GCI at session end only, so a no-save quit left the file with a
  cleared code and detection could never fire; (b) `mCD_SaveHome_bg`
  cleared the code on EVERY save, but GC clears it only on the final
  save-and-quit (`_04==0`) and keeps it armed for save-and-continue
  (param 1, aNRST door save). Fix (pc_m_card.c): persist the save right
  after arming in `mCD_InitGameStart_bg` (like GC's game-start card
  write), and make `mCD_SaveHome_bg` mirror GC param semantics (clear
  iff param_1==0, else keep/arm; dropped the old always-re-arm-in-memory
  block). Repro check: quit without saving → next load logs
  `[PC] Reset detected!` and Resetti shows; save-and-quit → no Resetti.
  `disable_resetti` setting unaffected (default 0). DEVICE-VERIFIED
  2026-07-19 (RG-34XX SP): user confirmed Resetti appears after a
  no-save quit.

- **Item dupe: save while holding a tool → reload → copy in hands AND
  pockets** (RESOLVED 2026-07-15; opened 2026-07-14, GCI forensics detail
  in git history of this file). Root cause: NOT a pipeline stow —
  `src/game/m_card.c` is excluded from the PC build (pc/CMakeLists.txt
  ~384), so the whole GC `mCD_InitGameStart_bg` pipeline, including the
  decoy-gated equipment clear (~5086), is dead code; the port's
  replacement in pc_m_card.c does no inventory writes. Exhaustive
  enumeration of every `equipment`/`pockets[]` writer in linked code
  proved no load-time stow exists; the save-while-holding state
  (equipment=0x2202, its pocket slot empty) simply survives the PC load —
  GC re-reads the card each start and never surfaces that state — and the
  held copy then gets re-materialized into the first free pocket slot
  in-session. Only non-atomic transaction on the whole surface: the
  submenu's deferred PLAYER-slot grab (`mTG_catch_item_from_table`
  m_tag_ovl.c:2917 arms shared `wait_timer=16`; completion+clear deferred
  to m_tag_ovl.c:8018-8028, cursor-state dependent, timer shared with the
  money-sack flows). Fix: pc_m_card.c `mCD_InitGameStart_bg` normalizes
  at game start — stows held `equipment` via `mPr_SetFreePossessionItem`
  and clears `equipment` in the same step (clear iff stow succeeded;
  pockets full → stays in hands, never destroyed). Held+pocket-copy state
  now unconstructible from a reload. DEVICE-VERIFIED 2026-07-15: user
  repro confirms hands empty + no new dupe after reload; GCI bytes
  confirm equipment=0x0000, shovel count stable (the pre-fix pair from
  the original dupe remains in pockets, as expected).
- Wrong resolution on 640×480 panels until users hand-edited the .sh
  (2026-07-15, RG35XX H report): launcher first-run settings.ini hardcoded
  720×480 (dev-device value). Now the launcher writes no
  window_width/height; the game queries SDL_GetCurrentDisplayMode(0) after
  SDL_Init when settings.ini sets no resolution key and fullscreen=1
  (640×480→preset 2, 720×480→preset 5, CubeXX 720×720→custom; existing
  letterbox/stretch handles aspect). Explicit ini values and in-game menu
  changes still win; saves keep resolution commented while in auto mode so
  cards stay portable across panels. Log:
  `[Settings] Auto-detected display WxH (window_size=N)`. Note: the three
  launcher .sh copies (port_files/, portmaster/, pm-submission/) are
  hand-maintained duplicates — assemble.sh does not copy them.
  DEVICE-VERIFIED 2026-07-15 (RG-34XX SP): fresh ini → 720×480 fullscreen
  window, saved ini carries `# window_width = 720` / `# window_size = 5`
  commented (auto mode persisted). The log line itself was missing from
  log.txt — the log's tail (all gameplay stdout) was truncated by an
  unclean unmount/power-off, so autodetect prints moved to stderr
  (matches pc_main boot-log convention). Lesson: quit via in-game exit
  and eject cleanly before reading device logs, or the buffered tail
  (incl. any [PROF] lines) is lost.
- In-game clock frozen on device: correct at boot/character select, then
  ~1 in-game min per 24 real min (2026-07-15, RG28XX user repro 6:03→6:04
  over 24 min) → u64 overflow in pc_os.c osGetTime():
  `elapsed_ns * GC_TIMER_CLOCK` wraps every 455.5s when
  SDL_GetPerformanceFrequency()=1e9 (Linux CLOCK_MONOTONIC), so the clock
  sawtooths: climbs 7.59 min then snaps back to boot time (24 min real →
  wrapped 73.6s elapsed = exactly the observed 6:04). All gameplay time
  (lbRTC_GetGameTime = OSGetTime + time_delta) sat downstream. Fixed by
  splitting whole seconds from remainder before tick scaling
  ((diff/freq)*CLK + (diff%freq)*CLK/freq) — overflow-free. If clock bugs
  recur, first suspect other cumulative-counter * constant multiplies.
  DEVICE-VERIFIED 2026-07-15: clock tracks real time through a full play
  session ("i think the clock is fixed too" — user, >10 min session).
- Intro train black screen → decomp code must build unoptimized (kb/perf.md).
- First-boot menu music stutter → shader seed warmup during splash.
- No audio → 32-bit PipeWire env in launcher (kb/device.md).
- Game running at 57% speed under load → fps_target must be dynamic (6);
  fixed targets tie game logic to render rate.
- Outdoor area locked at 30 fps until a house visit reset it → dynamic-fps
  governor was bistable (batch measurement); fixed with upward probe,
  kb/perf.md #11. If it recurs, check PC_NO_FPS_PROBE handling first.
