-- Clear difficulty-only SmartScript event flags on Wailing Caverns combat AI
-- so events load in all spawn modes instead of leaving an empty event list.
UPDATE `smart_scripts`
SET `event_flags` = `event_flags` & ~0x1E
WHERE `source_type` = 0
  AND `entryorguid` IN (5055, 5761, 3636, 5053, 5756, 5048, 5755, 3674, 5775)
  AND (`event_flags` & 0x1E) <> 0;

-- Sync difficulty template unit_class and npcflag with the normal entry
-- so difficulty links are accepted by the creature template validator.
UPDATE `creature_template` AS `d`
INNER JOIN `creature_template` AS `c` ON `d`.`entry` = `c`.`difficulty_entry_1`
SET `d`.`unit_class` = `c`.`unit_class`,
    `d`.`npcflag` = `c`.`npcflag`
WHERE `c`.`difficulty_entry_1` > 0
  AND (`d`.`unit_class` <> `c`.`unit_class` OR `d`.`npcflag` <> `c`.`npcflag`);

UPDATE `creature_template` AS `d`
INNER JOIN `creature_template` AS `c` ON `d`.`entry` = `c`.`difficulty_entry_2`
SET `d`.`unit_class` = `c`.`unit_class`,
    `d`.`npcflag` = `c`.`npcflag`
WHERE `c`.`difficulty_entry_2` > 0
  AND (`d`.`unit_class` <> `c`.`unit_class` OR `d`.`npcflag` <> `c`.`npcflag`);

UPDATE `creature_template` AS `d`
INNER JOIN `creature_template` AS `c` ON `d`.`entry` = `c`.`difficulty_entry_3`
SET `d`.`unit_class` = `c`.`unit_class`,
    `d`.`npcflag` = `c`.`npcflag`
WHERE `c`.`difficulty_entry_3` > 0
  AND (`d`.`unit_class` <> `c`.`unit_class` OR `d`.`npcflag` <> `c`.`npcflag`);
