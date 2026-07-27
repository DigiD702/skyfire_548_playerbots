-- Fix linked_respawn spawnMask pairs that are both 0 (bitwise AND never matches).
-- Classic raid trash/boss links on Molten Core and Blackwing Lair need normal (1).
UPDATE `creature` c
INNER JOIN `linked_respawn` lr ON lr.guid = c.guid OR lr.linkedGuid = c.guid
SET c.spawnMask = 1
WHERE c.spawnMask = 0
  AND c.map IN (409, 469);