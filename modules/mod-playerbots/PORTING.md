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
* [DONE] Bot auto-creator (`Playerbots.AutoCreate.*` / `.playerbots create`):
  provisions bot accounts and characters by faction and tank/healer/dps ratio at
  a configured start level, with unique generated names and correct realm id.
* [DONE] Specialization assignment: created/init bots (level >= 10) get the spec
  matching their role and learn its spells (`Player::LearnSpecialization`).
* [DONE] Gear initialisation via `.playerbots init`: clears and refills every
  slot with the best level/class/spec-appropriate item (armor type + primary
  stat aware; per-class weapon/shield/off-hand/ranged selection).
* [DONE] Role change via `.playerbots init <name> <tank|healer|dps>`: switches
  the bot's spec and re-gears it for the new role.
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
* [DONE] Per-spec rotation framework (`modules/mod-playerbots/src/rotations/`):
  priority picker with aura/resource/enemy-count helpers. DPS specs with
  hand-ported MoP Hekili `.simc` priorities: Ret, WW, BM/MM/Survival, Shadow,
  Affliction/Demo/Destro, Elemental/Enhancement, Feral, Arms/Fury, Combat Rogue,
  Frost Mage, Unholy DK. Wave 3: Balance, Guardian, Fire/Arcane, Assassination/
  Subtlety, Frost/Blood DK, Prot Pala/War, Brewmaster. Elemental/Balance ranged
  stance. `.playerbots init` learns recommended talent spells and accepts
  explicit names (`enhancement`, `feral`, `moonkin`, …). Local `Hekili/` is
  reference-only (gitignored).
* [DONE] Wave 4 healers + shared combat utilities:
  * Per-spec heals: Holy Pala, Disc/Holy Priest, Resto Shaman/Druid, Mistweaver
    (via `SelectNextHeal`, used by `HandleHealing`).
  * Shared `TryCombatUtilities`: class interrupt when the target is casting,
    offensive/defensive racials, on-use trinkets — wired into DPS rotation and
    healer combat.
* [DONE] World interaction (first pass):
  * Solo bots wander to nearby random ground points so they aren't frozen statues.
  * Bots auto-accept trade windows and duel challenges.
  * Bots walk to and loot nearby corpses they are allowed to take from (group
    loot rules still apply via `isAllowedToLoot`).
  * Bots walk to a nearby repairer and repair for free when equipped durability
    drops below 50%.
  * Rest: `eat`/`drink` or `nc +food` — sit and regenerate HP/mana (no spell spam);
    thresholds via `Playerbots.Rest.HealthPct` / `ManaPct`. Self-bots never get
    stood up by rest AI (AFK sit / clicked food&drink stay intact).
  * Healer strategies via `co +heal` / `co +healer dps` / `co +save mana`.
  * Bots whisper back their `co:` / `nc:` state on strategy commands.
* [DONE] Chat orders via whisper or party/raid chat:
  * `stay` / `follow` (also `come`) - hold position or resume following.
  * `flee` / `summon` - run to / teleport to the issuer.
  * `leave` - leave the current group.
  * `grind` - attack nearest hostiles; `reset` clears orders/casts.
  * `passive` / `aggressive` - stop assisting (still retaliate) or resume normal
    assist behaviour.
  * `attack` - all bots attack the issuer's current target.
  * `tank attack` / `dps attack` - same, filtered by the bot's combat role
    (from its active specialization).
  * `maintenance` / `autogear` - re-run `InitializeBot` (spec + gear).
  * Party filters: `@tank` / `@dps` / `@heal` / `@ranged` before an order.
  * `help` - list orders (whisper reply). Whisper commands get a short ack;
    party/raid orders apply silently to avoid spam.
* [DONE] Self-bot mode (`.playerbots self`): attach cast-only AI to a real
  logged-in player. Client keeps movement; AI casts fillers / per-spec rotations.
  `.playerbots init` with no args gears yourself and grouped bots.
* [DONE] **Thin AC-style strategy engine** (`src/engine/BotStrategyEngine`):
  * Named strategy sets per `BotState` (Combat / NonCombat), source of truth for
    `co` / `nc` (+/-/~/?, comma lists).
  * Chat shortcuts apply AC packs: `follow`, `stay`, `flee`, `grind`, `passive`,
    `aggressive`, `reset` rewrite both engines like AC `ChatShortcutActions`.
  * Procedural AI still reads synced flags (`_passive`, `_stay`, …) so MoP
    rotations and movement keep working.
  * DPS default includes `+dps assist` (group assist); `-dps assist` = own
    aggro / forced targets only.
* [DONE] **Trigger → Action → Queue** (`src/engine/BotAiEngine` + Action/Trigger/
  Queue/Multiplier): AC-shaped tick selects combat/rest/follow/stay/loot/wander.
  `PassiveMultiplier` zeroes combat when `+passive`. MoP `rotations/` unchanged.
* [DONE] **Target Values** (`BotTargetValues`): pull / current / dps (least HP) /
  tank / assist-tank — SelectTarget reads these like AC AiObjectContext values.
* [DONE] **wait for attack** (`co +wait for attack`, default on for DPS): non-tanks
  hold damage for `Playerbots.WaitForAttack.Seconds` after combat starts; still
  fight back if attacked. Tanks ignore. Disable with `co -wait for attack`.
* [DONE] **Role formations**: follow angle/distance by tank / healer / melee DPS /
  ranged DPS (`BotFormation` + `BotMovement::MoveFollowLeader`).
* [TODO] Richer AC actions (flee manager, RTI icons, pull sequences).
* [DONE] Class/raid buff maintenance (self + party equivalents), recommended
  major/minor glyphs on `.playerbots init` (3+3 per spec).
* [DONE] Deeper cooldowns/DoTs/AoE on thinner DPS lines (Balance, Destro/Demo,
  Subtlety, MM, Arms, Feral). Still TODO: trinket sync polish, glyph-aware
  conditional lines.
* [DONE] Party resurrection: Priest/Pala/Shaman/Druid/Monk OOC rez, Druid
  Rebirth / DK Raise Ally in combat; dead bots auto-accept rez requests.
* [TODO] Point movement / travel to arbitrary destinations (for questing,
  objectives, and dungeon navigation).
* [TODO] Expand the co/nc strategy set further (aoe/boost/cc/avoid aoe, etc.).
  Role-gated tank/heal/dps strategies, self-whisper, and spell refreshment are live.
* [TODO] AC wiki backlog (later): RTSC/aedm, loot lists (`ll`), item/vendor
  chat ops, glyphs, raid-specific strats, Multibot addon protocol.
* [TODO] Non-combat behaviour remaining: sell junk to vendors,
  mounts, gossip/quest NPC interaction.

## Reference tree

Upstream AC module checked into the repo root as `mod-playerbots/` (study /
command semantics). SkyFire runtime module remains `modules/mod-playerbots/`.
Do not compile the root tree into worldserver.

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
* [DONE] Between-pull food: socket + self-bots sit and cast Refreshment
  (`128701`, HP+mana) — no bag food/drink. Cancels at full resources.
* [IN PROGRESS] In-dungeon behaviour: tank assist / threat throttle / party rest
  between pulls. Boss/trash scripting still thin — depends on deeper rotations.
* [DONE] Loot rolls: bots auto Need/Greed/Pass on GROUP_LOOT / NEED_BEFORE_GREED
  (LFG). Corpse loot peels briefly when OOC with `nc +loot`.
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

### Rotation coverage

* [DONE] All 34 MoP specializations have combat (or heal) priority lists.
* Healer specs also have `*Dps` damage lines for `co +healer dps`.
* Trinkets sync with burst windows; Elemental/Assassination/Mage lines deepened
  (AoE, bombs, Ascendance Lava Beam, Marked for Death). Balance/Destro/Demo/
  Subtlety/MM/Arms/Feral also deepened (Incarnation, Havoc, Infernal/Doomguard,
  Hellfire, Vanish/MfD, Murder of Crows, Avatar/Skull Banner).
* [DONE] `.playerbots init` applies recommended major/minor glyphs per spec;
  `TryMaintainBuffs` keeps MotW/Fort/Brilliance/shouts/blessings/etc. up.
* [TODO] Further SimC-faithful tuning, glyph-aware rotation branches, and
  boss-specific holds.
