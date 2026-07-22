# Playerbot Commands

Reference for SkyFire `mod-playerbots`. Commands fall into two groups:

1. **GM slash commands** — typed in chat as `.playerbots …` (require GM permission).
2. **Chat orders** — whisper a bot, or say them in party/raid chat so every grouped bot hears them.

Whisper replies with a short ack. Party/raid orders apply silently (no spam).

---

## GM slash commands (`.playerbots …`)

| Command | What it does |
| --- | --- |
| `.playerbots status` | Module on/off, random-bot counts, active bot count. |
| `.playerbots list` | Names/GUIDs of active (socket) bots. |
| `.playerbots add <name>` | Log an offline character in as a full bot session. |
| `.playerbots remove <name>` | Log that bot out and free its session. |
| `.playerbots summon` | Teleport all bots in your group to you. |
| `.playerbots reload` | Reload `playerbots.conf` and the random-bot candidate pool. |
| `.playerbots create` | Run the auto-creator (accounts + characters from config). |
| `.playerbots init [<name>\|all\|self] [tank\|healer\|dps\|<spec>] [rare\|epic\|…]` | Re-apply specialization, talents, and gear (optional quality cap). |
| `.playerbots self [on\|off]` | Attach/detach cast-only AI on **your** character. |

### `.playerbots init`

Re-gears and re-applies spec spells for the current level. For Wave-1 DPS
specs it also learns a recommended talent spell set used by the rotation.

| Args | Effect |
| --- | --- |
| *(none)* | Initialize **yourself**, then every bot in your group. |
| `self` | Initialize only yourself. |
| `<charname>` | Initialize that active bot (or self-bot). |
| `all` | Initialize every active socket bot. |
| `… tank` / `healer` / `dps` | Switch to that role’s **default** spec, then gear. |
| `… <spec>` | Switch to an explicit specialization (needed for hybrid DPS). |
| `… rare` / `epic` / … | Cap gear quality (default **epic**). Also: `blue`, `purple`, `quality=rare`. |

Tokens may be in any order, e.g. `.playerbots init self fury rare` or `.playerbots init rare self fury`.

**Hybrid DPS:** `dps` alone is not enough when a class has two damage specs:

| Class | `dps` defaults to | Explicit specs |
| --- | --- | --- |
| Shaman | Elemental | `elemental` / `ele`, `enhancement` / `enh` |
| Druid | Balance (moonkin) | `moonkin` / `balance`, `feral` |
| Druid tank/heal | — | `guardian`, `resto` / `healer` |

Other useful tokens: `ret`, `shadow`, `bm`, `ww`, `aff`, `arms`, `fury`, `frost`, etc.

### Combat rotations

Bots (and `.playerbots self`) use per-spec priority lists for:

**Wave 1:** Retribution, Windwalker, Beast Mastery, Shadow, Affliction, Elemental

**Wave 2:** Enhancement, Feral, Marksmanship, Survival, Arms, Fury, Combat Rogue,
Frost Mage, Destruction, Demonology, Unholy DK

**Wave 3:** Balance, Guardian, Fire, Arcane, Assassination, Subtlety, Frost DK,
Blood DK, Prot Paladin, Prot Warrior, Brewmaster

Healer specs still lean on the group heal AI (below) plus class fillers. Priorities
were simplified from MoP Hekili/SimC lists (local `Hekili/` reference only; not shipped).

### Tank / healer in groups

- **Tanks** peel mobs attacking party members and taunt when they lose threat.
- **Healers** prioritize injured group members (below ~90% HP) before dealing damage.

Init bots to the right role before queueing: `.playerbots init Botname tank` /
`healer` / `dps` (or an explicit spec).

### LFG / LFR party roles

MoP blocks dungeon finder join until every party member has a **party role** set
(tank / healer / dps). Bots now set that from their active specialization as soon
as they are grouped, and still auto-accept the LFG role check + ready proposal.

When a real player queues (solo or with bots) and the dungeon forms, that player
is preferred as **LFG group leader** so bots auto-follow them like a normal party.

Queue needs a workable mix (at least one tank + healer for normal dungeons). All-DPS
parties will fail the role check with wrong roles.

**Solo LFG fill (optional):** set `Playerbots.RandomBotJoinLfg = 1` in
`playerbots.conf`. When a real player solo-queues, online bots within
`RandomBotJoinLfg.LevelRange` of that player (and inside the dungeon level bracket)
also join the same LFG selection. Premade parties still use grouped bots only.

Init also teaches level-gated armor proficiency (mail at 40 for hunter/shaman,
plate at 40 for warrior/paladin, etc.) and gears by **RequiredLevel** near the
bot’s level (so a level 90 no longer gets ~70 ilvl from the old ItemLevel cap).

Examples:

```
.playerbots init
.playerbots init self enhancement
.playerbots init self moonkin
.playerbots init self feral
.playerbots init self fury rare
.playerbots init rare self fury
.playerbots init Arix elemental
.playerbots init all dps
```

### `.playerbots self`

Attaches AI to your logged-in character **without** replacing the client session:

- You keep WASD / jump / camera.
- AI picks combat targets and casts class filler / Wave-1 rotation spells when you are in range with LoS.
- Toggle again, or `.playerbots self off`, to detach.

Useful for testing rotations and for `.playerbots init` gearing yourself.

---

## Chat orders (whisper or party/raid)

Send these as the message text (case-insensitive).

| Order | What it does |
| --- | --- |
| `help` | Whisper back the order list. |
| `stay` | Hold position (no follow/wander). Self-bot: no-op (you move). |
| `follow` / `come` | Resume following the group leader. Self-bot: no-op. |
| `flee` | Clear combat orders and teleport to the issuer. Self-bot: no-op. |
| `leave` | Leave the current group. Self-bot: ignored. |
| `summon` | Teleport the bot to the issuer. Self-bot: ignored. |
| `grind` | Aggressive; pick nearest attackable hostiles. |
| `reset` | Clear stay/passive/grind/forced target, stop attack/casts. |
| `passive` | Do not assist or pull; still fight back if attacked. |
| `aggressive` / `aggro` | Resume normal assist behaviour. |
| `attack` | Attack the **issuer’s** current target. |
| `tank attack` | Same, but only bots whose active spec is a tank role. |
| `dps attack` | Same, but only damage-spec bots. |
| `maintenance` | Re-run init (spec + gear) on that bot. |
| `autogear` | Same as `maintenance`. |

### Party role filters

Prefix an order with a filter so only matching bots react (party/raid):

| Filter | Who reacts |
| --- | --- |
| `@tank …` | Tank-spec bots |
| `@dps …` | Damage-spec bots |
| `@heal …` / `@healer …` | Healer-spec bots |
| `@ranged …` | Hunter / priest / mage / warlock / Elemental / Balance (non-tank) |

Examples:

```
/w BotName stay
/p follow
/p attack
/p @tank attack
/p @dps follow
/r grind
```

---

## `co` / `nc` strategies — not implemented

AzerothCore playerbots use whisper commands like:

```
co +grind,-follow
nc +loot
co ?
```

Those control a full **combat (`co`) / non-combat (`nc`) strategy engine**.

**SkyFire mod-playerbots does not support `co` or `nc` yet.** Whispering them does nothing useful (they are not recognized orders). That engine is tracked as future work in `PORTING.md` (along with RTSC, loot lists, item/vendor chat ops, etc.).

Closest equivalents today:

| AC-style idea | Use instead |
| --- | --- |
| Stay put | `stay` |
| Follow master | `follow` |
| Attack target | `attack` / `tank attack` / `dps attack` |
| Attack anything nearby | `grind` |
| Stop assisting | `passive` |
| Resume assist | `aggressive` |
| Clear bot state | `reset` |
| Re-gear / talents refresh | `autogear` / `maintenance` or `.playerbots init` |

---

## Quick start

1. Enable the module in `playerbots.conf` (`Playerbots.Enable = 1`).
2. `.playerbots add SomeOfflineChar` — or let random bots spawn from config.
3. Invite bots, then `/p follow` or `/p attack` with a mob selected.
4. Optional: `.playerbots self` on your own character to test casting while you move.
5. `.playerbots init` after leveling to refresh gear/spec (use `enhancement` / `feral` / `moonkin` when you need a specific hybrid DPS tree).

See `README.md` for build/config and `PORTING.md` for port status and backlog.
