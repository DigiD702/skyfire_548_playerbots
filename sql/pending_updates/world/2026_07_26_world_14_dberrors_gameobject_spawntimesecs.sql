-- Set positive respawn for gameobjects marked despawn-at-action but with spawntimesecs 0.
UPDATE `gameobject` SET `spawntimesecs` = 60 WHERE `guid` IN (
101268, 101269, 101270, 101271, 101272, 101273, 101274, 101275, 101276, 101277, 101278, 101279, 101314, 101315, 101316, 101317, 101318, 101319, 101320, 101321, 101322, 101323, 101324, 101325, 101326, 101327, 101328, 101329, 101330, 101331
) AND `spawntimesecs` = 0;