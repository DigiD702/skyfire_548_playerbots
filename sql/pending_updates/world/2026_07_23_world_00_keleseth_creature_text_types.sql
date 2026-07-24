-- Fix Prince Keleseth creature_text chat types.
-- All lines were typed as MONSTER_WHISPER (15). Boss combat lines should yell;
-- the Frost Tomb cast line is an emote. Whisper type + MoP chat packing had
-- been aborting the worldserver when Talk(SAY_FROST_TOMB_EMOTE) ran.

UPDATE `creature_text` SET `type` = 14 WHERE `entry` = 23953 AND `groupid` IN (1, 2, 3, 5);
UPDATE `creature_text` SET `type` = 16 WHERE `entry` = 23953 AND `groupid` = 4;
