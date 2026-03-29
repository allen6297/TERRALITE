# VoxelGame

Minimal macOS voxel-game starter using:

- C++
- CMake
- GLFW installed with Homebrew
- OpenGL compatibility profile for fast prototyping

## Concept
a survival game where you control a character to explore a world.
- seasons/daycycle/weather
- terrain generation/biomesystem
- player movement
- player interaction
- player inventory

chunks: 16*16*16

- assets:
  - textures
  - models
  - animations
- data:
  - items
  - blocks
data driven from json files

Current project directories:

- `assets/textures`
- `assets/models`
- `assets/animations`
- `data/blocks`
- `data/items`

## Notes
Feel free to divide and separate screens into separate files, as needed.
I'm not a fan of having everything in one file.
I'm also not a fan of having everything in one class.
feel free to add directories to organize code.
I'm not a fan of having everything in one header.


## Prerequisites

Install GLFW:

```bash
brew install glfw
```

## Build

```bash
cmake -S . -B build
cmake --build build
./build/VoxelGame
```

## Controls

- `W`, `A`, `S`, `D`: move
- `Q`, `E`: move down/up
- Arrow keys: look around
- `Esc`: quit

## Next steps

- Add a JSON loader for block and item definitions
- Replace hardcoded block colors/types with loaded block data
- Connect placed/broken blocks to item definitions and inventory
- Add texture/material support to the renderer
