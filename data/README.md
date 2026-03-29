# Data

This project is intended to become data-driven from JSON files.

Current layout:

- `blocks/`: block definitions used by world generation, rendering, drops, and placement
- `items/`: item definitions used by inventory, drops, and icons

Conventions:

- `id` is the stable lookup key
- `render.texture` and `icon` are relative to `assets/`
- block `drops[].item` should reference an item id
- item `placeableBlock` should reference a block id

The runtime loader is not implemented yet. These files define the target content format.
