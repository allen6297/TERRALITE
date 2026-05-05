/**
 * engine/scripts/02_registry.js
 *
 * Defines the Registry global that pack scripts use to query and modify
 * registered content.  Wraps the C++ __get* / __modifyBlock primitives.
 *
 * This file is executed before any pack script.
 */

const Registry = {
  // ── Read ───────────────────────────────────────────────────────────────────
  getBlock(id)  { return __getBlock(id)  },
  getItem(id)   { return __getItem(id)   },
  getBiome(id)  { return __getBiome(id)  },
  getTag(id)    { return __getTag(id)    },

  // ── Write ──────────────────────────────────────────────────────────────────

  /**
   * Modify a block that has already been registered (by JSON or an earlier
   * script).  The callback receives a mutable block object; any properties set
   * on it are sent back to C++ via __modifyBlock.
   *
   * Supported fields:
   *   block.displayName   — human-readable name (alias for block.name)
   *   block.name          — same as displayName
   *   block.hardness      — stored in block.properties.hardness
   *   block.solid         — bool
   *   block.translucent   — bool
   *   block.opacity       — float 0–1
   *   block.tintKey       — bool
   *   block.modelPath     — string
   *   block.renderType    — "cube" | "model"
   *   block.material      — string
   *   block.color         — [r, g, b]
   *
   * Example:
   *   Registry.modifyBlock('example_pack:copper_ore', block => {
   *     block.displayName = 'Dense Copper Ore'
   *     block.hardness    = 4.0
   *   })
   */
  modifyBlock(id, fn) {
    if (typeof id !== 'string')
      throw new Error('Registry.modifyBlock: first argument must be a block id string')
    if (typeof fn !== 'function')
      throw new Error('Registry.modifyBlock: second argument must be a function')

    const current = __getBlock(id)
    if (!current)
      throw new Error(`Registry.modifyBlock: block "${id}" has not been registered`)

    // Build a mutable patch object, pre-filled with current values.
    // "displayName" is a user-friendly alias that writes through to "name".
    const patch = Object.assign({}, current)
    Object.defineProperty(patch, 'displayName', {
      get()  { return this.name },
      set(v) { this.name = v },
      enumerable:   true,
      configurable: true,
    })

    fn(patch)

    // Remove the alias descriptor before sending to C++ (it only reads .name).
    delete patch.displayName

    __modifyBlock(id, patch)
  },
}
