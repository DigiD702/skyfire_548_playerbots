<p align="center">
  <img src="contrib/skyfire_transp.png" alt="Project SkyFire logo" width="520">
</p>

## Build Status
[![windows-build](https://github.com/ProjectSkyfire/SkyFire_548/actions/workflows/windows.yml/badge.svg)](https://github.com/ProjectSkyfire/SkyFire_548/actions/workflows/windows.yml)
[![ubuntu-build](https://github.com/ProjectSkyfire/SkyFire_548/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/ProjectSkyfire/SkyFire_548/actions/workflows/ubuntu.yml)
[![CodeFactor](https://www.codefactor.io/repository/github/projectskyfire/skyfire_548/badge)](https://www.codefactor.io/repository/github/projectskyfire/skyfire_548)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/57a11392c3ed42dcae439669e893565f)](https://app.codacy.com/gh/ProjectSkyfire/SkyFire_548/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade)

## Project Information
SkyFire is a *MMORPG* Framework based mostly on C++.

SkyFireEMU is a MMORPG Framework based mostly on C++. It is completely 
open-source, and is community supported. It is derived
from CactusEmu, TrinityCore, and MaNGOS, the Massive Network Game Object Servers, 
and is based on the code of their projects with extensive changes over time to optimize, 
improve and cleanup the code base at the same time as improving the in game mechanics
and functionality.

It is completely open source; community involvement is highly encouraged.

If you wish to contribute ideas or code please visit our site linked below or
make pull requests to our 
[Github repository](https://github.com/ProjectSkyfire/SkyFire_548/pulls).

For further information on the SkyFire project, please visit our project website at 
[projectskyfire.org](http://www.projectskyfire.org).

## Supported Client Version
**MoP 5.4.8 Build 18414**

## Requirements
+ Platform/Architecture:
  + Ubuntu 24.04 LTS: x86_64
  + Windows 11: x86_64
  + Windows 10: x86_64
  + Windows Server 2025: x86_64
  + Windows Server 2022: x86_64
+ Processor with SSE2 support.
+ C++23-capable compiler:
  + GCC 14.0 or newer on Linux.
  + MSVC 19.44.35217.0 or newer with Visual Studio 2022 on Windows.
+ CMake:
  + 3.27.7 or newer on Linux.
  + 4.1.2 or newer on Windows.
+ Boost 1.91.0. Set `BOOST_ROOT` when Boost is installed outside the system prefix.
+ OpenSSL 4.0.1 with the legacy provider module available.
+ MySQL client development libraries.
  + MySQL 9.6 is the tested Windows install.
  + On Ubuntu, install the distro client development package such as `default-libmysqlclient-dev`.
+ Ninja is recommended for Linux builds.

## Playerbots

This repository includes SkyFire core changes for playerbots plus a modules system.
The playerbots module lives in a separate repository and is checked out as a submodule:

- Module: [DigiD702/mod-playerbots](https://github.com/DigiD702/mod-playerbots)

Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/DigiD702/skyfire_548_playerbots.git
```

If you already cloned without submodules:

```bash
git submodule update --init --recursive
```

Enable modules at configure time with `-DMODULES=1` (default).

## Install
Detailed installation guides are available in the wiki for

[Windows](https://wiki.projectskyfire.org/index.php?title=Installation_(Windows_5xx)),
[Fedora 39](https://wiki.projectskyfire.org/index.php?title=Installation_(Fedora_39)),
[Ubuntu 24.04](https://wiki.projectskyfire.org/index.php?title=Installation_(Ubuntu_24.04_LTS)) and
[Mac OSX](https://wiki.projectskyfire.org/index.php?title=Installation_Mac_OS_X).


## Reporting issues
Please use the search function before you report issues.
[SkyFire Issue Tracker](https://github.com/ProjectSkyfire/SkyFire_548/issues).

## Submitting fixes
Fixes are submitted as pull requests.

[SkyFire Pull Request](https://github.com/ProjectSkyfire/SkyFire_548/pulls)

## Copyright
License: GPL 3.0

Read file [LICENSE](LICENSE.md)

## Authors &amp; Contributors
Read file [THANKS](THANKS.md)

## Links
Forum: [https://www.projectskyfire.org/](https://www.projectskyfire.org/)

Database Releases: [https://github.com/ProjectSkyfire/SkyFire_548/releases](https://github.com/ProjectSkyfire/SkyFire_548/releases)

Wiki: [https://wiki.projectskyfire.org](https://wiki.projectskyfire.org)
