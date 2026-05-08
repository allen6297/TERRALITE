// Schema for ItemDefinition / ItemDef
module.exports = {
  cppStruct:    'ItemDefinition',
  dtsInterface: 'ItemDef',
  dtsGroups:    {},

  fields: [
    {
      cpp: 'id',        jsPath: 'id',        type: 'string',  required: true,
      validator: 'item_id', dtsType: 'ItemId',
      doc: 'Unique namespaced identifier. @example "base:wheat_seeds"',
    },
    {
      cpp: 'name',      jsPath: 'name',      type: 'string',  required: true,
      doc: 'Human-readable display name.',
    },
    {
      cpp: 'stackSize', jsPath: 'stackSize', type: 'int',     default: 1,
      validator: 'positive_int',
      doc: 'Maximum number of this item per inventory slot.',
    },
    {
      cpp: 'icon',      jsPath: 'icon',      type: 'string',  default: '',
      validator: 'texture_path', dtsType: 'TexturePath',
      doc: 'Icon texture path relative to the pack assets folder.',
    },
    {
      cpp: 'placeableBlock', jsPath: 'placeableBlock', type: 'string?',
      validator: 'block_id', dtsType: 'BlockId',
      doc: 'If set, using this item places the named block in the world.',
    },
  ],
}
