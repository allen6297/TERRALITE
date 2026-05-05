/**
 * Voxel Game — Pack Scripting API
 *
 * GENERATED FILE — do not edit by hand.
 * Source:      tools/codegen/schema/{block,item,biome}.js
 * Regenerate:  node tools/codegen/generate.js   (or: cmake --build . --target generate_bindings)
 */


/** RGB colour in linear space, each channel in [0, 1]. */
type RGB = [number, number, number]

/** Namespaced identifier: "pack:name". @example "base:grass" */
type NamespacedId = string

/** A min/max range for a climate axis, normalised to [0, 1]. */
interface ClimateRange { min: number; max: number }

interface BlockTextures {
  albedo?:    string
  normal?:    string
  roughness?: string
  emissive?:  string
}

interface BlockDrop {
  /** Namespaced item ID. */
  item: NamespacedId
  count: number
}

interface BlockStatePropInt    { type: 'int';    min: number; max: number; default?: number }
interface BlockStatePropBool   { type: 'bool';   default?: boolean }
interface BlockStatePropString { type: 'string'; values: string[];  default?: string }
type BlockStateProp = BlockStatePropInt | BlockStatePropBool | BlockStatePropString

interface BlockStateVariant { model?: string }

/** Physics and voxel-space properties. */

interface BlockVoxel {
  /** Whether this block has a solid collision box. */
  solid?: boolean
  /** Whether light passes through this block. */
  translucent?: boolean
  /** Physics/sound material group. @example "terrain" "rock" "liquid" "plant" */
  material?: string
}

/** Visual and rendering configuration. */

interface BlockRender {
  /** Base tint colour (linear RGB). Applied when tintKey is false. */
  color?: RGB
  /** Alpha opacity. 1 = fully opaque, 0 = invisible. */
  opacity?: number
  /** When true the engine uses the biome tint colour instead of color. */
  tintKey?: boolean
  /** Render geometry type. "cube" = full block, "model" = custom model. */
  type?: "cube" | "model"
  /** Path to the block model JSON, relative to the pack assets folder. */
  model?: string
  /** Texture paths. Pass a string to set the albedo only, or an object for named maps. */
  texture?: string | BlockTextures
}

/** Climate axis ranges this biome occupies (each normalised to [0,1]). */

interface BiomeClimate {
  /** Temperature range. */
  temperature?: ClimateRange
  /** Humidity range. */
  humidity?: ClimateRange
  /** Rainfall range. */
  rainfall?: ClimateRange
  /** Elevation range. */
  elevation?: ClimateRange
  /** Drainage range. */
  drainage?: ClimateRange
  /** Water table range. */
  waterTable?: ClimateRange
}

/** Terrain height parameters. */

interface BiomeTerrain {
  /** Average surface height in blocks. */
  baseHeight?: number
  /** Amplitude of height noise. */
  heightVariation?: number
}

/** Surface block layers. */

interface BiomeSurface {
  /** Block on the very top of the terrain column. */
  top?: NamespacedId
  /** Block filling the middle layer. */
  middle?: NamespacedId
  /** Block below the middle layer. */
  base?: NamespacedId
  /** Depth of the middle layer in blocks. */
  middleDepth?: number
}

/** Sky, fog, and water colours. */

interface BiomeAtmosphere {
  /** Sky gradient colour (linear RGB). */
  skyColor?: RGB
  /** Distance fog colour (linear RGB). */
  fogColor?: RGB
  /** Water tint colour (linear RGB). */
  waterColor?: RGB
}

/** Soil nutrient values for agriculture. */

interface BiomeFertility {
  nitrogen?: number
  phosphorus?: number
  potassium?: number
  magnesium?: number
  calcium?: number
  sulfur?: number
}

interface BlockDef {
  states?:   Record<string, BlockStateProp>
  variants?: Record<string, BlockStateVariant>
  /** Unique namespaced identifier. @example "base:grass" */
  id: string
  /** Human-readable display name. */
  name: string
  /** Items dropped when this block is broken. */
  drops?: BlockDrop[]
  /** Arbitrary key–value properties accessible from gameplay code. */
  properties?: Record<string, boolean | number | string>
  voxel?: BlockVoxel
  render?: BlockRender
}

interface ItemDef {
  /** Unique namespaced identifier. @example "base:wheat_seeds" */
  id: string
  /** Human-readable display name. */
  name: string
  /** Maximum number of this item per inventory slot. */
  stackSize?: number
  /** Icon texture path relative to the pack assets folder. */
  icon?: string
  /** If set, using this item places the named block in the world. */
  placeableBlock?: NamespacedId
}

interface BiomeDef {
  /** Unique namespaced identifier. */
  id: string
  /** Human-readable display name. */
  name: string
  /** Tie-breaker when two biomes score equally. */
  priority?: number
  /** Score multiplier — values below 1 make the biome rarer. */
  rarity?: number
  /** Named tint colours applied to blocks that opt in via tintKey. @example { grass: [0.3, 0.76, 0.22] } */
  colors?: Record<string, RGB>
  climate?: BiomeClimate
  terrain?: BiomeTerrain
  surface?: BiomeSurface
  atmosphere?: BiomeAtmosphere
  fertility?: BiomeFertility
}

interface RecipeDef {
  /** Namespaced recipe id */
  id: string
  /** Recipe type (crafting, smelting, etc.) */
  type: string
  /** Output item id */
  output: string
  /** Output count */
  count?: number
  /** List of ingredient item ids */
  ingredients?: unknown
}


// ── Utils ─────────────────────────────────────────────────────────────────────

declare const Utils: {
  clamp(value: number, min: number, max: number): number
  lerp(a: number, b: number, t: number): number
  remap(value: number, inMin: number, inMax: number, outMin: number, outMax: number): number
  hexToRgb(hex: string): RGB
  hslToRgb(h: number, s: number, l: number): RGB
  lerpRgb(a: RGB, b: RGB, t: number): RGB
  scaleRgb(rgb: RGB, factor: number): RGB
  parseId(id: NamespacedId): { namespace: string; path: string }
  makeId(namespace: string, path: string): NamespacedId
  isValidId(id: string): boolean
  climate(min: number, max: number): ClimateRange
  makeClimate(overrides?: Partial<BiomeClimate>): BiomeClimate
  drop(itemId: NamespacedId, count?: number): BlockDrop
}

// ── Globals ───────────────────────────────────────────────────────────────────

interface TagDef {
  /** Namespaced identifier, e.g. "base:flammable". */
  id: NamespacedId
  /** Optional human-readable description. */
  description?: string
}

declare const Startup: {
  registerBlock(def: BlockDef): void
  registerItem(def: ItemDef): void
  registerBiome(def: BiomeDef): void
  /** Register a tag. Pass a namespaced id string or a TagDef object. */
  registerTag(idOrDef: NamespacedId | TagDef): void
}

declare const Registry: {
  getBlock(id: NamespacedId): BlockDef | null
  getItem(id: NamespacedId):  ItemDef  | null
  getBiome(id: NamespacedId): BiomeDef | null
  getTag(id: NamespacedId):   TagDef   | null
}
