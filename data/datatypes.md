# Data Types

This directory is live content. `loadGameData()` reads JSON from here at runtime.

Current top-level folders:

- `blocks/`: block definitions
- `items/`: item definitions
- `states/blocks/`: per-block state definitions and state-based model variants
- `biomes/`: biome definitions used by terrain generation
- `tags/`: tag definitions of any id(s)
- `recipes/`: recipe definitions
- `loottables/`: loot table definitions
- `functions/`: functions definitions

General conventions:

- `id` is the stable lookup key.
- Paths like `render.texture`, `render.model`, variant `model`, and item `icon` are relative to `assets/`.
- Block `drops[].item` should reference an item id.
- Item `placeableBlock` should reference a block id.
- Unknown extra fields are generally ignored unless the loader reads them explicitly.

## Blocks

Files live in `data/blocks/<id>.json`.

Required fields:

- `id`: string
- `name`: string
- `voxel`: object
- `render`: object
- `drops`: array

`voxel` fields:

- `solid`: bool, required
- `material`: string, required
- `translucent`: bool, optional, defaults to `false`

`render` fields:

- `color`: `[r, g, b]`, required
- `opacity`: number, optional, defaults to `1.0`
- `texture`: string or object, optional
- `type`: string, optional, currently expected to be `"cube"` or `"model"`
- `model`: string, optional
- `tintKey`: bool, optional, defaults to `false`

`render.texture` supports:

- A single string: treated as the albedo texture
- An object with any of:
  - `albedo`
  - `normal`
  - `roughness`
  - `emissive`

`drops` is an array of objects:

- `item`: string, required
- `count`: number, required

Optional top-level `properties`:

- Object of arbitrary key/value pairs
- Supported value types are `bool`, `number`, and `string`
- Integer-like numbers are stored as integers; non-integers are stored as floats

Example:

```json
{
  "id": "wheat",
  "name": "Wheat",
  "voxel": {
    "solid": false,
    "translucent": false,
    "material": "plant"
  },
  "render": {
    "color": [0.55, 0.86, 0.36],
    "opacity": 1.0,
    "type": "model",
    "model": "models/blocks/wheat_stage_0.json",
    "texture": "textures/blocks/wheat_stage_0.ppm"
  },
  "drops": [
    { "item": "wheat_seeds", "count": 1 }
  ],
  "properties": {
    "crop": true,
    "tickInterval": 1,
    "maxAge": 3,
    "seedItem": "wheat_seeds"
  }
}
```

## Items

Files live in `data/items/<id>.json`.

Required fields:

- `id`: string
- `name`: string
- `stackSize`: number
- `icon`: string

Optional fields:

- `placeableBlock`: string

Example:

```json
{
  "id": "wheat_seeds",
  "name": "Wheat Seeds",
  "stackSize": 99,
  "placeableBlock": "wheat",
  "icon": "textures/blocks/wheat_stage_0.ppm"
}
```

## Block States

Files live in `data/states/blocks/<block-id>.json`.

These define a block's state space and optional per-state model overrides. The file `id` should match the block id.

Top-level fields:

- `id`: string, required
- `states`: object, optional
- `variants`: object, optional

`states` maps property names to one of these definitions:

- `bool`
  - `type`: `"bool"`
  - `default`: bool
- `enum`
  - `type`: `"enum"`
  - `default`: string
  - `values`: string array
- `int`
  - `type`: `"int"`
  - `default`: number
  - `min`: number
  - `max`: number

Notes:

- State properties are sorted by property name before runtime ids are assigned.
- The default value is always enumerated first.
- Variant keys use canonical `key=value` pairs joined by commas, for example `age=3` or `facing=north,open=true`.

Each `variants.<state-key>` value currently supports:

- `model`: string, optional

Example:

```json
{
  "id": "wheat",
  "states": {
    "age": {
      "type": "int",
      "default": 0,
      "min": 0,
      "max": 3
    }
  },
  "variants": {
    "age=0": { "model": "models/blocks/wheat_stage_0.json" },
    "age=1": { "model": "models/blocks/wheat_stage_1.json" },
    "age=2": { "model": "models/blocks/wheat_stage_2.json" },
    "age=3": { "model": "models/blocks/wheat_stage_3.json" }
  }
}
```

## Biomes

Files live in `data/biomes/<id>.json`.

Required fields:

- `id`: string
- `name`: string

Optional top-level fields:

- `priority`: number
- `rarity`: number
- `climate`: object
- `terrain`: object
- `surface`: object
- `atmosphere`: object
- `fertility`: object
- `colors`: object

`climate` supports these optional range objects:

- `temperature`
- `humidity`
- `rainfall`
- `elevation`
- `drainage`
- `waterTable`

Each range object may contain:

- `min`: number
- `max`: number

`terrain`:

- `baseHeight`: number
- `heightVariation`: number

`surface`:

- `top`: string
- `middle`: string
- `base`: string
- `middleDepth`: number

`atmosphere`:

- `skyColor`: `[r, g, b]`
- `fogColor`: `[r, g, b]`
- `waterColor`: `[r, g, b]`

`fertility`:

- `nitrogen`: number
- `phosphorus`: number
- `potassium`: number
- `magnesium`: number
- `calcium`: number
- `sulfur`: number

`colors`:

- Arbitrary string keys mapped to `[r, g, b]`
- Current content uses entries like `grass` and `foliage`

Example:

```json
{
  "id": "temperate_forest",
  "name": "Temperate Forest",
  "priority": 10,
  "rarity": 1.0,
  "climate": {
    "temperature": { "min": 0.30, "max": 0.70 },
    "humidity": { "min": 0.40, "max": 0.90 }
  },
  "terrain": {
    "baseHeight": 48,
    "heightVariation": 14
  },
  "surface": {
    "top": "grass",
    "middle": "dirt",
    "middleDepth": 3,
    "base": "stone"
  },
  "colors": {
    "grass": [0.3, 0.76, 0.22],
    "foliage": [0.36, 0.62, 0.18]
  }
}
```

## Runtime Notes

- Blocks receive runtime ids when data is loaded.
- Blocks with state definitions reserve a contiguous runtime-id range for every state combination.
- Preferred low runtime ids are currently assigned to `grass`, `dirt`, `water`, and `stone` before other blocks.
