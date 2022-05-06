
# SuperTux v0.1.3 Level Player

<img src="screencaps/stl_player_build_6989aa1.png" height="320">

https://user-images.githubusercontent.com/57591392/137566427-165d9aca-c52b-4f39-8d5c-ec266b4f1c79.mov

## Introduction

The goal of the SuperTux v0.1.3 Level Player project is to produce an unofficial standalone Linux program that can read and play preexisting SuperTux v0.1.3 level files.

## Quick Start

Download the binary asset appropriate for your platform from the [Releases page](https://github.com/s53g4z/stl_player/releases).

On macOS, the quarantine bit may have to be stripped from the app bundle before it will run. This can be accomplished by running `xattr -r -d com.apple.quarantine path/to/the.app`.

## Build Notes

### macOS / Mac OS X

Support for the platform is provided by the contents of the `mac/` directory at the root of the software repository. The Xcode project in `mac/` is known to build on v10.5, and the Xcode project in `mac/m1/STLPlayerM1/` is known to build on v12.x. The resulting binaries run on PPC/x86-32 and x86-64/arm64, respectively.

### Linux

Run `./build.sh` to build. The single executable program emitted is the SuperTux v0.1.3 Level player.

## How to Play

Use WASD or arrow keys to move. Holding CTRL allows stunned ice cubes to be picked up. Press Esc to quit the game.

The objective of the game is to make it to the goal at the end of each (side-scrolling) level.

Some enemies will die when jumped upon.

## Development

See [README-dev.md](/README-dev.md) for details.

## Licensing

All code in this repository, with the exception of the contents of the `gpl/` directory, belongs to the public domain under The Unlicense software license. The `gpl/` contents are licensed under the GNU GPL v2 and come from the original GPL-licensed SuperTux v0.1.3 game project.
