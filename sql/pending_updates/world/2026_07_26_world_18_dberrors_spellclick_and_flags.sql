-- Restore npc_spellclick_spells and clear orphaned SPELLCLICK flags.
REPLACE INTO `npc_spellclick_spells` VALUES (64526,126022,1,0),(65736,118977,3,1),(72192,145671,3,0),(72358,93970,1,0),(72656,145752,0,0),(73536,148032,3,0);

-- Clear SPELLCLICK npcflag where no spellclick data exists.
UPDATE `creature_template`
SET `npcflag` = `npcflag` & ~0x1000000
WHERE `entry` IN (62382,65253,71082,72045,72588)
  AND (`npcflag` & 0x1000000) <> 0;