-- Remove creature_loot_template rows for item IDs absent from item_template (post-expansion contamination).
DELETE FROM `creature_loot_template` WHERE `item` IN (120950, 120954, 122199, 132569, 132570, 151074, 151075, 151076, 151077);
DELETE FROM `gameobject_loot_template` WHERE `item` IN (120950, 120954, 122199, 132569, 132570, 151074, 151075, 151076, 151077);