// modes: 0 records, 1/2 TLAS rebuild/update, 3/4 BLAS rebuild/update
const mode = 4
instanceCount = 3
animateInstances = mode == 1 || mode == 2

const library = app.loadLibrary("GenerateMesh")
const geometry = library.generate(JSON.stringify({
  type: "Trefoil Knot", slices: 32, stacks: 96, radius: 1.5,
  scaleU: 25, indexed: true
}))
const vertices = library.getVertices(geometry)
const indices = library.getIndices(geometry)
if (!vertices.length || !indices.length)
  throw new Error("Generating geometry failed")
vertexCount = vertices.length / 8
const vertexBlock = app.getItem("Vertices/Vertex")
const indexBlock = app.getItem("Indices/Triangle")
vertexBlock.rowCount = vertexCount
indexBlock.rowCount = indices.length / 3
app.setBlockData(vertexBlock, vertices)
app.setBlockData(indexBlock, indices)

const restBlock = app.getItem("RestVertices/Vertex")
restBlock.rowCount = vertexCount
app.setBlockData(restBlock, vertices)
app.getItem("Scene").allowTlasUpdate = mode == 2 || mode == 4
app.getItem("Scene").allowBlasUpdate = mode == 4
app.getItem("Deform/Deform Mesh").checked = mode == 3 || mode == 4
