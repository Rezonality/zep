#pragma once

namespace Mgfx
{

class BufferGL;

class GeometryGL : public IGeometry
{
public:
    GeometryGL(DeviceGL* pDevice);
    ~GeometryGL();

    virtual void EndGeometry() override;
    virtual void BeginGeometry(uint32_t id, IDeviceBuffer* pVB, IDeviceBuffer* pIB) override;
    virtual void DrawTriangles(
        uint32_t VBOffset,
        uint32_t IBOffset,
        uint32_t numVertices,
        uint32_t numIndices) override;
    virtual void DrawLines(
        uint32_t VBOffset,
        uint32_t IBOffset,
        uint32_t numVertices,
        uint32_t numIndices) override;

private:
    DeviceGL* m_pDevice = nullptr;
    uint32_t VertexArrayID = 0;
    glm::vec4 lastTarget = glm::vec4(0.0f);
    glm::vec4 lastCoords = glm::vec4(0.0f);
};

} // namespace Mgfx
