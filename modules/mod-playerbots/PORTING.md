# Playerbots port plan (AzerothCore/WotLK -> SkyFire/MoP 5.4.8)

This document tracks the staged effort to bring the AzerothCore playerbots
system to SkyFire. The upstream project depends on a **forked core** with extra
scripting hooks plus a very large module (bot sessions, AI, strategies, actions,
triggers, a random-bot pool, and gear/talent generation). It cannot be copied
verbatim because SkyFire is a different expansion (5.4.8) with different core
APIs, spell/talent systems, LFG internals and opcodes.

The work is therefore split into independently testable phases.

## Phase 0 - Module system (DONE)

* Added an AzerothCore-style module system to SkyFire (`modules/` +
  `AddModulesScripts()` core hook). See `modules/README.md`.
* Registered `mod-playerbots` with config, manager and `.playerbots` command.

## Phase 1 - Core hooks (the "forked core" equivalent) (IN PROGRESS)

Upstream relies on core changes. On SkyFire these are added as minimal,
well-isolated hooks (prefer new `ScriptMgr` hooks over scattered edits):

* [DONE] Socketless `WorldSession` ("bot session"): a `SetBot/IsBot` flag,
  null-socket guards in `WorldSession::Update`, and `SendPacket` already no-ops
  without a socket. Bot sessions are NOT tracked in `World::m_sessions`; they are
  owned by `PlayerbotMgr`.
* [DONE] Synchronous character login for a session:
  `WorldSession::LoginBotCharacter(guid)` (in `CharacterHandler.cpp`) builds the
  login query holder, waits for it, and calls `HandlePlayerLogin`.
* [DONE] Per-`Player` tick hook: `PlayerScript::OnUpdate(Player*, uint32)`,
  dispatched from `Player::Update` via `ScriptMgr::OnPlayerUpdate`. Because bot
  players are updated by their `Map`, this fires for bots automatically - the
  attach point for Phase 3 AI.
* [DONE] `PlayerbotMgr` bot lifecycle + `.playerbots add/remove/list` commands,
  with cleanup on world shutdown.

Remaining for later phases:

* Outgoing packet interception so bot AI can react to server packets it would
  normally receive as a client (Phase 3).
* Group/LFG/Battleground entry points reachable programmatically for bots
  (Phase 4).

## Phase 2 - Bot lifecycle (IN PROGRESS)

* [DONE] Random-bot pool sourced from dedicated bot accounts
  (`Playerbots.RandomBots.AccountPrefix`). Candidates are all characters on
  accounts whose username matches the prefix (looked up across the auth and
  character databases).
* [DONE] `PlayerbotMgr::Update()` driven from the module's
  `WorldScript::OnUpdate` (no core change): keeps up to
  `Playerbots.RandomBots.MaxBots` online, spawns one bot per
  `Playerbots.RandomBots.LoginInterval` ms, trims excess when the cap is lowered,
  and cleans up bots whose player left the world.
* [DONE] `.playerbots reload` (re-reads config + candidate pool) and richer
  `.playerbots status` (random/active/candidate counts).
* [TODO] Per-master "alt bots" (a real player summoning their own alts).
* [TODO] Async login path for the pool (current logins reuse the synchronous
  `LoginBotCharacter`, throttled to one per interval to keep world-thread
  hitching negligible).

## Phase 3 - Bot AI framework (IN PROGRESS)

* [DONE] `PlayerbotAI` controller: one instance per bot, created in
  `PlayerbotMgr::SpawnBot` and destroyed with the bot. Ticked from
  `PlayerScript::OnUpdate` via `PlayerbotMgr::UpdateBotAI` (throttled to ~500ms
  per bot). This is the attach point for the strategy engine.
* [DONE] Auto-accept group/raid invites (mirrors `HandleGroupAcceptOpcode`) so
  bots can be pulled into parties, and thus LFG/LFR/RBG queues.
* [DONE] Defensive combat: bots keep swinging at a valid victim, retaliate
  against attackers, and chase the target into melee range.
* [DONE] Server-side movement: bots follow the group leader out of combat
  (spread around the leader by a GUID-derived angle) and chase in combat. State
  is tracked so generators aren't re-issued every tick.
* [DONE] Teleport/summon: bots warp to the leader when they're on another map or
  too far to catch up on foot (auto), and `.playerbots summon` teleports all of
  the caller's grouped bots to their position (manual). Because bots have no
  client to send the teleport ack, the core exposes
  `WorldSession::FinalizeBotTeleport()` (mirrors the near/far worldport ack
  handlers) which the module calls right after `Player::TeleportTo` - without it
  the bot got stuck "being teleported" forever.
* [DONE] Combat target acquisition/assist: bots pick a unit attacking them, and
  otherwise assist the group leader's target (within 60y) so the party
  focus-fires the same mob.
* [DONE] Basic class rotation: melee classes close in and auto-attack (paladin
  and rogue also use a filler strike); ranged/caster classes (hunter, priest,
  mage, warlock) hold at ~25y and spam a single filler spell. Bots only cast
  spells they actually know; casts go through the normal path so GCD/power/range/
  LoS are validated by the core.
* [TODO] Per-spec rotations: cooldowns, dots/procs, AoE, interrupts, dispels,
  healing, tanking, resource management (the large, iterative piece). Warrior,
  DK, shaman, monk, druid are melee-only auto-attack until their specs land.
* [TODO] Point movement / travel to arbitrary destinations (for questing,
  objectives, and dungeon navigation).
* [TODO] Port the strategy/action/trigger/value engine on top of `PlayerbotAI`.
* [TODO] Targeting + combat rotations per class/spec (rebuilt for 5.4.8 spells
  and talents - the largest single piece).
* [TODO] Non-combat behaviour: loot, vendor, repair, rest, travel.

## Phase 4 - Feature testing targets (the reason for this port) (IN PROGRESS)

Merged the `lfg_mechanics` branch (LFG/LFR/scenario/challenge-mode work) into
`playerbots` so bots can use the existing dungeon finder. `lfg_mechanics` is kept
for future updates.

* [DONE] LFG auto-response: the master queues a party (normal client UI) and the
  bots auto-answer the group role check and accept the join proposal. Role check
  uses a deterministic, group-aware assignment (first tank-capable bot tanks,
  first healer-capable heals, rest dps) so a bot party forms a valid composition
  without communicating. Core exposes `LFGMgr::GetActiveProposalIdForPlayer` so
  the module can find a bot's pending proposal to accept.
* [TODO] In-dungeon behaviour: bots need real tanking/healing/threat and
  boss/trash handling to actually *complete* a dungeon (depends on per-spec
  rotations above).
* [TODO] LFR: bots fill raid finder queues (auto-accept the LFR prompt/roles).
* [TODO] Random battlegrounds / rated battlegrounds and arenas.
* [TODO] Open-world grinding/questing for population.

## Phase 5 - Content data

* Bot equipment/talent templates appropriate to 5.4.8.
* SQL for any module-owned tables (kept under `sql/`, applied by your tooling).

## Notes on reuse

* The upstream AI is data/logic heavy but many *strategies* are conceptually
  portable; the class-specific *spell IDs and rotations* must be rebuilt for MoP.
* Keep core edits behind `ScriptMgr` hooks where possible so the core diff stays
  reviewable and this module remains self-contained.
