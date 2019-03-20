#include "mcommon.h"
#include "deviceGL.h"
#include "geometryGL.h"
#include "bufferGL.h"
#include <graphics3d/camera/camera.h>
#include "file/media_manager.h"

namespace Mgfx
{

GeometryGL::GeometryGL(DeviceGL* pDevice)
    : m_pDevice(pDevice)
{
    CHECK_GL(glGenVertexArrays(1, &VertexArrayID));
    CHECK_GL(glBindVertexArray(VertexArrayID));

    m_spPositionBuffer = std::make_shared<BufferGL>(m_pDevice, 1000);
    m_spColorBuffer = std::make_shared<BufferGL>(m_pDevice, 1000);
    m_spTexCoordBuffer = std::make_shared<BufferGL>(m_pDevice, 1000);
    m_spIndexBuffer = std::make_shared<BufferGL>(m_pDevice, 4000, GL_ELEMENT_ARRAY_BUFFER);
    
    CHECK_GL(glEnableVertexAttribArray(0));
    CHECK_GL(glEnableVertexAttribArray(1));
    CHECK_GL(glEnableVertexAttribArray(2));

    CHECK_GL(glBindVertexArray(0));
    
    // Create and compile our GLSL program from the shaders
    m_programID = LoadShaders(MediaManager::Instance().FindAsset("Quad.vertexshader", MediaType::Shader).c_str(), MediaManager::Instance().FindAsset("Quad.fragmentshader", MediaType::Shader).c_str());
    
    glUseProgram(m_programID);
    glActiveTexture(GL_TEXTURE0);
    m_samplerID = glGetUniformLocation(m_programID, "albedo_sampler");
    m_projectionID = glGetUniformLocation(m_programID, "Projection");
    glUniform1i(m_samplerID, 0);
    glUseProgram(0);

}

GeometryGL::~GeometryGL()
{
    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteProgram(m_programID);
    for (auto& qd : m_mapQuadData)
    {
        glDeleteTextures(1, &qd.first);
    }
    m_mapQuadData.clear();
}

uint32_t GeometryGL::CreateQuad()
{

    uint32_t tex;
    CHECK_GL(glGenTextures(1, &tex));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, tex));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));

    auto spQuadData = std::make_shared<QuadDataGL>();
    CHECK_GL(glGenBuffers(1, &spQuadData->PBOBufferID));
    spQuadData->currentPBO = 0;

    m_mapQuadData[tex] = spQuadData;
    return tex;
}

void GeometryGL::DestroyQuad(uint32_t id)
{
    auto itr = m_mapQuadData.find(id);
    if (itr == m_mapQuadData.end())
    {
        return;
    }
    glDeleteBuffers(1, &itr->second->PBOBufferID);
    m_mapQuadData.erase(itr);

    CHECK_GL(glDeleteTextures(1, &id));
}

glm::u8vec4* GeometryGL::ResizeQuad(uint32_t id, const glm::uvec2& size)
{
    auto itr = m_mapQuadData.find(id);
    if (itr == m_mapQuadData.end())
    {
        return nullptr;
    }

    itr->second->quadData.resize(size.x * size.y);
    itr->second->RequiredSize = size;
    return &itr->second->quadData[0];
}

void GeometryGL::UpdateQuad(uint32_t id)
{
    auto itr = m_mapQuadData.find(id);
    if (itr == m_mapQuadData.end())
    {
        return;
    }

    auto byteSize = itr->second->quadData.size() * sizeof(glm::u8vec4);
    auto& spQuad = itr->second;
    bool skip = false;
    if (spQuad->Size != spQuad->RequiredSize)
    {
        CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, spQuad->PBOBufferID));
        CHECK_GL(glBufferData(GL_PIXEL_UNPACK_BUFFER, byteSize, 0, GL_STREAM_DRAW));
        itr->second->Size = spQuad->RequiredSize;
        skip = true;
    }

    CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, spQuad->PBOBufferID));
    GLubyte* ptr = (GLubyte*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, byteSize, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
    memcpy(ptr, &itr->second->quadData[0], byteSize);
    CHECK_GL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, spQuad->Size.x, spQuad->Size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
    CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
}

void GeometryGL::BeginGeometry()
{
    CHECK_GL(glDisable(GL_CULL_FACE));
    CHECK_GL(glEnable(GL_DEPTH_TEST));
    CHECK_GL(glEnable(GL_BLEND));

    CHECK_GL(glUseProgram(m_programID));
    CHECK_GL(glBindVertexArray(VertexArrayID));
    CHECK_GL(glActiveTexture(GL_TEXTURE0));
  
    auto pCamera = m_pDevice->GetCamera();
    if (pCamera)
    {
        glm::mat4 projection = pCamera->GetProjection();
        glm::mat4 view = glm::mat4(1.0f);// pCamera->GetLookAt();
        glm::mat4 model = glm::mat4(1.0f);
        glm::mat4 MVP = projection * view * model;

        // Send our transformation to the currently bound shader, 
        // in the "MVP" uniform
        CHECK_GL(glUniformMatrix4fv(m_projectionID, 1, GL_FALSE, &MVP[0][0]));
    }
}

void GeometryGL::EndGeometry()
{
    CHECK_GL(glUseProgram(0));
    CHECK_GL(glEnable(GL_CULL_FACE));
    CHECK_GL(glEnable(GL_DEPTH_TEST));
}

void GeometryGL::DrawQuads(uint32_t id, const std::vector<GeometryVertex>& vertices)
{
    BeginGeometry();

    auto itr = m_mapQuadData.find(id);
    if (itr == m_mapQuadData.end())
    {
        return;
    }

    if (vertices.size() % 4 != 0)
    {
        LOG(ERROR) << "Invalid vertex count for quads!";
        return;
    }

    uint32_t numIndices = (uint32_t(vertices.size()) / 4) * 6;
    auto pPos = (glm::vec2*)m_spPositionBuffer->Map(uint32_t(vertices.size()), sizeof(glm::vec2), posOffset);
    auto pColor = (glm::vec4*)m_spColorBuffer->Map(uint32_t(vertices.size()), sizeof(glm::vec4), colorOffset);
    auto pTexCoord = (glm::vec2*)m_spTexCoordBuffer->Map(uint32_t(vertices.size()), sizeof(glm::vec2), texCoordOffset);
    auto pIndices = (uint32_t*)m_spIndexBuffer->Map(numIndices, sizeof(uint32_t), indexOffset);

    for (uint32_t count = 0; count < vertices.size(); count++)
    {
        pPos[count] = glm::vec2(vertices[count].pos);
        pTexCoord[count] = vertices[count].tex;
        pColor[count] = vertices[count].color;
    }

    for (uint32_t count = 0; count < vertices.size() / 4; count++)
    {
        uint32_t offset = count * 6;
        uint32_t indexOffset = count * 4;
        pIndices[offset + 0] = indexOffset + 0;
        pIndices[offset + 1] = indexOffset + 1;
        pIndices[offset + 2] = indexOffset + 2;
        pIndices[offset + 3] = indexOffset + 2;
        pIndices[offset + 4] = indexOffset + 1;
        pIndices[offset + 5] = indexOffset + 3;
    }

    m_spPositionBuffer->UnMap();
    m_spColorBuffer->UnMap();
    m_spTexCoordBuffer->UnMap();
    m_spIndexBuffer->UnMap();

    CHECK_GL(glActiveTexture(GL_TEXTURE0));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));

    m_spPositionBuffer->Bind();
    CHECK_GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(uintptr_t)posOffset));

    m_spTexCoordBuffer->Bind();
    CHECK_GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)(uintptr_t)texCoordOffset));

    m_spColorBuffer->Bind();
    CHECK_GL(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)(uintptr_t)colorOffset));

    m_spIndexBuffer->Bind();
    CHECK_GL(glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, (void*)(uintptr_t)indexOffset));

    EndGeometry();
}

} // namespace Mgfx
