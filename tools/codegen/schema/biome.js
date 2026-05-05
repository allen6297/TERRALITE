// Schema for BiomeDefinition / BiomeDef

module.exports = {
  cppStruct:    'BiomeDefinition',
  dtsInterface: 'BiomeDef',

  dtsGroups: {
    climate:    { name: 'BiomeClimate',    doc: 'Climate axis ranges this biome occupies (each normalised to [0,1]).' },
    terrain:    { name: 'BiomeTerrain',    doc: 'Terrain height parameters.' },
    surface:    { name: 'BiomeSurface',    doc: 'Surface block layers.' },
    atmosphere: { name: 'BiomeAtmosphere', doc: 'Sky, fog, and water colours.' },
    fertility:  { name: 'BiomeFertility',  doc: 'Soil nutrient values for agriculture.' },
  },

  fields: [
    // ── Top-level ────────────────────────────────────────────────────────────
    { cpp: 'id',       jsPath: 'id',       type: 'string', required: true, doc: 'Unique namespaced identifier.' },
    { cpp: 'name',     jsPath: 'name',     type: 'string', required: true, doc: 'Human-readable display name.' },
    { cpp: 'priority', jsPath: 'priority', type: 'float',  default: 0.0,   doc: 'Tie-breaker when two biomes score equally.' },
    { cpp: 'rarity',   jsPath: 'rarity',   type: 'float',  default: 1.0,   doc: 'Score multiplier — values below 1 make the biome rarer.' },

    // ── climate group — each axis is a custom ClimateRange sub-object ────────
    { cpp: 'climate.temperature', jsPath: 'climate.temperature', type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'temperature', dtsType: 'ClimateRange', doc: 'Temperature range.' },
    { cpp: 'climate.humidity',    jsPath: 'climate.humidity',    type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'humidity',    dtsType: 'ClimateRange', doc: 'Humidity range.' },
    { cpp: 'climate.rainfall',    jsPath: 'climate.rainfall',    type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'rainfall',    dtsType: 'ClimateRange', doc: 'Rainfall range.' },
    { cpp: 'climate.elevation',   jsPath: 'climate.elevation',   type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'elevation',   dtsType: 'ClimateRange', doc: 'Elevation range.' },
    { cpp: 'climate.drainage',    jsPath: 'climate.drainage',    type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'drainage',    dtsType: 'ClimateRange', doc: 'Drainage range.' },
    { cpp: 'climate.waterTable',  jsPath: 'climate.waterTable',  type: 'custom',
      cppType: 'BiomeClimateRange', cppDefault: '{}', parser: 'parseClimateRange', parserPassKey: true,
      dtsGroup: 'climate', dtsKey: 'waterTable',  dtsType: 'ClimateRange', doc: 'Water table range.' },

    // ── terrain group ────────────────────────────────────────────────────────
    { cpp: 'terrain.baseHeight',      jsPath: 'terrain.baseHeight',      type: 'float', default: 48.0,
      dtsGroup: 'terrain', dtsKey: 'baseHeight',      doc: 'Average surface height in blocks.' },
    { cpp: 'terrain.heightVariation', jsPath: 'terrain.heightVariation', type: 'float', default: 12.0,
      dtsGroup: 'terrain', dtsKey: 'heightVariation', doc: 'Amplitude of height noise.' },

    // ── surface group ────────────────────────────────────────────────────────
    { cpp: 'surface.top',         jsPath: 'surface.top',         type: 'string', default: 'base:grass',
      dtsGroup: 'surface', dtsKey: 'top',         dtsType: 'NamespacedId', doc: 'Block on the very top of the terrain column.' },
    { cpp: 'surface.middle',      jsPath: 'surface.middle',      type: 'string', default: 'base:dirt',
      dtsGroup: 'surface', dtsKey: 'middle',      dtsType: 'NamespacedId', doc: 'Block filling the middle layer.' },
    { cpp: 'surface.base',        jsPath: 'surface.base',        type: 'string', default: 'base:stone',
      dtsGroup: 'surface', dtsKey: 'base',        dtsType: 'NamespacedId', doc: 'Block below the middle layer.' },
    { cpp: 'surface.middleDepth', jsPath: 'surface.middleDepth', type: 'int',    default: 3,
      dtsGroup: 'surface', dtsKey: 'middleDepth',                           doc: 'Depth of the middle layer in blocks.' },

    // ── atmosphere group ─────────────────────────────────────────────────────
    { cpp: 'atmosphere.skyColor',   jsPath: 'atmosphere.skyColor',   type: 'rgb', default: [0.58, 0.78, 0.98],
      dtsGroup: 'atmosphere', dtsKey: 'skyColor',   doc: 'Sky gradient colour (linear RGB).' },
    { cpp: 'atmosphere.fogColor',   jsPath: 'atmosphere.fogColor',   type: 'rgb', default: [0.75, 0.85, 0.95],
      dtsGroup: 'atmosphere', dtsKey: 'fogColor',   doc: 'Distance fog colour (linear RGB).' },
    { cpp: 'atmosphere.waterColor', jsPath: 'atmosphere.waterColor', type: 'rgb', default: [0.20, 0.45, 0.80],
      dtsGroup: 'atmosphere', dtsKey: 'waterColor', doc: 'Water tint colour (linear RGB).' },

    // ── fertility group ───────────────────────────────────────────────────────
    { cpp: 'fertility.nitrogen',   jsPath: 'fertility.nitrogen',   type: 'float', default: 0.5, dtsGroup: 'fertility', dtsKey: 'nitrogen' },
    { cpp: 'fertility.phosphorus', jsPath: 'fertility.phosphorus', type: 'float', default: 0.5, dtsGroup: 'fertility', dtsKey: 'phosphorus' },
    { cpp: 'fertility.potassium',  jsPath: 'fertility.potassium',  type: 'float', default: 0.5, dtsGroup: 'fertility', dtsKey: 'potassium' },
    { cpp: 'fertility.magnesium',  jsPath: 'fertility.magnesium',  type: 'float', default: 0.5, dtsGroup: 'fertility', dtsKey: 'magnesium' },
    { cpp: 'fertility.calcium',    jsPath: 'fertility.calcium',    type: 'float', default: 0.5, dtsGroup: 'fertility', dtsKey: 'calcium' },
    { cpp: 'fertility.sulfur',     jsPath: 'fertility.sulfur',     type: 'float', default: 0.2, dtsGroup: 'fertility', dtsKey: 'sulfur' },

    // ── Top-level — custom ────────────────────────────────────────────────────
    {
      cpp: 'colors', jsPath: 'colors', type: 'custom',
      cppType: 'std::unordered_map<std::string, std::array<float, 3>>', cppDefault: '{}',
      parser: 'parseBiomeColors',
      dtsType: 'Record<string, RGB>',
      doc: 'Named tint colours applied to blocks that opt in via tintKey. @example { grass: [0.3, 0.76, 0.22] }',
    },
  ],
}
