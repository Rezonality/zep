#include "mcommon.h"

#include <easylogging/src/easylogging++.h>

#include "device/GL/deviceGL.h"
#include "device/GL/geometryGL.h"
#include "device/GL/bufferGL.h"

#include "ui/imgui_sdl_common.h"

#include "file/media.h"

#include "gli/gli.hpp"

#include "camera.h"
#include "body.h"
#include "mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "imgui.h"
namespace Mgfx
{

inline void CheckGL(const char* call, const char* file, int line)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        LOG(ERROR) << std::hex << err << ", " << file << "(" << line << "): " << call;
#if (_MSC_VER)
        DebugBreak();
#endif
    }
}
void APIENTRY DebugCB(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *param)
{
    //DeviceGL* pDevice = (DeviceGL*)param;
    if (type != GL_DEBUG_TYPE_OTHER)
    {
        if (type == GL_DEBUG_TYPE_PERFORMANCE)
        {
            LOG(INFO) << "GLPerf: " << message;
        }
        else if (type == GL_DEBUG_TYPE_ERROR)
        {
            LOG(ERROR) << "GLError: " << message;
            assert(!message);
        }
        else
        {
            LOG(WARNING) << "GL: " << message;
        }
    }
}

DeviceGL::DeviceGL()
{

}

DeviceGL::~DeviceGL()
{

}

bool DeviceGL::Init(const char* pszWindowName)
{
    // Setup window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    pSDLWindow = SDL_CreateWindow(pszWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    glContext = SDL_GL_CreateContext(pSDLWindow);
    SDL_GL_MakeCurrent(pSDLWindow, glContext);


    static bool initOnce = false;
    if (!initOnce)
    {
        gl3wInit();
        initOnce = true;
    }

    // Get useful debug messages
    if (glDebugMessageCallback != nullptr)
    {
        CHECK_GL(glDebugMessageCallback((GLDEBUGPROC)&DebugCB, this));
        CHECK_GL(glEnable(GL_DEBUG_OUTPUT));
    }

    m_spImGuiDraw = std::make_shared<ImGuiSDL_GL3>();

    SDL_GL_SetSwapInterval(1);

    // Create and compile our GLSL program from the shaders
    programID = LoadShaders(media_find_asset("shaders/GL/StandardShading.vertexshader").c_str(), media_find_asset("shaders/GL/StandardShading.fragmentshader").c_str());
    if (programID == 0)
    {
        return false;
    }

    if (programID != 0)
    {
        // Get a handle for our "LightPosition" uniform
        CHECK_GL(glUseProgram(programID));

        // Get a handle for our "MVP" uniform
        MatrixID = glGetUniformLocation(programID, "MVP");
        ViewMatrixID = glGetUniformLocation(programID, "V");
        ModelMatrixID = glGetUniformLocation(programID, "M");

        // Get a handle for our "myTextureSampler" uniform
        TextureID = glGetUniformLocation(programID, "albedo_sampler");
        TextureIDNormal = glGetUniformLocation(programID, "normal_sampler");

        CameraID = glGetUniformLocation(programID, "camera_pos");
        LightDirID = glGetUniformLocation(programID, "light_dir");

        HasNormalMapID = glGetUniformLocation(programID, "has_normalmap");
        IsLitID = glGetUniformLocation(programID, "is_lit");
        HasTextureID = glGetUniformLocation(programID, "has_texture");

        ConstantDiffuseID = glGetUniformLocation(programID, "constant_diffuse_in");
    }

    CHECK_GL(glGenVertexArrays(1, &VertexArrayID));
    CHECK_GL(glBindVertexArray(VertexArrayID));

    CHECK_GL(glGenTextures(1, &BackBufferTextureID));

    // Setup ImGui binding
    m_spImGuiDraw->Init(pSDLWindow);

    m_spGeometry = std::make_shared<GeometryGL>(this);
    return true;
}

void DeviceGL::DestroyDeviceMeshes()
{
    SDL_GL_MakeCurrent(pSDLWindow, glContext);

    for (auto& spMesh : m_mapDeviceMeshes)
    {
        DestroyDeviceMesh(spMesh.second.get());
    }
    m_mapDeviceMeshes.clear();

    for (auto& tex : m_mapPathToTextureID)
    {
        glDeleteTextures(1, &tex.second);
    }
    m_mapPathToTextureID.clear();
}

void DeviceGL::DestroyDeviceMesh(GLMesh* pDeviceMesh)
{
    SDL_GL_MakeCurrent(pSDLWindow, glContext);
    for (auto& indexPart : pDeviceMesh->m_glMeshParts)
    {
        auto& spGLPart = indexPart.second;
        glDeleteBuffers(1, &spGLPart->normalID);
        glDeleteBuffers(1, &spGLPart->positionID);
        glDeleteBuffers(1, &spGLPart->uvID);
        glDeleteBuffers(1, &spGLPart->diffuseID);
    }
}

std::shared_ptr<IDeviceBuffer> DeviceGL::CreateBuffer(uint32_t size, uint32_t flags)
{
    return std::static_pointer_cast<IDeviceBuffer>(std::make_shared<BufferGL>(this, size, flags));
}

uint32_t DeviceGL::LoadTexture(const fs::path& path)
{
    if (!fs::exists(path))
    {
        return 0;
    }

    SDL_GL_MakeCurrent(pSDLWindow, glContext);
    auto itr = m_mapPathToTextureID.find(path);
    if (itr == m_mapPathToTextureID.end())
    {
        GLuint TextureName = 0;
        if (path.extension().string() == ".dds")
        {
            gli::texture Texture = gli::load(path.string());
            if (Texture.empty())
                return 0;

            Texture = gli::flip(Texture);

            gli::gl GL(gli::gl::PROFILE_GL33);
            gli::gl::format const Format = GL.translate(Texture.format(), Texture.swizzles());
            GLenum Target = GL.translate(Texture.target());
            //assert(gli::is_compressed(Texture.format()) && Target == gli::gl::TARGET_2D);

            glGenTextures(1, &TextureName);
            glBindTexture(Target, TextureName);

            CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
            CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
            if (Texture.levels() > 1)
            {
                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
            }
            else
            {
                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            }
            CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

            glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(Texture.levels() - 1));
            glTexParameteriv(Target, GL_TEXTURE_SWIZZLE_RGBA, &Format.Swizzles[0]);
            glTexStorage2D(Target, static_cast<GLint>(Texture.levels()), Format.Internal, Texture.extent().x, Texture.extent().y);
            for (std::size_t Level = 0; Level < Texture.levels(); ++Level)
            {
                glm::tvec3<GLsizei> Extent(Texture.extent(Level));
                if (gli::is_compressed(Texture.format()))
                {
                    glCompressedTexSubImage2D(
                        Target, static_cast<GLint>(Level), 0, 0, Extent.x, Extent.y,
                        Format.Internal, static_cast<GLsizei>(Texture.size(Level)), Texture.data(0, 0, Level));
                }
                else
                {
                    glTexSubImage2D(Target, static_cast<GLint>(Level), 0, 0, Extent.x, Extent.y, Format.External, Format.Type, Texture.data(0, 0, Level));
                }
            }
        }
        else
        {
            int w;
            int h;
            int comp;
            unsigned char* image = stbi_load(path.string().c_str(), &w, &h, &comp, STBI_default);

            assert(image != nullptr);
            if (image != nullptr)
            {
                CHECK_GL(glGenTextures(1, &TextureName));
                CHECK_GL(glBindTexture(GL_TEXTURE_2D, TextureName));

                if (comp == 3)
                {
                    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, image));
                }
                else if (comp == 4)
                {
                    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image));
                }

                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
                CHECK_GL(glGenerateMipmap(GL_TEXTURE_2D));
                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
                CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

                CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));

                stbi_image_free(image);
            }
        }

        if (TextureName != 0)
        {
            m_mapPathToTextureID[path] = TextureName;
        }
        return TextureName;
    }
    return itr->second;
}

uint32_t DeviceGL::CreateTexture()
{

    uint32_t tex;
    CHECK_GL(glGenTextures(1, &tex));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, tex));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    CHECK_GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));

    auto spTextureData = std::make_shared<TextureDataGL>();
    CHECK_GL(glGenBuffers(1, &spTextureData->PBOBufferID));
    spTextureData->currentPBO = 0;

    m_mapIDToTextureData[tex] = spTextureData;
    return tex;
}

void DeviceGL::DestroyTexture(uint32_t id)
{
    auto itr = m_mapIDToTextureData.find(id);
    if (itr == m_mapIDToTextureData.end())
    {
        return;
    }
    glDeleteBuffers(1, &itr->second->PBOBufferID);
    m_mapIDToTextureData.erase(itr);

    CHECK_GL(glDeleteTextures(1, &id));
}

TextureData DeviceGL::ResizeTexture(uint32_t id, const glm::uvec2& size)
{
    TextureData ret;
    auto itr = m_mapIDToTextureData.find(id);
    if (itr == m_mapIDToTextureData.end())
    {
        return ret;
    }

    itr->second->quadData.resize(size.x * size.y);
    itr->second->RequiredSize = size;
    ret.pData = &itr->second->quadData[0].x;
    ret.pitch = size.x * sizeof(glm::u8vec4);
    return ret;
}

void DeviceGL::UpdateTexture(uint32_t id)
{
    auto itr = m_mapIDToTextureData.find(id);
    if (itr == m_mapIDToTextureData.end())
    {
        return;
    }

    auto byteSize = itr->second->quadData.size() * sizeof(glm::u8vec4);
    auto& spTexture = itr->second;
    bool skip = false;
    if (spTexture->Size != spTexture->RequiredSize)
    {
        CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, spTexture->PBOBufferID));
        CHECK_GL(glBufferData(GL_PIXEL_UNPACK_BUFFER, byteSize, 0, GL_STREAM_DRAW));
        itr->second->Size = spTexture->RequiredSize;
        skip = true;
    }

    CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, spTexture->PBOBufferID));
    GLubyte* ptr = (GLubyte*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, byteSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    memcpy(ptr, &itr->second->quadData[0], byteSize);
    CHECK_GL(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));

    CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));
    CHECK_GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, spTexture->Size.x, spTexture->Size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0));
    CHECK_GL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
}

void DeviceGL::BeginGeometry(uint32_t id, IDeviceBuffer* pVB, IDeviceBuffer* pIB, const glm::mat4& world)
{
    SetTransform(world);
    
    CHECK_GL(glActiveTexture(GL_TEXTURE0));
    auto itr = m_mapIDToTextureData.find(id);
    if (itr != m_mapIDToTextureData.end())
    {
        auto& spTexture = itr->second;
        CHECK_GL(glUniform1i(HasTextureID, 1));
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, id));
    }
    else
    {
        CHECK_GL(glUniform1i(HasTextureID, 0));
        CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    }

    CHECK_GL(glActiveTexture(GL_TEXTURE1));
    CHECK_GL(glUniform1i(HasNormalMapID, 0));
    CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
    CHECK_GL(glUniform4f(ConstantDiffuseID, 1.0f, 1.0f, 1.0f, .33f));
    CHECK_GL(glUniform1i(IsLitID, 0));

    m_spGeometry->BeginGeometry(id, pVB, pIB);
}

void DeviceGL::EndGeometry()
{
    m_spGeometry->EndGeometry();
}

void DeviceGL::DrawTriangles(
    uint32_t VBOffset,
    uint32_t IBOffset,
    uint32_t numVertices,
    uint32_t numIndices)
{
    m_spGeometry->DrawTriangles(VBOffset, IBOffset, numVertices, numIndices);
}

void DeviceGL::DrawLines(
    uint32_t VBOffset,
    uint32_t IBOffset,
    uint32_t numVertices,
    uint32_t numIndices)
{
    m_spGeometry->DrawLines(VBOffset, IBOffset, numVertices, numIndices);
}

EntityId DeviceGL::GetCamera() const
{
    return m_currentCamera;
}

void DeviceGL::SetCamera(EntityId cameraEntity)
{
    if (cameraEntity != m_currentCamera)
    {
        m_currentCamera = cameraEntity;
    }

}

void DeviceGL::SetDeviceFlags(uint32_t flags)
{
    m_deviceFlags = flags;
    SDL_GL_SetSwapInterval(m_deviceFlags & DeviceFlags::SyncToRefresh ? 1 : 0);
}

bool DeviceGL::BeginFrame()
{
    if (m_inFrame)
    { 
        assert(!"BeginFrame called twice?");
        return false;
    }

    m_inFrame = true;

    SDL_GL_MakeCurrent(pSDLWindow, glContext);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Enable depth test
    CHECK_GL(glEnable(GL_DEPTH_TEST));

    // Accept fragment if it closer to the camera than the former one
    CHECK_GL(glDepthFunc(GL_GEQUAL));

    // Cull triangles which normal is not towards the camera
    CHECK_GL(glDisable(GL_CULL_FACE));

    // Use our shader
    CHECK_GL(glUseProgram(programID));

    // Bind our texture in Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(TextureID, 0);

    glActiveTexture(GL_TEXTURE1);
    glUniform1i(TextureIDNormal, 1);

    uint32_t flag = 0;
    if (m_clearFlags & ClearType::Color)
    {
        glClearColor(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);
        flag |= GL_COLOR_BUFFER_BIT;
    }

    if (m_clearFlags & ClearType::Depth)
    {
        glClearDepth(m_clearDepth);
        flag |= GL_DEPTH_BUFFER_BIT;
    }

    // Clear the screen
    glClear(flag);

    return true;
}

void DeviceGL::SetClear(const glm::vec4& clearColor, float depth, uint32_t clearFlags)
{
    m_clearColor = clearColor;
    m_clearDepth = depth;
    m_clearFlags = clearFlags;
}

std::shared_ptr<GLMesh> DeviceGL::BuildDeviceMesh(MeshComponent* pMesh)
{
    SDL_GL_MakeCurrent(pSDLWindow, glContext);
    auto spDeviceMesh = std::make_shared<GLMesh>();

    CHECK_GL(glBindVertexArray(VertexArrayID));

    auto pMeshData = mesh_get_data(*pMesh);
    for (auto& spPart : pMeshData->meshParts)
    {
        auto spGLPart = std::make_shared<GLMeshPart>();

        CHECK_GL(glGenBuffers(1, &spGLPart->positionID));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->positionID));
        CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*spPart->Positions.size(), spPart->Positions.data(), GL_STATIC_DRAW));

        CHECK_GL(glGenBuffers(1, &spGLPart->normalID));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->normalID));
        CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*spPart->Normals.size(), spPart->Normals.data(), GL_STATIC_DRAW));

        CHECK_GL(glGenBuffers(1, &spGLPart->uvID));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->uvID));
        CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2)*spPart->UVs.size(), spPart->UVs.data(), GL_STATIC_DRAW));

        CHECK_GL(glGenBuffers(1, &spGLPart->diffuseID));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->diffuseID));
        CHECK_GL(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4)*spPart->Diffuse.size(), spPart->Diffuse.data(), GL_STATIC_DRAW));

        CHECK_GL(glGenBuffers(1, &spGLPart->indicesID));
        CHECK_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spGLPart->indicesID));
        CHECK_GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t)*spPart->Indices.size(), spPart->Indices.data(), GL_STATIC_DRAW));

        spGLPart->numIndices = uint32_t(spPart->Indices.size());

        if (spPart->MaterialID != -1)
        {
            auto& mat = pMeshData->materials[spPart->MaterialID];
            if (!mat->diffuseTex.empty())
            {
                spGLPart->textureID = LoadTexture(media_find_asset(pMesh->m_rootPath / "textures" / mat->diffuseTex));

                // This is a hack to detect transparent textures in Sponza.
                // We could scan the texture for alpha < 1, or store the information in the scene file as a better solution
                if (std::string(mat->diffuseTex).find("thorn") != std::string::npos ||
                    std::string(mat->diffuseTex).find("plant") != std::string::npos ||
                    std::string(mat->diffuseTex).find("chain") != std::string::npos)
                {
                    spGLPart->transparent = true;
                }
            }

            if (!mat->normalTex.empty())
            {
                spGLPart->textureIDNormal = LoadTexture(media_find_asset(pMesh->m_rootPath / fs::path("textures") / fs::path(mat->normalTex)));
            }
            // Another fix for bad sponza data ;) TODO: Fix the source asset
            else if (!mat->heightTex.empty() && mat->heightTex.find("diff") == std::string::npos)
            {
                spGLPart->textureIDNormal = LoadTexture(media_find_asset(pMesh->m_rootPath / "textures" / mat->heightTex));
            }
            else
            {
                spGLPart->textureIDNormal = 0;
            }

        }
        else
        {
            spGLPart->textureID = 0;
            spGLPart->textureIDNormal = 0;
        }
        spDeviceMesh->m_glMeshParts[spPart.get()] = spGLPart;
    }

    return spDeviceMesh;
}

void DeviceGL::SetTransform(const glm::mat4& model)
{
    auto pCamera = component_get<CameraComponent>(m_currentCamera);
    auto pBody = component_get<BodyComponent>(m_currentCamera);
    if (pCamera && pBody)
    {
        auto position = pBody->position;
        auto orientation = pBody->orientation;
        glViewport(0, 0, pCamera->filmSize.x, pCamera->filmSize.y);

        // Use our shader
        CHECK_GL(glUseProgram(programID));
        glm::mat4 projection = Camera_GetProjection(*pCamera, ProjectionType::GL);
        glm::mat4 view = Camera_GetLookAt(*pCamera, position, orientation);
        glm::mat4 MVP = projection * view * model;

        // Send our transformation to the currently bound shader, 
        // in the "MVP" uniform
        CHECK_GL(glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]));
        CHECK_GL(glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &model[0][0]));
        glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &view[0][0]);

        int x, y;
        SDL_GetMouseState(&x, &y);
        glm::vec3 cameraLook = Camera_GetWorldRay(*pCamera, glm::vec2(x, y), position).direction;

        glUniform3f(CameraID, position.x, position.y, position.z);
        glUniform3f(LightDirID, cameraLook.x, cameraLook.y, cameraLook.z);
    }
    else
    {
        int w, h;
        SDL_GetWindowSize(pSDLWindow, &w, &h);
        glViewport(0, 0, w, h);
    }
}

void DeviceGL::DrawMesh(MeshComponent* pMesh, const glm::mat4& model, GeometryType type)
{
    GLMesh* pDeviceMesh = nullptr;
    auto itrFound = m_mapDeviceMeshes.find(pMesh);
    if (itrFound == m_mapDeviceMeshes.end() ||
        itrFound->second->generation != pMesh->generation)
    {
        auto spDeviceMesh = BuildDeviceMesh(pMesh);
        spDeviceMesh->generation = pMesh->generation;
        m_mapDeviceMeshes[pMesh] = spDeviceMesh;
        pDeviceMesh = spDeviceMesh.get();
    }
    else
    {
        pDeviceMesh = itrFound->second.get();
    }

    SetTransform(model);

    CHECK_GL(glBindVertexArray(VertexArrayID));

    for (auto& indexPart : pDeviceMesh->m_glMeshParts)
    {
        auto spGLPart = indexPart.second;

        // Skip if not the requested type
        if ((spGLPart->transparent && type == GeometryType::Opaque) ||
            (!spGLPart->transparent && type == GeometryType::Transparent))
        {
            continue;
        }

        CHECK_GL(glEnableVertexAttribArray(0));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->positionID));
        // attrib, size, type, normalized, stride, offset 
        CHECK_GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));

        CHECK_GL(glEnableVertexAttribArray(1));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->normalID));
        CHECK_GL(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));

        CHECK_GL(glEnableVertexAttribArray(2));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->uvID));
        CHECK_GL(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0));

        CHECK_GL(glEnableVertexAttribArray(3));
        CHECK_GL(glBindBuffer(GL_ARRAY_BUFFER, spGLPart->diffuseID));
        CHECK_GL(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, (void*)0));

        CHECK_GL(glActiveTexture(GL_TEXTURE0));
        if (spGLPart->textureID)
        {
            CHECK_GL(glUniform1i(HasTextureID, 1));
            CHECK_GL(glBindTexture(GL_TEXTURE_2D, spGLPart->textureID));
        }
        else
        {
            CHECK_GL(glUniform1i(HasTextureID, 0));
            CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
        }

        CHECK_GL(glActiveTexture(GL_TEXTURE1));
        if (spGLPart->textureIDNormal)
        {
            CHECK_GL(glUniform1i(HasNormalMapID, 1));
            CHECK_GL(glBindTexture(GL_TEXTURE_2D, spGLPart->textureIDNormal));
        }
        else
        {
            CHECK_GL(glUniform1i(HasNormalMapID, 0));
            CHECK_GL(glBindTexture(GL_TEXTURE_2D, 0));
        }
        CHECK_GL(glUniform1i(IsLitID, 1));

        if (indexPart.first->MaterialID != -1)
        {
            auto pMeshData = mesh_get_data(*indexPart.first->pMeshComponent.get());
            auto& mat = pMeshData->materials[indexPart.first->MaterialID];
            CHECK_GL(glUniform4f(ConstantDiffuseID, mat->globalDiffuse.x, mat->globalDiffuse.y, mat->globalDiffuse.z, mat->opacity));
        }
        else
        {
            CHECK_GL(glUniform4f(ConstantDiffuseID, 1.0f, 1.0f, 1.0f, 1.0f));
        }
        CHECK_GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, spGLPart->indicesID));
        CHECK_GL(glDrawElements(GL_TRIANGLES, spGLPart->numIndices, GL_UNSIGNED_INT, (void*)0));
    }

    CHECK_GL(glDisableVertexAttribArray(0));
    CHECK_GL(glDisableVertexAttribArray(1));
    CHECK_GL(glDisableVertexAttribArray(2));
}

void DeviceGL::Cleanup()
{
    // Cleanup IMGui and device
    SDL_GL_MakeCurrent(pSDLWindow, glContext);

    m_spGeometry.reset();

    m_spImGuiDraw->Shutdown();
    m_spImGuiDraw.reset();

    DestroyDeviceMeshes();

    glDeleteTextures(1, &BackBufferTextureID);

    glDeleteVertexArrays(1, &VertexArrayID);
    glDeleteProgram(programID);

    for (auto& qd : m_mapIDToTextureData)
    {
        glDeleteTextures(1, &qd.first);
    }
    m_mapIDToTextureData.clear();

    // Cleanup
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(pSDLWindow);
}

// Prepare the device for doing 2D Rendering using ImGUI
void DeviceGL::BeginGUI()
{
    SDL_GL_MakeCurrent(pSDLWindow, glContext);
    m_spImGuiDraw->NewFrame(pSDLWindow);

    glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
}

void DeviceGL::EndGUI()
{
    ImGui::Render();
    m_spImGuiDraw->RenderDrawLists(ImGui::GetDrawData());
}

// Handle any interesting SDL events
void DeviceGL::ProcessEvent(SDL_Event& event)
{
    ImGui_SDL_Common::ProcessEvent(&event);
}

// Complete all rendering
void DeviceGL::Flush()
{
    /* Nothing to do */
}

// Copy the back buffer to the screen
void DeviceGL::Swap()
{
    m_inFrame = false;

    SDL_GL_MakeCurrent(pSDLWindow, glContext);
    SDL_GL_SwapWindow(pSDLWindow);
}

} // namespace Mgfx
