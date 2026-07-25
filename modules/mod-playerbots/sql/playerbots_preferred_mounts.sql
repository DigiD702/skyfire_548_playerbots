-- Preferred mount spells per bot character (characters database).
-- Applied automatically on module load via CREATE TABLE IF NOT EXISTS;
-- this file is the reference schema for tooling / manual apply.

CREATE TABLE IF NOT EXISTS `playerbots_preferred_mounts` (
  `id` INT(11) NOT NULL AUTO_INCREMENT,
  `guid` INT(11) NOT NULL,
  `type` TINYINT(3) NOT NULL COMMENT '0: Ground, 1: Flying',
  `spellid` INT(11) NOT NULL,
  PRIMARY KEY (`id`),
  KEY `guid` (`guid`),
  KEY `type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
