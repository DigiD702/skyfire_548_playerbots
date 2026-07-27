-- Remap remaining gossip ActionPoiID values to existing same-city directory POIs.

UPDATE `gossip_menu_option` SET `ActionPoiID`=456 WHERE `ActionPoiID`=495; -- Archaeology -> Ironforge Archaeology
UPDATE `gossip_menu_option` SET `ActionPoiID`=361 WHERE `ActionPoiID`=496; -- Silvermoon profession directory
UPDATE `gossip_menu_option` SET `ActionPoiID`=335 WHERE `ActionPoiID`=497; -- Warrior (Undercity warriors terrace area)
UPDATE `gossip_menu_option` SET `ActionPoiID`=335 WHERE `ActionPoiID` IN (498,499); -- Undercity warrior/shaman area
UPDATE `gossip_menu_option` SET `ActionPoiID`=266 WHERE `ActionPoiID`=500; -- Darnassus Paladin
UPDATE `gossip_menu_option` SET `ActionPoiID`=456 WHERE `ActionPoiID`=501; -- Archaeology
UPDATE `gossip_menu_option` SET `ActionPoiID`=44 WHERE `ActionPoiID`=502; -- Engineering
UPDATE `gossip_menu_option` SET `ActionPoiID`=74 WHERE `ActionPoiID`=503; -- Jewelcrafting
UPDATE `gossip_menu_option` SET `ActionPoiID`=300 WHERE `ActionPoiID` IN (504,506); -- Valley of Honor
UPDATE `gossip_menu_option` SET `ActionPoiID`=176 WHERE `ActionPoiID`=505; -- Valley of Strength / bank area
UPDATE `gossip_menu_option` SET `ActionPoiID`=18 WHERE `ActionPoiID`=555; -- SW AH
UPDATE `gossip_menu_option` SET `ActionPoiID`=19 WHERE `ActionPoiID`=556; -- SW Bank
UPDATE `gossip_menu_option` SET `ActionPoiID`=22 WHERE `ActionPoiID`=557; -- Trade District Inn
UPDATE `gossip_menu_option` SET `ActionPoiID`=22 WHERE `ActionPoiID`=558; -- Dwarven District Inn
UPDATE `gossip_menu_option` SET `ActionPoiID`=28 WHERE `ActionPoiID`=559; -- Champions' Hall
UPDATE `gossip_menu_option` SET `ActionPoiID`=21 WHERE `ActionPoiID`=560; -- Deeprun Tram
UPDATE `gossip_menu_option` SET `ActionPoiID`=39 WHERE `ActionPoiID`=562; -- Stockade / Barracks area
UPDATE `gossip_menu_option` SET `ActionPoiID`=24 WHERE `ActionPoiID`=563; -- Stormwind Keep / visitor center area
UPDATE `gossip_menu_option` SET `ActionPoiID`=26 WHERE `ActionPoiID` IN (564,565); -- Stable Master
UPDATE `gossip_menu_option` SET `ActionPoiID`=29 WHERE `ActionPoiID` IN (567,568); -- Quartermasters / battlemasters
UPDATE `gossip_menu_option` SET `ActionPoiID`=33 WHERE `ActionPoiID`=570; -- Hunter Trainer
UPDATE `gossip_menu_option` SET `ActionPoiID`=97 WHERE `ActionPoiID`=571; -- Darnassus Battlemaster
UPDATE `gossip_menu_option` SET `ActionPoiID`=456 WHERE `ActionPoiID`=572; -- Archaeology
UPDATE `gossip_menu_option` SET `ActionPoiID`=44 WHERE `ActionPoiID`=573; -- Engineering
UPDATE `gossip_menu_option` SET `ActionPoiID`=74 WHERE `ActionPoiID`=574; -- Jewelcrafting
UPDATE `gossip_menu_option` SET `ActionPoiID`=48 WHERE `ActionPoiID`=575; -- Mining
UPDATE `gossip_menu_option` SET `ActionPoiID`=37 WHERE `ActionPoiID`=576; -- Shaman
UPDATE `gossip_menu_option` SET `ActionPoiID`=38 WHERE `ActionPoiID`=577; -- Warlock
UPDATE `gossip_menu_option` SET `ActionPoiID`=34 WHERE `ActionPoiID`=579; -- Mage
UPDATE `gossip_menu_option` SET `ActionPoiID`=37 WHERE `ActionPoiID`=581; -- Shaman
UPDATE `gossip_menu_option` SET `ActionPoiID`=33 WHERE `ActionPoiID`=583; -- Hunter
UPDATE `gossip_menu_option` SET `ActionPoiID`=35 WHERE `ActionPoiID` IN (588,589); -- Paladin/Priest
UPDATE `gossip_menu_option` SET `ActionPoiID`=181 WHERE `ActionPoiID`=591; -- AH/Bank Orgrimmar
UPDATE `gossip_menu_option` SET `ActionPoiID`=179 WHERE `ActionPoiID` IN (592,593,595); -- Inn / general goods area
UPDATE `gossip_menu_option` SET `ActionPoiID`=307 WHERE `ActionPoiID` IN (597,598,599); -- Herb/Inscription/Fishing near alchemy
UPDATE `gossip_menu_option` SET `ActionPoiID`=98 WHERE `ActionPoiID`=600; -- Druid
UPDATE `gossip_menu_option` SET `ActionPoiID`=99 WHERE `ActionPoiID`=601; -- Hunter
UPDATE `gossip_menu_option` SET `ActionPoiID`=265 WHERE `ActionPoiID`=602; -- Mage
UPDATE `gossip_menu_option` SET `ActionPoiID`=266 WHERE `ActionPoiID`=603; -- Priest (closest)
UPDATE `gossip_menu_option` SET `ActionPoiID`=32 WHERE `ActionPoiID`=609; -- Lion's Rest / The Park

-- Clear ActionPoiID only where target still missing and no safe remap remains.
UPDATE `gossip_menu_option` g
LEFT JOIN `points_of_interest` p ON p.entry = g.ActionPoiID
SET g.ActionPoiID = 0
WHERE g.ActionPoiID > 0 AND p.entry IS NULL;
