-- Add scenario metadata needed by Dungeon Finder scenario teleports.

CREATE TABLE IF NOT EXISTS `lfg_dungeon_template` (
    `dungeonId` INT UNSIGNED NOT NULL,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    `requiredItemLevel` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`dungeonId`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DELETE FROM `instance_template` WHERE `map` IN (999, 1000);
INSERT INTO `instance_template` (`map`, `parent`, `script`, `allowMount`) VALUES
(999, 870, 'instance_fall_of_theramore_horde', 1),
(1000, 870, 'instance_fall_of_theramore_alliance', 0);

INSERT INTO `lfg_dungeon_template`
    (`dungeonId`, `position_x`, `position_y`, `position_z`, `orientation`, `requiredItemLevel`)
VALUES
    (542, -3813.67, -4790.98, 10.5307, 0.805, 425),
    (543, -4023.08, -4748.8, 5.21768, 0.781, 425)
ON DUPLICATE KEY UPDATE
    `position_x` = VALUES(`position_x`),
    `position_y` = VALUES(`position_y`),
    `position_z` = VALUES(`position_z`),
    `orientation` = VALUES(`orientation`),
    `requiredItemLevel` = VALUES(`requiredItemLevel`);
