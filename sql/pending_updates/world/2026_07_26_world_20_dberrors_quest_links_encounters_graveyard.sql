-- Remove creature quest starter/ender links for quest IDs absent from quest_template.
-- Timeless Isle / related quests exist in reference data but are not yet ported to this schema.
DELETE FROM `creature_queststarter` WHERE `quest` IN (31521,32603,32616,32617,33098,33133,33134,33161,33228,33234,33235,33236,33238,33239,33250,33342,36608,36609,36882,38935,38936,39127);
DELETE FROM `creature_questender` WHERE `quest` IN (31521,32603,32616,32617,33098,33133,33134,33161,33228,33234,33235,33236,33238,33239,33250,33342,36608,36609,36882,38935,38936,39127);

-- Clear invalid encounter credit spells (absent from Spell DBC); kill-credit remains via scripts where present.
UPDATE `instance_encounters` SET `creditEntry` = 0 WHERE `entry` IN (296,300,334,336,338,339,567,568,1086,1121,1133,1135,1141) AND `creditType` = 1 AND `creditEntry` IN (58630,68572,68574,59046,68184,59450,65074,64899,64985);

-- Remove invalid graveyard zone id.
DELETE FROM `game_graveyard_zone` WHERE `ghost_zone` = 8505;
