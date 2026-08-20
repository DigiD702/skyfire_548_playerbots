-- Add RBAC entry for the LFG scenario requirement debug command.

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 1010 OR `id` = 1010;
DELETE FROM `rbac_permissions` WHERE `id` = 1010;

INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1010, 'Command: debug lfg requirements');

INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1010);
