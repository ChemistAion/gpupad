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
    // Histogram compute pipeline
    this._histBuffer      = null
    this._histProgram     = null
    this._histImageBind   = null
    this._histBufferBind  = null
    this._histClearCall   = null
    this._histComputeCall = null
    // Composite display pipeline
    this._compTexture     = null
    this._compTarget      = null
    this._compProgram     = null
    this._compSamplerBind = null
    this._compModeBind    = null
    this._compRangeBind   = null
    this._compChanBind    = null
    this._compOORBind     = null
    this._compHistHBind   = null
    this._compCall        = null
    this.enabled    = false
    this.expression = ''
    this.lastType   = ''
  }

  // ── Validation

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

    // Function return type: TYPE funcName(  e.g. vec3 myHelper(
    const fm = combined.match(
      new RegExp(`\\b(float|int|uint|bool|vec[234]|ivec[234]|uvec[234]|mat[234])\\s+${esc}\\s*\\(`)
    )
    if (fm) return fm[1]

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
    // layout(location=0) out vec4 name;  or  out vec4 name;
    const m = src.match(/(?:layout\s*\([^)]*\)\s*)?out\s+vec4\s+(\w+)\s*;/)
    return m ? m[1] : 'fragColor'
  }

  // For mainImage(out vec4 <name>, ...) style shaders
  detectMainImageOutVar(src) {
    const m = src.match(/\bvoid\s+mainImage\s*\(\s*out\s+vec4\s+(\w+)/)
    return m ? m[1] : 'fragColor'
  }

  // Replace void main() and everything after it with the inspector body.
  rewriteMain(src, outVar, vec4Expr) {
    const idx = src.search(/\bvoid\s+main\s*\(\s*\)/)
    if (idx === -1) return null
    return (
      src.slice(0, idx) +
      `void main() {\n  ${outVar} = ${vec4Expr};\n}\n`
    )
  }

  // Inject inspector output right before the closing brace of mainImage().
  // All locals are in scope there, so any local variable can be inspected.
  rewriteMainImage(src, outVar, vec4Expr) {
    const funcMatch = src.match(/\bvoid\s+mainImage\s*\(/)
    if (!funcMatch) return null

    // Skip parameter list to find the function body '{'
    let pos = funcMatch.index + funcMatch[0].length
    let parenDepth = 1
    while (pos < src.length && parenDepth > 0) {
      if      (src[pos] === '(') parenDepth++
      else if (src[pos] === ')') parenDepth--
      pos++
    }
    while (pos < src.length && src[pos] !== '{') pos++
    if (pos >= src.length) return null

    // Find matching closing brace
    let depth = 0, bodyEnd = -1
    for (let i = pos; i < src.length; i++) {
      if      (src[i] === '{') depth++
      else if (src[i] === '}') { if (--depth === 0) { bodyEnd = i; break } }
    }
    if (bodyEnd === -1) return null

    return (
      src.slice(0, bodyEnd) +
      `  ${outVar} = ${vec4Expr};\n` +
      src.slice(bodyEnd)
    )
  }

  // ── Shader targeting ────────────────────────────────────────────────────────

  // Determine which linked fragment shader to rewrite for a given expression.
  // Simple identifiers: search each shader for a local variable declaration.
  // Complex expressions / built-ins: always use the shader that has void main().
  findExpressionShader(expr, shaders, sources) {
    const e = expr.trim()

    // Extract base identifier (before any swizzle, operator, or call)
    const baseMatch = e.match(/^(\w+)/)
    const base = baseMatch ? baseMatch[1] : null

    // GLSL built-ins (gl_*) and expressions without a simple base → main() shader
    if (!base || /^gl_/.test(base))
      return this._findMainShaderIdx(shaders, sources)

    // Search each fragment shader for a local-variable declaration of `base`
    const localRe = new RegExp(
      `\\b(float|int|uint|bool|vec[234]|ivec[234]|uvec[234]|mat[234])\\s+${escapeRegex(base)}\\s*[=;,)]`
    )
    for (let i = 0; i < shaders.length; i++) {
      if (shaders[i].shaderType !== 'Fragment') continue
      if (localRe.test(sources[i])) return i
    }

    // Not a local — use main() shader (handles uniforms, built-ins, functions)
    return this._findMainShaderIdx(shaders, sources)
  }

  _findMainShaderIdx(shaders, sources) {
    const idx = shaders.findIndex(
      (s, i) => s.shaderType === 'Fragment' && /\bvoid\s+main\s*\(\s*\)/.test(sources[i])
    )
    return idx !== -1 ? idx : shaders.findIndex(s => s.shaderType === 'Fragment')
  }

  // ── Range hint ──────────────────────────────────────────────────────────────

  // Scan shader sources for  // [min, max]  near the base identifier.
  // Returns { min, max } or null.
  parseRangeHint(expr, sources) {
    const baseMatch = expr.trim().match(/^(\w+)/)
    if (!baseMatch) return null
    const nameRe   = new RegExp(`\\b${escapeRegex(baseMatch[1])}\\b`)
    const rangeRe  = /\/\/\s*\[\s*([-\d.eE+]+)\s*,\s*([-\d.eE+]+)\s*\]/
    for (const src of sources) {
      for (const line of src.split(/\r?\n/)) {
        if (!nameRe.test(line)) continue
        const m = line.match(rangeRe)
        if (m) {
          const min = parseFloat(m[1]), max = parseFloat(m[2])
          if (!isNaN(min) && !isNaN(max) && max >= min) return { min, max }
        }
      }
    }
    return null
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
    this._histBuffer = null
    this._histProgram = null
    this._histImageBind = null
    this._histBufferBind = null
    this._histClearCall = null
    this._histComputeCall = null
    this._compTexture = null
    this._compTarget = null
    this._compProgram = null
    this._compSamplerBind = null
    this._compModeBind = null
    this._compRangeBind = null
    this._compChanBind = null
    this._compOORBind = null
    this._compHistHBind = null
    this._compCall = null
    return this._group
  }

  // ── Public API ──────────────────────────────────────────────────────────────

  apply(expression) {
    const v = this.validate(expression)
    if (!v.ok) return v

    const program = this.findTargetProgram()
    if (!program)
      return { ok: false, error: 'No active Draw call found in session' }

    // Collect all shaders; read fragment sources
    const shaders = program.items.filter(s => s.type === 'Shader')
    const sources = shaders.map(s =>
      (s.shaderType !== 'Vertex' && s.fileName)
        ? (app.readTextFile(s.fileName) || '') : ''
    )

    // Find the best-matching shader for this expression
    const targetIdx = this.findExpressionShader(expression, shaders, sources)
    if (targetIdx === -1)
      return { ok: false, error: 'No Fragment shader found in program' }

    const targetSrc    = sources[targetIdx]
    const type         = this.inferType(expression, sources)
    const vec4Expr     = this.coerce(expression, type)
    const rangeHint    = this.parseRangeHint(expression, sources)
    const hasMain      = /\bvoid\s+main\s*\(\s*\)/.test(targetSrc)
    const hasMainImage = /\bvoid\s+mainImage\s*\(/.test(targetSrc)

    let rewritten
    if (hasMain) {
      rewritten = this.rewriteMain(targetSrc, this.detectOutVar(targetSrc), vec4Expr)
    } else if (hasMainImage) {
      rewritten = this.rewriteMainImage(
        targetSrc, this.detectMainImageOutVar(targetSrc), vec4Expr)
      if (!rewritten) {
        const mainIdx = this._findMainShaderIdx(shaders, sources)
        if (mainIdx !== -1) {
          const src2 = sources[mainIdx]
          rewritten  = this.rewriteMain(src2, this.detectOutVar(src2), vec4Expr)
        }
      }
    } else {
      rewritten = this.rewriteMain(targetSrc, this.detectOutVar(targetSrc), vec4Expr)
    }

    if (!rewritten)
      return { ok: false, error: 'Cannot locate entry point in shader' }

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
    app.session.setShaderSource(this._program.items[targetIdx], rewritten)

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

    // ── Histogram compute pipeline

    if (!this._histBuffer) {
      this._histBuffer = app.session.insertItem(group, {
        type: 'Buffer', name: 'HistogramSSBO',
        items: [{
          type: 'Block', name: 'HistogramBlock', offset: '0', rowCount: '1',
          items: [
            { type: 'Field', name: 'binsR',       dataType: 'Uint32', count: 128, padding: 0 },
            { type: 'Field', name: 'binsG',       dataType: 'Uint32', count: 128, padding: 0 },
            { type: 'Field', name: 'binsB',       dataType: 'Uint32', count: 128, padding: 0 },
            { type: 'Field', name: 'binsA',       dataType: 'Uint32', count: 128, padding: 0 },
            { type: 'Field', name: 'uDataMin',    dataType: 'Uint32', count: 1, padding: 0 },
            { type: 'Field', name: 'uDataMax',    dataType: 'Uint32', count: 1, padding: 0 },
            { type: 'Field', name: 'uAutoMin',    dataType: 'Uint32', count: 1, padding: 0 },
            { type: 'Field', name: 'uAutoMax',    dataType: 'Uint32', count: 1, padding: 0 },
            { type: 'Field', name: 'totalPixels', dataType: 'Uint32', count: 1, padding: 0 }
          ]
        }]
      })
    }

    if (!this._histProgram) {
      this._histProgram = app.session.insertItem(group, {
        type: 'Program', name: 'HistogramProgram',
        items: [{
          type: 'Shader', shaderType: 'Compute',
          language: 'GLSL'
        }]
      })
      app.session.setShaderSource(
        this._histProgram.items[0], app.readTextFile('histogram.comp'))
    }

    if (!this._histImageBind) {
      this._histImageBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uInspectorTex',
        bindingType: 'Image',
        textureId: this._texture.id,
        imageFormat: 'Internal',
        level: 0, layer: 0
      })
    }

    if (!this._histBufferBind) {
      this._histBufferBind = app.session.insertItem(group, {
        type: 'Binding', name: 'HistogramBuffer',
        bindingType: 'Buffer',
        bufferId: this._histBuffer.id
      })
    }

    if (!this._histClearCall) {
      this._histClearCall = app.session.insertItem(group, {
        type: 'Call', name: 'HistogramClear',
        callType: 'ClearBuffer', checked: true,
        executeOn: 'EveryEvaluation',
        bufferId: this._histBuffer.id
      })
    }

    if (!this._histComputeCall) {
      // NOTE: parseInt handles fixed-size textures; viewport-expression sizes
      // fall back to 1280x720 (shader bounds-checks handle overshoot)
      const texW = parseInt(this._texture.width) || 1280
      const texH = parseInt(this._texture.height) || 720
      this._histComputeCall = app.session.insertItem(group, {
        type: 'Call', name: 'HistogramCompute',
        callType: 'Compute', checked: true,
        executeOn: 'EveryEvaluation',
        programId: this._histProgram.id,
        workGroupsX: String(Math.ceil(texW / 16)),
        workGroupsY: String(Math.ceil(texH / 16)),
        workGroupsZ: '1'
      })
    }

    // ── Composite display pipeline ────────────────────────────────────────────

    if (!this._compTexture) {
      this._compTexture = app.session.insertItem(group, {
        type: 'Texture', name: 'InspectorDisplay', format: 'RGBA8_UNorm',
        width: this._texture.width, height: this._texture.height,
        target: 'Target2D', samples: 1, flipVertically: false
      })
    }

    if (!this._compTarget) {
      this._compTarget = app.session.insertItem(group, {
        type: 'Target', name: 'CompositeTarget',
        cullMode: 'NoCulling', frontFace: 'CCW',
        items: [{
          type: 'Attachment', name: 'Color',
          textureId: this._compTexture.id, level: 0
        }]
      })
    }

    if (!this._compProgram) {
      this._compProgram = app.session.insertItem(group, {
        type: 'Program', name: 'CompositeProgram',
        items: [
          { type: 'Shader', shaderType: 'Vertex',
            language: 'GLSL' },
          { type: 'Shader', shaderType: 'Fragment',
            language: 'GLSL' }
        ]
      })
      app.session.setShaderSource(
        this._compProgram.items[0], app.readTextFile('attributeless.vs'))
      app.session.setShaderSource(
        this._compProgram.items[1], app.readTextFile('composite.fs'))
    }

    if (!this._compSamplerBind) {
      this._compSamplerBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uInspectorSampler',
        bindingType: 'Sampler',
        textureId: this._texture.id,
        minFilter: 'Nearest', magFilter: 'Nearest'
      })
    }

    if (!this._compModeBind) {
      this._compModeBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uMappingMode',
        bindingType: 'Uniform', editor: 'Expression',
        values: ['0']
      })
    }

    if (!this._compRangeBind) {
      const rMin = rangeHint ? String(rangeHint.min) : '0.0'
      const rMax = rangeHint ? String(rangeHint.max) : '1.0'
      this._compRangeBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uMappingRange',
        bindingType: 'Uniform', editor: 'Expression2',
        values: [rMin, rMax]
      })
    }

    if (!this._compChanBind) {
      this._compChanBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uChannelMask',
        bindingType: 'Uniform', editor: 'Expression4',
        values: ['1', '1', '1', '0']
      })
    }

    if (!this._compOORBind) {
      this._compOORBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uHighlightOOR',
        bindingType: 'Uniform', editor: 'Expression',
        values: ['0']
      })
    }

    if (!this._compHistHBind) {
      this._compHistHBind = app.session.insertItem(group, {
        type: 'Binding', name: 'uHistogramHeight',
        bindingType: 'Uniform', editor: 'Expression',
        values: ['0.2']
      })
    }

    if (!this._compCall) {
      this._compCall = app.session.insertItem(group, {
        type: 'Call', name: 'CompositeDraw',
        callType: 'Draw', checked: true,
        executeOn: 'EveryEvaluation',
        programId: this._compProgram.id, targetId: this._compTarget.id,
        primitiveType: 'TriangleStrip',
        count: '4', first: '0', instanceCount: '1', vertexStreamId: 0
      })
    }

    app.session.openEditor(this._compTexture)

    this.enabled    = true
    this.expression = expression
    this.lastType   = type
    return { ok: true, type, rangeHint }
  }

  cleanup() {
    if (this._group) app.session.deleteItem(this._group)
    this._group = this._texture = this._target = this._program = this._call = null
    this._histBuffer = this._histProgram = this._histImageBind = null
    this._histBufferBind = this._histClearCall = this._histComputeCall = null
    this._compTexture = this._compTarget = this._compProgram = null
    this._compSamplerBind = null
    this._compModeBind = this._compRangeBind = this._compChanBind = null
    this._compOORBind = this._compHistHBind = this._compCall = null
    this.enabled    = false
    this.expression = ''
    this.lastType   = ''
  }
}

if (!this.inspector) this.inspector = new Inspector()
if (!this.arguments) app.openEditor("ui.qml", "🔬 Inspector")
