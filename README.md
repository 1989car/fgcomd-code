[English](README.md) | [简体中文](README.zh-CN.md)

# fgcomd-code

> **This repository is a backup mirror of the [fgcomd project on SourceForge](https://sourceforge.net/projects/fgcomd/) — it is not the upstream development repository.**
>
> The upstream project has been unmaintained for a long time (last code commit 2011-06-14), and its original hosting on SourceForge's SVN carries the risk of becoming inaccessible or lost. A complete snapshot is therefore kept here as a backup. Treat **upstream** as the source of truth for commits and pull requests; this mirror exists only to preserve and consult the code.

## Mirror source

| Item | Address |
| --- | --- |
| Upstream project page | <https://sourceforge.net/projects/fgcomd/> |
| Upstream code browser | <https://sourceforge.net/p/fgcomd/code/> |
| Upstream SVN repository (HTTPS) | <https://svn.code.sf.net/p/fgcomd/code/> |
| Upstream SVN repository (svn://) | `svn://svn.code.sf.net/p/fgcomd/code/` |
| Upstream project website | <https://fgcomd.sourceforge.io/> |
| Upstream maintainer | Holger Wirtz (SourceForge user `dcoredump`) |

### Snapshot version

This mirror corresponds to upstream SVN **`trunk` at r55** (committed `2011-06-14 20:06:45 +0800`); the file list matches upstream `trunk` item by item:

```bash
# to pull the original upstream version for comparison
svn checkout https://svn.code.sf.net/p/fgcomd/code/trunk
```

The upstream repository is laid out as `trunk` / `tags` / `branches`. This mirror contains only `trunk` (the development line); `tags` and `branches` are not included.

## About the project

fgcomd is a radio (voice) conference server for the flight simulator **FlightGear**, providing multi-user radio communication inside the simulator:

- It runs as a standalone daemon and talks to clients over UDP.
- Clients report their position; the server computes the distance between users and applies **range-based attenuation and audio mixing** — beyond `MAX_DISTANCE` users are no longer audible to each other.
- Audio is carried over the network in fixed frames of 256 bytes at an 8 kHz sample rate; the server side applies AGC (automatic gain control) and squelch (background noise suppression).
- The wire protocol is defined in `packet.h` and covers position, user info, text chat, audio and ping commands.

The official positioning of the project is to provide voice capability as a standalone application (FlightGear's built-in voice support was not implemented at the time).

## Source layout

| Executable | Description |
| --- | --- |
| `fgcomd` | Main daemon: manages the user and conference hash tables, listens on a UDP port and dispatches incoming packets |
| `lighthouse` | Test tool: simulates a client continuously transmitting audio (a beacon) |
| `listener` | Test tool: receives and plays back the audio returned by the server (requires OpenAL) |

The `Makefile` also declares a `stress` target depending on `stress.c`, but that source file does not exist in upstream `trunk`, so the target cannot actually be built.

Main modules:

| File | Responsibility |
| --- | --- |
| `fgcomd.c` / `fgcomd.h` | Entry point, signal handling, global state, version and protocol constants |
| `config.c` / `config.h` | Command-line option parsing (`-v` / `-h` / `-p`) |
| `net.c` / `net.h` | UDP socket and network dispatcher thread |
| `user.c` / `user.h` | User management, position updates, timeout cleanup |
| `conference.c` / `conference.h` | Conference (channel) management and audio mixing |
| `packet.c` / `packet.h` | Packet structures and (de)serialization |
| `audio.c` / `audio.h` | AGC, squelch and mixing audio processing |
| `distance.c` / `distance.h` | Distance matrix computation between users |
| `data/audiodata.h` | Pre-generated audio sample data (about 6 MB) |

## Building

### Dependencies

- A toolchain (GCC / Make)
- **GLib 2.x** (`glib-2.0`)
- **GNet 2.x** (`gnet-2.0`)
- **OpenAL + libalut** (`listener` only)

Debian / Ubuntu example:

```bash
sudo apt install build-essential libglib2.0-dev libgnet2.0-dev libopenal-dev libalut-dev
```

### Compiling

```bash
make            # builds fgcomd, lighthouse, listener
make clean      # removes *.o and the executables
```

> The `CFLAGS` in the `Makefile` hard-code the paths `/usr/lib/glib-2.0/include` and `/usr/lib/gnet-2.0/include`; on the common `/usr/include/glib-2.0` layout of modern distributions these may need adjusting for your environment.
>
> The `Makefile` injects the version number via `svnversion`, so `SVN_REV` ends up empty outside an SVN working copy (the build still completes).

## Running

```bash
./fgcomd -h 0.0.0.0 -p 6463 -v
```

| Option | Meaning | Default |
| --- | --- | --- |
| `-v` | Verbose output while running | off |
| `-h` | Listen address | `localhost` |
| `-p` | Listen port | `6463` |

At startup `/etc/fgcomd.conf` is passed in as the configuration file path, but **the current implementation only actually parses command-line options — the configuration file is never read**. Use the command line to customise the settings.

## License and copyright

- License: **GNU General Public License v3.0 (GPLv3)**, matching the upstream project declaration.
- Copyright: `Copyright (C) 2006-2011 Holger Wirtz <dcoredump@gmail.com>`.
- All source code in this mirror comes from upstream and is functionally unmodified; only this README and the Git ignore rules were added.

## About this mirror

- This repository uses Git while upstream uses SVN, so the two histories are not interchangeable. This repository contains a single import commit and therefore does **not** carry upstream's full commit history (the complete upstream history remains visible in the upstream code browser).
- If upstream ever becomes active again, **upstream** takes precedence — apply the changes there and sync them back into this mirror.
- No feature development is accepted here. If you need to modify the code, do it upstream (or in your own fork) first, then sync the result here.
