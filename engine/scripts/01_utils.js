/**
 * engine/scripts/utils.js
 *
 * General-purpose utilities available to all pack scripts.
 * Loaded before any pack script runs.
 */
const Utils = (() => {

  // ── Math ───────────────────────────────────────────────────────────────────

  function clamp(value, min, max) {
    return Math.min(Math.max(value, min), max)
  }

  function lerp(a, b, t) {
    return a + (b - a) * clamp(t, 0, 1)
  }

  /** Remap a value from one range to another. */
  function remap(value, inMin, inMax, outMin, outMax) {
    const t = (value - inMin) / (inMax - inMin)
    return lerp(outMin, outMax, t)
  }

  // ── Colour ─────────────────────────────────────────────────────────────────

  /**
   * Parse a CSS hex colour string into a linear-space RGB triple.
   * Supports "#rgb", "#rrggbb", "#rrggbbaa" (alpha is ignored).
   * @param {string} hex
   * @returns {[number, number, number]}
   */
  function hexToRgb(hex) {
    const h = hex.replace('#', '')
    let r, g, b

    if (h.length === 3) {
      r = parseInt(h[0] + h[0], 16)
      g = parseInt(h[1] + h[1], 16)
      b = parseInt(h[2] + h[2], 16)
    } else {
      r = parseInt(h.slice(0, 2), 16)
      g = parseInt(h.slice(2, 4), 16)
      b = parseInt(h.slice(4, 6), 16)
    }

    return [r / 255, g / 255, b / 255]
  }

  /**
   * Convert HSL (each in [0, 1]) to a linear-space RGB triple.
   * @param {number} h  Hue
   * @param {number} s  Saturation
   * @param {number} l  Lightness
   * @returns {[number, number, number]}
   */
  function hslToRgb(h, s, l) {
    if (s === 0) return [l, l, l]

    const hue2rgb = (p, q, t) => {
      if (t < 0) t += 1
      if (t > 1) t -= 1
      if (t < 1/6) return p + (q - p) * 6 * t
      if (t < 1/2) return q
      if (t < 2/3) return p + (q - p) * (2/3 - t) * 6
      return p
    }

    const q = l < 0.5 ? l * (1 + s) : l + s - l * s
    const p = 2 * l - q
    return [
      hue2rgb(p, q, h + 1/3),
      hue2rgb(p, q, h),
      hue2rgb(p, q, h - 1/3),
    ]
  }

  /**
   * Linearly interpolate between two RGB colours.
   * @param {[number,number,number]} a
   * @param {[number,number,number]} b
   * @param {number} t  Mix factor in [0, 1]
   * @returns {[number, number, number]}
   */
  function lerpRgb(a, b, t) {
    return [lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)]
  }

  /**
   * Multiply an RGB colour by a scalar.
   * @param {[number,number,number]} rgb
   * @param {number} factor
   * @returns {[number, number, number]}
   */
  function scaleRgb(rgb, factor) {
    return [
      clamp(rgb[0] * factor, 0, 1),
      clamp(rgb[1] * factor, 0, 1),
      clamp(rgb[2] * factor, 0, 1),
    ]
  }

  // ── Namespaced IDs ─────────────────────────────────────────────────────────

  /**
   * Split a namespaced ID into its namespace and path parts.
   * @param {string} id  e.g. "base:stone"
   * @returns {{ namespace: string, path: string }}
   */
  function parseId(id) {
    const colon = id.indexOf(':')
    if (colon === -1) throw new Error(`Utils.parseId: "${id}" is not a namespaced ID`)
    return { namespace: id.slice(0, colon), path: id.slice(colon + 1) }
  }

  /**
   * Build a namespaced ID from separate parts.
   * @param {string} namespace
   * @param {string} path
   * @returns {string}
   */
  function makeId(namespace, path) {
    return `${namespace}:${path}`
  }

  /**
   * Return true if the string looks like a valid namespaced ID.
   * @param {string} id
   * @returns {boolean}
   */
  function isValidId(id) {
    return typeof id === 'string' && /^[a-z0-9_]+:[a-z0-9_/.-]+$/.test(id)
  }

  // ── Climate helpers ────────────────────────────────────────────────────────

  /**
   * Shorthand for a climate range object.
   * @param {number} min  Normalised [0, 1]
   * @param {number} max  Normalised [0, 1]
   * @returns {{ min: number, max: number }}
   */
  function climate(min, max) {
    return { min, max }
  }

  /**
   * Build a full climate block with sensible defaults for any axes not specified.
   * @param {Partial<BiomeClimate>} overrides
   * @returns {BiomeClimate}
   */
  function makeClimate(overrides = {}) {
    const any = climate(0, 1)
    return Object.assign({
      temperature: any,
      humidity:    any,
      rainfall:    any,
      elevation:   any,
      drainage:    any,
      waterTable:  any,
    }, overrides)
  }

  // ── Drop helpers ───────────────────────────────────────────────────────────

  /**
   * Create a single-item drop entry.
   * @param {string} itemId  Namespaced item ID
   * @param {number} [count=1]
   * @returns {{ item: string, count: number }}
   */
  function drop(itemId, count = 1) {
    return { item: itemId, count }
  }

  return {
    // Math
    clamp, lerp, remap,
    // Colour
    hexToRgb, hslToRgb, lerpRgb, scaleRgb,
    // IDs
    parseId, makeId, isValidId,
    // Climate
    climate, makeClimate,
    // Drops
    drop,
  }
})()
