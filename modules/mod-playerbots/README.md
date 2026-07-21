# mod-playerbots (SkyFire)

A port of the AzerothCore playerbots system to SkyFire (World of Warcraft
5.4.8 / Mists of Pandaria), intended to populate a test server with bot players
so features such as LFG, LFR, random battlegrounds, arenas and open-world
activity can be exercised without a large human population.

Upstream references:

* Core fork:  https://github.com/mod-playerbots/azerothcore-wotlk/tree/Playerbot
* Module:     https://github.com/mod-playerbots/mod-playerbots

## Status

This module is being ported in phases. What is present today:

* Integrated into SkyFire's new module system (builds + links automatically).
* Configuration (`playerbots.conf.dist`) and a `PlayerbotMgr` that reads it.
* Phase 1 core hooks: socketless bot `WorldSession`, synchronous character
  login, and a per-player tick hook (`PlayerScript::OnUpdate`).
* Commands (GM level):
  * `.playerbots status` - report module state and active bot count.
  * `.playerbots add <charname>` - log an existing character in as a bot.
  * `.playerbots remove <charname>` - log a bot out.
  * `.playerbots summon` - teleport all of your grouped bots to you (in-game).
  * `.playerbots list` - list active bots.
  * `.playerbots reload` - reload config and the candidate bot pool.

What is **not** implemented yet (tracked in `PORTING.md`):

* Bot AI, strategies, actions and triggers (bots currently just stand in world).
* Random-bot pool and the automated LFG/LFR/RBG queue behaviour.

To try it: set `Playerbots.Enable = 1`, then `.playerbots add <charname>` for a
character that is offline. The bot appears in the world at its saved location.

## Building

Nothing special is required. With the SkyFire module system enabled
(`-DMODULES=1`, the default), this folder is compiled into the `modules`
library and linked into `worldserver` automatically.

## Configuration

Copy `playerbots.conf.dist` to `playerbots.conf` next to `worldserver.conf`
(the build also stages a copy next to the binary) and set `Playerbots.Enable = 1`.

## Why this is a port, not a drop-in

The upstream module targets AzerothCore, which is WotLK (3.3.5a). SkyFire is
MoP (5.4.8): the class/spell system, talent trees, LFG internals, packet/opcode
layout, and many core APIs differ substantially. Upstream also requires a
*forked core* with extra hooks, so a straight copy does not compile. `PORTING.md`
describes the staged approach used here.
