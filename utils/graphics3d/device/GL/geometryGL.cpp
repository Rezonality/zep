#include "mcommon.h"
#include "deviceGL.h"
#include "geometryGL.h"
#include "bufferGL.h"
#include "file/media.h"

#include "camera.h"
namespace Mgfx
{

GeometryGL::GeometryGL(DeviceGL* pDevice)
    : m_pDevice(pDevice)
{
    CHECK_GL(glGenVertexArrays(1, &VertexArrayID));
}

GeometryGL::~GeometryGL()
{
    glDeleteVertexArrays(1, &VertexArrayID);
}

void GeometryGL::EndGeometry()
{
    CHECK_GL(glBindVertexArray(0));
}

void GeometryGL::BeginGeometry(uint32_t id, IDeviceBuffer* pVB, IDeviceBuffer* pIB)
{
    // Vertices
    CHECK_GL(glBindVertexArray(VertexArrayID));

    pVB->Bind();
    pIB->Bind();

    CHECK_GL(glEnableVertexAttribArray(0));
    CHECK_GL(glEnableVertexAttribArray(1));
    CHECK_GL(glEnableVertexAttribArray(2));
    CHECK_GL(glEnableVertexAttribArray(3));

    // index, size, type, norm, stride, pointer
    CHECK_GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)offsetof(GeometryVertex, pos)));
    CHECK_GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)offsetof(GeometryVertex, normal)));
    CHECK_GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)offsetof(GeometryVertex, tex)));
    CHECK_GL(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(GeometryVertex), (void*)offsetof(GeometryVertex, color)));
}

void GeometryGL::DrawTriangles(
    uint32_t VBOffset,
    uint32_t IBOffset,
    uint32_t numVertices,
    uint32_t numIndices)
{
    CHECK_GL(glDrawElementsBaseVertex(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, (void*)(IBOffset * sizeof(uint32_t)), VBOffset));
}

void GeometryGL::DrawLines(
    uint32_t VBOffset,
    uint32_t IBOffset,
    uint32_t numVertices,
    uint32_t numIndices)
{
    CHECK_GL(glDrawElementsBaseVertex(GL_LINES, numIndices, GL_UNSIGNED_INT, (void*)(IBOffset * sizeof(uint32_t)), VBOffset));
}

} // namespace Mgfx
