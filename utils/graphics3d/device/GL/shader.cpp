#include "mcommon.h"

#include "ui/ui_manager.h"

#include "deviceGL.h"
#include "shader.h"

namespace Mgfx
{

uint32_t LoadShaders(const fs::path& vertex_file_path, const fs::path& fragment_file_path)
{
    // Create the shaders
    auto VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
    auto FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

    // Read the Vertex Shader code from the file
    auto VertexShaderCode = file_read(vertex_file_path);
    if (VertexShaderCode.empty())
    {
        UIManager::Instance().AddMessage(MessageType::Error | MessageType::System, std::string("Couldn't load shader: ") + vertex_file_path.string());
        LOG(ERROR) << "No shader: " << vertex_file_path;
        return 0;
    }

    auto FragmentShaderCode = file_read(fragment_file_path);
    if (FragmentShaderCode.empty())
    {
        UIManager::Instance().AddMessage(MessageType::Error | MessageType::System, std::string("Couldn't load shader: ") + fragment_file_path.string());
        LOG(ERROR) << "No shader: " << fragment_file_path;
        return 0;
    }

    GLint Result = GL_FALSE;
    int InfoLogLength = 0;

    // Compile Vertex Shader
    LOG(INFO) << "Compiling shader : " << vertex_file_path;

    char const * VertexSourcePointer = VertexShaderCode.c_str();
    CHECK_GL(glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL));
    CHECK_GL(glCompileShader(VertexShaderID));

    // Check Vertex Shader
    CHECK_GL(glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result));
    CHECK_GL(glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength));

    if (InfoLogLength > 0)
    {
        std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
        glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
        LOG(ERROR) << &VertexShaderErrorMessage[0];
    }

    // Compile Fragment Shader
    LOG(INFO) << "Compiling shader : " << fragment_file_path;

    char const * FragmentSourcePointer = FragmentShaderCode.c_str();
    CHECK_GL(glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL));
    CHECK_GL(glCompileShader(FragmentShaderID));

    // Check Fragment Shader
    CHECK_GL(glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result));
    CHECK_GL(glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength));

    if (InfoLogLength > 0)
    {
        std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
        CHECK_GL(glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]));
        LOG(ERROR) << &FragmentShaderErrorMessage[0];
    }

    // Link the program
    LOG(INFO) << "Linking program";
    GLuint ProgramID = glCreateProgram();
    CHECK_GL(glAttachShader(ProgramID, VertexShaderID));
    CHECK_GL(glAttachShader(ProgramID, FragmentShaderID));
    CHECK_GL(glLinkProgram(ProgramID));

    // Check the program
    CHECK_GL(glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result));
    CHECK_GL(glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength));

    if (InfoLogLength > 0)
    {
        std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
        glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
        LOG(ERROR) << &ProgramErrorMessage[0];
    }

    CHECK_GL(glDetachShader(ProgramID, VertexShaderID));
    CHECK_GL(glDetachShader(ProgramID, FragmentShaderID));

    CHECK_GL(glDeleteShader(VertexShaderID));
    CHECK_GL(glDeleteShader(FragmentShaderID));

    return ProgramID;
}

} // namespace Mgfx
