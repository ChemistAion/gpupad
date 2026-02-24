"use strict"

const manifest = {
  name: "🔬 &Inspector..."
}

// GLSL keywords that are not valid standalone expressions
const GLSL_KEYWORDS = new Set([
  'void','return','main','if','else','for','while','do','switch','case',
  'break','continue','discard','const','in','out','inout','uniform',
  'varying','attribute','flat','smooth','noperspective','struct','layout',
  'precision','highp','mediump','lowp',
  'float','int','uint','bool','double',
  'vec2','vec3','vec4','ivec2','ivec3','ivec4','uvec2','uvec3','uvec4',
  'dvec2','dvec3','dvec4','bvec2','bvec3','bvec4',
  'mat2','mat3','mat4','mat2x2','mat2x3','mat2x4',
  'mat3x2','mat3x3','mat3x4','mat4x2','mat4x3','mat4x4',
  'sampler2D','sampler3D','samplerCube','sampler2DArray',
  'isampler2D','isampler3D','isamplerCube',
  'usampler2D','usampler3D','usamplerCube',
  'sampler2DShadow','samplerCubeShadow','sampler2DArrayShadow'
])

function escapeRegex(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
}

class Inspector {
  constructor() {
    this._group   = null
    this._texture = null
    this._target  = null
    this._program = null
    this._call    = null
    this.enabled    = false
    this.expression = ''
    this.lastType   = ''
  }

  // ── Validation ─────────────────────────────────────────────────────────────

  validate(expr) {
    const e = expr.trim()
    if (!e)
      return { ok: false, error: 'Empty expression' }
    if (GLSL_KEYWORDS.has(e))
      return { ok: false, error: `'${e}' is a reserved keyword` }
    if (/^void\s+main/.test(e))
      return { ok: false, error: 'Cannot inspect main()' }

    let p = 0, b = 0
    for (const c of e) {
      if      (c === '(') p++
      else if (c === ')') { if (--p < 0) return { ok: false, error: 'Unbalanced ()' } }
      else if (c === '[') b++
      else if (c === ']') { if (--b < 0) return { ok: false, error: 'Unbalanced []' } }
    }
    if (p !== 0) return { ok: false, error: 'Unbalanced ()' }
    if (b !== 0) return { ok: false, error: 'Unbalanced []' }

    if (/^[*/%=<>^,&|]/.test(e))
      return { ok: false, error: 'Expression starts with operator' }
    if (/[+\-*/%=<>!&|^,([;]$/.test(e))
      return { ok: false, error: 'Expression ends with operator' }

    return { ok: true }
  }

  // ── Type inference ──────────────────────────────────────────────────────────

  inferType(expr, sources) {
    const e = expr.trim()
    const combined = sources.join('\n')

    // Constructor calls: vec3(...), mat2(...), etc.
    const ctor = e.match(/^(vec[234]|ivec[234]|uvec[234]|bvec[234]|mat[234](?:x[234])?)\s*\(/)
    if (ctor) return ctor[1]

    // GLSL built-in variables
    const builtins = {
      'gl_FragCoord': 'vec4', 'gl_FragDepth': 'float',
      'gl_FrontFacing': 'bool', 'gl_PointCoord': 'vec2',
    }
    if (builtins[e]) return builtins[e]

    // Swizzle suffix determines vector dimension
    const sw = e.match(/\.([xyzwrgba]+)$/)
    if (sw) {
      const n = sw[1].length
      if (n === 1) return 'float'
      if (n === 2) return 'vec2'
      if (n === 3) return 'vec3'
      if (n === 4) return 'vec4'
    }

    const esc = escapeRegex(e)

    // uniform TYPE name
    const um = combined.match(new RegExp(`\\buniform\\s+(\\w+)\\s+${esc}\\b`))
    if (um) return um[1]

    // TYPE name [=;,)]
    const vm = combined.match(
      new RegExp(`\\b(float|int|uint|bool|vec[234]|ivec[234]|uvec[234]|mat[234])\\s+${esc}\\s*[=;,)]`)
    )
    if (vm) return vm[1]

    return 'float'
  }

  // ── vec4 coercion ───────────────────────────────────────────────────────────

  coerce(expr, type) {
    switch (type) {
      case 'float': case 'int': case 'uint':
        return `vec4(${expr}, ${expr}, ${expr}, 1.0)`
      case 'bool':
        return `vec4(vec3(float(${expr})), 1.0)`
      case 'vec2': case 'ivec2': case 'uvec2':
        return `vec4(vec2(${expr}), 0.0, 1.0)`
      case 'vec3': case 'ivec3': case 'uvec3':
        return `vec4(vec3(${expr}), 1.0)`
      case 'vec4': case 'ivec4': case 'uvec4':
        return `vec4(${expr})`
      case 'mat2':
        return `vec4(${expr}[0], ${expr}[1])`
      case 'mat3':
        return `vec4(${expr}[0], 1.0)`
      case 'mat4':
        return `vec4(${expr}[0])`
      default:
        return `vec4(vec3(float(${expr})), 1.0)`
    }
  }

  // ── Shader rewrite ──────────────────────────────────────────────────────────

  detectOutVar(src) {
    const m = src.match(/\bout\s+vec4\s+(\w+)\s*;/)
    return m ? m[1] : 'fragColor'
  }

  // Replace void main() and everything after it with the inspector body.
  // Preserves the preamble (version, uniforms, helpers, forward decls).
  rewriteMain(src, outVar, vec4Expr) {
    const idx = src.search(/\bvoid\s+main\s*\(\s*\)/)
    if (idx === -1) return null
    return (
      src.slice(0, idx) +
      `void main() {\n  ${outVar} = ${vec4Expr};\n}\n`
    )
  }

  // ── Session helpers ─────────────────────────────────────────────────────────

  // Find the first active Draw call that is NOT inside our Inspector group.
  findTargetProgram() {
    const inspId = this._group ? this._group.id : null
    const calls  = app.session.findItems(
      item => item.type === 'Call' && item.callType === 'Draw'
    )
    for (const call of calls) {
      const parent = app.session.getParentItem(call)
      if (inspId && parent && parent.id === inspId) continue
      if (call.checked === false) continue
      if (call.programId) {
        const prog = app.session.findItem(call.programId)
        if (prog) return prog
      }
    }
    return null
  }

  // Create (or re-adopt) the __Inspector__ group at the end of the session.
  ensureGroup() {
    if (this._group && app.session.findItem(this._group.id))
      return this._group

    this._group   = app.session.insertItem(app.session, {
      type: 'Group', name: '__Inspector__', inlineScope: false
    })
    // Reset children so we recreate them inside the new group
    this._texture = null
    this._target  = null
    this._program = null
    this._call    = null
    return this._group
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  apply(expression) {
    const v = this.validate(expression)
    if (!v.ok) return v

    const program = this.findTargetProgram()
    if (!program)
      return { ok: false, error: 'No active Draw call found in session' }

    // Collect all shaders; read fragment sources; find the one with void main()
    const shaders = program.items.filter(s => s.type === 'Shader')
    const sources = shaders.map(s =>
      (s.shaderType !== 'Vertex' && s.fileName)
        ? (app.readTextFile(s.fileName) || '') : ''
    )

    let mainIdx = shaders.findIndex(
      (s, i) => s.shaderType === 'Fragment' && /\bvoid\s+main\s*\(\s*\)/.test(sources[i])
    )
    if (mainIdx === -1)
      mainIdx = shaders.findIndex(s => s.shaderType === 'Fragment')
    if (mainIdx === -1)
      return { ok: false, error: 'No Fragment shader found in program' }

    const mainSrc   = sources[mainIdx]
    const type      = this.inferType(expression, sources)
    const vec4Expr  = this.coerce(expression, type)
    const outVar    = this.detectOutVar(mainSrc)
    const rewritten = this.rewriteMain(mainSrc, outVar, vec4Expr)
    if (!rewritten)
      return { ok: false, error: 'Cannot locate void main() in shader' }

    const group = this.ensureGroup()

    // Texture (RGBA32F) – match existing target texture size if possible
    if (!this._texture) {
      const ref = app.session.findItem(item =>
        item.type === 'Texture' &&
        !(this._group && app.session.getParentItem(item)?.id === this._group.id)
      )
      this._texture = app.session.insertItem(group, {
        type: 'Texture', name: 'InspectorFBO', format: 'RGBA32F',
        width: ref?.width || '1280', height: ref?.height || '720',
        target: 'Target2D', samples: 1, flipVertically: false
      })
    }

    // Render target
    if (!this._target) {
      this._target = app.session.insertItem(group, {
        type: 'Target', name: 'InspectorTarget',
        cullMode: 'NoCulling', frontFace: 'CCW',
        items: [{
          type: 'Attachment', name: 'Color',
          textureId: this._texture.id, level: 0
        }]
      })
    }

    // Inspector program: mirror the original's shaders, override main frag source.
    // Rebuild each apply() call because expression/type may differ.
    if (this._program) {
      app.session.deleteItem(this._program)
      this._program = null
      this._call    = null
    }

    this._program = app.session.insertItem(group, {
      type: 'Program', name: 'InspectorProgram',
      items: shaders.map(s => ({
        type: 'Shader', shaderType: s.shaderType,
        fileName: s.fileName, language: s.language
      }))
    })
    app.session.setShaderSource(this._program.items[mainIdx], rewritten)

    // Draw call
    if (!this._call) {
      this._call = app.session.insertItem(group, {
        type: 'Call', name: 'InspectorDraw',
        callType: 'Draw', checked: true, executeOn: 'EveryEvaluation',
        programId: this._program.id, targetId: this._target.id,
        primitiveType: 'TriangleStrip',
        count: '4', first: '0', instanceCount: '1', vertexStreamId: 0
      })
    } else {
      this._call.programId = this._program.id
    }

    app.session.openEditor(this._texture)

    this.enabled    = true
    this.expression = expression
    this.lastType   = type
    return { ok: true, type }
  }

  cleanup() {
    if (this._group) app.session.deleteItem(this._group)
    this._group = this._texture = this._target = this._program = this._call = null
    this.enabled    = false
    this.expression = ''
    this.lastType   = ''
  }
}

if (!this.inspector) this.inspector = new Inspector()
if (!this.arguments) app.openEditor("ui.qml", "🔬 Inspector")
