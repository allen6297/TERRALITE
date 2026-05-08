#!/usr/bin/env node
/**
 * tools/codegen/generate.js
 *
 * Reads schema files and emits:
 *   include/common/pack/generated/ParseBindings.hpp  — parse function declarations
 *   src/common/pack/generated/ParseBindings.cpp      — parse function implementations
 *   packs/types/voxel.d.ts                    — TypeScript type declarations
 *
 * Usage:
 *   node tools/codegen/generate.js
 */

'use strict'

const fs = require('fs')
const path = require('path')

const ROOT = path.join(__dirname, '../..')
const SCHEMAS = ['block', 'item', 'biome', 'recipe'].map(n => require(`./schema/${n}`))

// ── Type mappings ─────────────────────────────────────────────────────────────

const CPP_TYPES = {
    string: 'std::string',
    enum: 'std::string',
    bool: 'bool',
    int: 'int',
    float: 'float',
    rgb: 'std::array<float, 3>',
    'string?': 'std::optional<std::string>',
}

const DTS_TYPES = {
    string: 'string',
    bool: 'boolean',
    int: 'number',
    float: 'number',
    rgb: 'RGB',
    'string?': 'string | undefined',
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function fmtFloat(n) {
    return Number.isInteger(n) ? `${n}.0f` : `${n}f`
}

function fmtDefault(type, def) {
    if (def === undefined) return null
    switch (type) {
        case 'string':
        case 'enum':
            return `"${def}"`
        case 'bool':
            return def ? 'true' : 'false'
        case 'int':
            return String(def)
        case 'float':
            return fmtFloat(def)
        case 'rgb':
            return `{${def.map(fmtFloat).join(', ')}}`
        case 'string?':
            return 'std::nullopt'
        default:
            return null
    }
}

// Build the C++ read expression for a simple (non-custom) field.
function dtsEnumType(values) {
    return values.map(v => `'${v}'`).join(' | ')
}

function cppEnumValues(values) {
    return `{${values.map(v => `"${v}"`).join(', ')}}`
}

function cppRequiredCheck(obj, jsKey) {
    return `voxel::js::jsRequire(ctx, ${obj}, "${jsKey}")`
}

function cppReadExpr(field, obj, jsKey) {
    const type = field.type
    const def = field.default
    const d = fmtDefault(type, def)
    const dflt = d !== null ? `, ${d}` : ''
    switch (type) {
        case 'string':
            return `voxel::js::jsStr  (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'enum':
            return `voxel::js::jsEnum (ctx, ${obj}, "${jsKey}", ${cppEnumValues(field.values)}, ${d ?? '""'})`
        case 'bool':
            return `voxel::js::jsBool (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'int':
            return `voxel::js::jsInt  (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'float':
            return `voxel::js::jsFloat(ctx, ${obj}, "${jsKey}"${dflt})`
        case 'rgb':
            return `voxel::js::jsColor(ctx, ${obj}, "${jsKey}"${dflt})`
        case 'string?':
            return `voxel::js::jsOptStr(ctx, ${obj}, "${jsKey}")`
    }
}

// Group fields by their top-level JS sub-object (first segment of jsPath).
// Returns { topLevel: [...], groups: { prefix: [...fields with jsKey set] } }
function groupFields(fields) {
    const topLevel = []
    const groups = {}

    for (const f of fields) {
        const parts = f.jsPath.split('.')
        if (parts.length === 1) {
            topLevel.push(f)
        } else {
            const prefix = parts[0]
            const jsKey = parts.slice(1).join('.')
            if (!groups[prefix]) groups[prefix] = []
            groups[prefix].push({...f, jsKey})
        }
    }

    return {topLevel, groups}
}

// ── C++ generation ────────────────────────────────────────────────────────────

function genParseBody(schema) {
    const {topLevel, groups} = groupFields(schema.fields)
    const lines = []
    const indent = '    '

    const writeLine = (l = '') => lines.push(l ? indent + l : '')

    writeLine(`${schema.cppStruct} out{};`)
    writeLine()

    // Top-level fields
    for (const f of topLevel) {
        if (f.required) {
            writeLine(cppRequiredCheck('obj', f.jsPath) + ';')
        }
        if (f.type === 'custom') {
            writeLine(`out.${f.cpp} = ${f.parser}(ctx, obj);`)
        } else {
            writeLine(`out.${f.cpp} = ${cppReadExpr(f, 'obj', f.jsPath)};`)
        }
    }

    // Sub-object groups
    for (const [prefix, fields] of Object.entries(groups)) {
        writeLine()
        writeLine(`// ${prefix}`)
        writeLine('{')
        writeLine(`    JSValue sub = JS_GetPropertyStr(ctx, obj, "${prefix}");`)
        for (const f of fields) {
            if (f.required) {
                writeLine('    ' + cppRequiredCheck('sub', f.jsKey) + ';')
            }
            if (f.type === 'custom') {
                const keyArg = f.parserPassKey ? `, "${f.jsKey}"` : ''
                writeLine(`    out.${f.cpp} = ${f.parser}(ctx, sub${keyArg});`)
            } else {
                writeLine(`    out.${f.cpp} = ${cppReadExpr(f, 'sub', f.jsKey)};`)
            }
        }
        writeLine('    JS_FreeValue(ctx, sub);')
        writeLine('}')
    }

    writeLine()
    writeLine('return out;')

    return lines.join('\n')
}

function genCppSource(schemas) {
    const banner = [
        '// GENERATED FILE — do not edit by hand.',
        '// Source:      tools/codegen/schema/{block,item,biome,recipe}.js',
        '// Regenerate:  node tools/codegen/generate.js',
        '',
    ].join('\n')

    const includes = [
        '#include "pack/generated/ParseBindings.hpp"',
        '#include "pack/JsHelpers.hpp"',
        '#include "data/GameData.hpp"',
        '#include "data/BiomeDefinition.hpp"',
        '#include <quickjs.h>',
        '',
    ].join('\n')

    // Forward-declare every custom parser used across all schemas.
    const customParsers = new Set()
    for (const s of schemas)
        for (const f of s.fields)
            if (f.type === 'custom') customParsers.add(f.parser)

    // Build extern declarations for each unique custom parser signature.
    // parserPassKey fields get an extra `const char*` argument.
    const parserSigs = new Map()
    for (const s of schemas) {
        for (const f of s.fields) {
            if (f.type !== 'custom') continue
            if (parserSigs.has(f.parser)) continue
            const extraArg = f.parserPassKey ? ', const char*' : ''
            parserSigs.set(f.parser, `extern ${f.cppType} ${f.parser}(JSContext*, JSValueConst${extraArg});`)
        }
    }

    const fwdDecls = [
        'namespace voxel {',
        '// Custom parsers — defined in ScriptBindingsCustom.cpp',
        ...[...parserSigs.values()],
        '',
        'namespace generated {',
        '',
    ].join('\n')

    const bodies = schemas.map(s => [
        `${s.cppStruct} parse${s.cppStruct}(JSContext* ctx, JSValueConst obj) {`,
        genParseBody(s),
        '}',
        '',
    ].join('\n')).join('\n')

    const footer = '} // namespace generated\n} // namespace voxel\n'

    return [banner, includes, fwdDecls, bodies, footer].join('\n')
}

function genCppHeader(schemas) {
    const banner = [
        '// GENERATED FILE — do not edit by hand.',
        '// Regenerate:  node tools/codegen/generate.js',
        '',
    ].join('\n')

    const guard = [
        '#pragma once',
        '#include "data/GameData.hpp"',
        '#include "data/BiomeDefinition.hpp"',
        '#include <quickjs.h>',
        '',
        'namespace voxel::generated {',
        '',
    ].join('\n')

    const decls = schemas.map(s =>
        `${s.cppStruct} parse${s.cppStruct}(JSContext* ctx, JSValueConst obj);`
    ).join('\n')

    const footer = '\n} // namespace voxel::generated\n'

    return [banner, guard, decls, footer].join('\n')
}

// ── TypeScript generation ─────────────────────────────────────────────────────

function dtsDoc(doc, indent = '') {
    if (!doc) return ''
    return `${indent}/** ${doc} */\n`
}

function dtsOptional(f) {
    return f.required ? '' : '?'
}

function dtsFieldType(f) {
    if (f.dtsType) return f.dtsType
    if (f.type === 'enum') return dtsEnumType(f.values)
    return DTS_TYPES[f.type] ?? 'unknown'
}

function genDtsGroupInterface(groupKey, groupDef, fields) {
    const members = fields.map(f => {
        const key = f.dtsKey ?? f.jsPath.split('.').pop()
        const opt = dtsOptional(f)
        const type = dtsFieldType(f)
        const docLine = f.doc ? `  /** ${f.doc} */\n` : ''
        return `${docLine}  ${key}${opt}: ${type}`
    }).join('\n')

    return [
        dtsDoc(groupDef.doc),
        `interface ${groupDef.name} {`,
        members,
        '}',
        '',
    ].join('\n')
}

function genDtsMainInterface(schema) {
    const groupFields = {}
    const rootFields = []

    for (const f of schema.fields) {
        if (f.dtsGroup) {
            if (!groupFields[f.dtsGroup]) groupFields[f.dtsGroup] = []
            groupFields[f.dtsGroup].push(f)
        } else {
            rootFields.push(f)
        }
    }

    const members = []

    // Root-level fields
    for (const f of rootFields) {
        if (f.doc) members.push(`  /** ${f.doc} */`)
        const opt = dtsOptional(f)
        const type = dtsFieldType(f)
        members.push(`  ${f.cpp}${opt}: ${type}`)
    }

    // Sub-interface fields
    for (const [groupKey, groupDef] of Object.entries(schema.dtsGroups ?? {})) {
        const groupName = groupDef.name
        members.push(`  ${groupKey}?: ${groupName}`)
    }

    return [
        `interface ${schema.dtsInterface} {`,
        members.join('\n'),
        '}',
        '',
    ].join('\n')
}

function genDts(schemas) {
    const banner = [
        '/**',
        ' * Voxel Game — Pack Scripting API',
        ' *',
        ' * GENERATED FILE — do not edit by hand.',
        ' * Source:      tools/codegen/schema/{block,item,biome,recipe}.js',
        ' * Regenerate:  node tools/codegen/generate.js   (or: cmake --build . --target generate_bindings)',
        ' */',
        '',
    ].join('\n')

    const primitives = `
/** RGB colour in linear space, each channel in [0, 1]. */
type RGB = [number, number, number]

/** Creates a nominal string type for editor-only safety. */
type Brand<T, Name extends string> = T & { readonly __brand: Name }

/** Namespaced identifier: "pack:name". @example "base:grass" */
type NamespacedId = Brand<string, 'NamespacedId'>
type BlockId = Brand<NamespacedId, 'BlockId'>
type ItemId = Brand<NamespacedId, 'ItemId'>
type BiomeId = Brand<NamespacedId, 'BiomeId'>
type TagId = Brand<NamespacedId, 'TagId'>
type RecipeId = Brand<NamespacedId, 'RecipeId'>
type TexturePath = Brand<string, 'TexturePath'>
type ModelPath = Brand<string, 'ModelPath'>

/** A min/max range for a climate axis, normalised to [0, 1]. */
interface ClimateRange { min: number; max: number }

interface BlockTextures {
  albedo?:    TexturePath
  normal?:    TexturePath
  roughness?: TexturePath
  emissive?:  TexturePath
}

interface BlockDrop {
  /** Namespaced item ID. */
  item: ItemId
  count: number
}

interface BlockStatePropInt    { type: 'int';    min: number; max: number; default?: number }
interface BlockStatePropBool   { type: 'bool';   default?: boolean }
interface BlockStatePropString { type: 'string'; values: string[];  default?: string }
type BlockStateProp = BlockStatePropInt | BlockStatePropBool | BlockStatePropString

interface BlockStateVariant { model?: ModelPath }
`

    // Sub-interfaces
    const subInterfaces = []
    for (const schema of schemas) {
        for (const [groupKey, groupDef] of Object.entries(schema.dtsGroups ?? {})) {
            const fields = schema.fields.filter(f => f.dtsGroup === groupKey)
            subInterfaces.push(genDtsGroupInterface(groupKey, groupDef, fields))
        }
    }

    // Main definition interfaces
    const mainInterfaces = schemas.map(s => {
        // Append states/variants to BlockDef manually (too complex to schema-ise)
        let iface = genDtsMainInterface(s)
        if (s.cppStruct === 'BlockDefinition') {
            iface = iface.replace(
                /^(interface BlockDef \{)/,
                '$1\n  states?:   Record<string, BlockStateProp>\n  variants?: Record<string, BlockStateVariant>'
            )
        }
        return iface
    })

    const utils = `
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
  drop(itemId: ItemId, count?: number): BlockDrop
}

// ── Globals ───────────────────────────────────────────────────────────────────

interface TagDef {
  /** Namespaced identifier, e.g. "base:flammable". */
  id: TagId
  /** Optional human-readable description. */
  description?: string
}

interface BlockBuilder {
  displayName(name: string): BlockBuilder
  hardness(value: number): BlockBuilder
  opacity(value: number): BlockBuilder
  color(r: number, g: number, b: number): BlockBuilder
  texture(pathOrObj: TexturePath | BlockTextures): BlockBuilder
  model(path: ModelPath): BlockBuilder
  renderType(type: "cube" | "model"): BlockBuilder
  solid(value: boolean): BlockBuilder
  translucent(value: boolean): BlockBuilder
  tintKey(value: boolean): BlockBuilder
  material(value: string): BlockBuilder
  drops(entries: BlockDrop | BlockDrop[]): BlockBuilder
  states(states: Record<string, BlockStateProp>): BlockBuilder
  variants(variants: Record<string, BlockStateVariant>): BlockBuilder
  property(key: string, value: boolean | number | string): BlockBuilder
}

interface ItemBuilder {
  displayName(name: string): ItemBuilder
  stackSize(value: number): ItemBuilder
  icon(path: TexturePath): ItemBuilder
  placesBlock(blockId: BlockId): ItemBuilder
}

declare const StartupEvents: {
  registry(type: 'block', fn: (event: { create(id: BlockId): BlockBuilder }) => void): void
  registry(type: 'item', fn: (event: { create(id: ItemId): ItemBuilder }) => void): void
  registry(type: 'biome', fn: (event: { register(def: BiomeDef): void }) => void): void
  registry(type: 'tag', fn: (event: { register(idOrDef: TagId | TagDef): void }) => void): void
}

declare const Registry: {
  getBlock(id: BlockId): BlockDef | null
  getItem(id: ItemId):   ItemDef  | null
  getBiome(id: BiomeId): BiomeDef | null
  getTag(id: TagId):     TagDef   | null
}
`

    return [banner, primitives, ...subInterfaces, ...mainInterfaces, utils].join('\n')
}

// ── Write output files ────────────────────────────────────────────────────────

function write(filePath, content) {
    fs.mkdirSync(path.dirname(filePath), {recursive: true})
    fs.writeFileSync(filePath, content, 'utf8')
    console.log(`  wrote ${path.relative(ROOT, filePath)}`)
}

console.log('codegen: generating bindings...')
write(path.join(ROOT, 'include/common/pack/generated/ParseBindings.hpp'), genCppHeader(SCHEMAS))
write(path.join(ROOT, 'src/common/pack/generated/ParseBindings.cpp'), genCppSource(SCHEMAS))
write(path.join(ROOT, 'packs/types/voxel.d.ts'), genDts(SCHEMAS))
console.log('codegen: done.')
