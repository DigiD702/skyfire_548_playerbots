-- Clear creature lootid values with no loot template available.
UPDATE `creature_template` SET `lootid` = 0 WHERE `entry` IN (61430) AND `lootid` > 0;
