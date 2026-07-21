/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*
* Module loader. Discovered and invoked automatically by AddModulesScripts().
* The function name MUST match the folder name (mod-playerbots -> mod_playerbots).
*/

// Registration functions provided by this module.
void AddSC_playerbot_scripts();

// Aggregate loader for the mod-playerbots module.
void Addmod_playerbotsScripts()
{
    AddSC_playerbot_scripts();
}
