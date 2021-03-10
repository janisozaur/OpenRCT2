/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_OPENGL

#    include "OpenGLShaderProgram.h"

#    include <openrct2/Context.h>
#    include <openrct2/PlatformEnvironment.h>
#    include <openrct2/core/Console.hpp>
#    include <openrct2/core/FileStream.h>
#    include <openrct2/core/Path.hpp>
#    include <openrct2/core/String.hpp>

using namespace OpenRCT2;

OpenGLShader::OpenGLShader(const char* name, GLenum type)
{
    _type = type;

    Console::WriteLine("OpenGLShader::OpenGLShader %s, type %d", name, type);
    CheckGLError("gls1");
    auto path = GetPath(name);
    auto sourceCode = ReadSourceCode(path);
    auto sourceCodeStr = sourceCode.c_str();
    Console::WriteLine("OpenGLShader source: >>>\n%s\n<<<", sourceCodeStr);

    _id = glCreateShader(type);
    glShaderSource(_id, 1, static_cast<const GLchar**>(&sourceCodeStr), nullptr);
    glCompileShader(_id);
    Console::WriteLine("OpenGLShader %s received id = %u", name, _id);
    CheckGLError("gls2");

    GLint status;
    glGetShaderiv(_id, GL_COMPILE_STATUS, &status);
    CheckGLError("gls3");
    if (status != GL_TRUE)
    {
        Console::WriteLine("OpenGLShader %s miscompiled", name);
        char buffer[512];
        glGetShaderInfoLog(_id, sizeof(buffer), nullptr, buffer);
        glDeleteShader(_id);

        Console::Error::WriteLine("Error compiling %s", path.c_str());
        Console::Error::WriteLine(buffer);

        throw std::runtime_error("Error compiling shader.");
    }
    Console::WriteLine("OpenGLShader %s compiled fine", name);
    CheckGLError("gls4");
}

OpenGLShader::~OpenGLShader()
{
    glDeleteShader(_id);
}

GLuint OpenGLShader::GetShaderId()
{
    return _id;
}

std::string OpenGLShader::GetPath(const std::string& name)
{
    auto env = GetContext()->GetPlatformEnvironment();
    auto shadersPath = env->GetDirectoryPath(DIRBASE::OPENRCT2, DIRID::SHADER);
    auto path = Path::Combine(shadersPath, name);
    if (_type == GL_VERTEX_SHADER)
    {
        path += ".vert";
    }
    else
    {
        path += ".frag";
    }
    return path;
}

std::string OpenGLShader::ReadSourceCode(const std::string& path)
{
    auto fs = FileStream(path, FILE_MODE_OPEN);

    uint64_t fileLength = fs.GetLength();
    if (fileLength > MaxSourceSize)
    {
        throw IOException("Shader source too large.");
    }

    auto fileData = std::string(static_cast<size_t>(fileLength) + 1, '\0');
    fs.Read(static_cast<void*>(fileData.data()), fileLength);
    return fileData;
}

OpenGLShaderProgram::OpenGLShaderProgram(const char* name)
{
    Console::WriteLine("OpenGLShaderProgram::OpenGLShaderProgram %s", name);
    CheckGLError("glsp1");
    _vertexShader = new OpenGLShader(name, GL_VERTEX_SHADER);
    _fragmentShader = new OpenGLShader(name, GL_FRAGMENT_SHADER);

    _id = glCreateProgram();
    glAttachShader(_id, _vertexShader->GetShaderId());
    glAttachShader(_id, _fragmentShader->GetShaderId());
    glBindFragDataLocation(_id, 0, "oColour");
    CheckGLError("glsp2");

    Console::WriteLine(
        "OpenGLShaderProgram::OpenGLShaderProgram id = %u, v = %u, f = %u", _id, _vertexShader->GetShaderId(),
        _fragmentShader->GetShaderId());

    if (!Link())
    {
        char buffer[512];
        GLsizei length;
        glGetProgramInfoLog(_id, sizeof(buffer), &length, buffer);

        Console::Error::WriteLine("Error linking %s", name);
        Console::Error::WriteLine(buffer);

        throw std::runtime_error("Failed to link OpenGL shader.");
    }
    Console::WriteLine("OpenGLShaderProgram::OpenGLShaderProgram id = %u linked correctly", _id);
}

OpenGLShaderProgram::~OpenGLShaderProgram()
{
    Console::WriteLine("OpenGLShaderProgram::~OpenGLShaderProgram id = %u", _id);

    if (_vertexShader != nullptr)
    {
        glDetachShader(_id, _vertexShader->GetShaderId());
        delete _vertexShader;
    }
    if (_fragmentShader != nullptr)
    {
        glDetachShader(_id, _fragmentShader->GetShaderId());
        delete _fragmentShader;
    }
    glDeleteProgram(_id);
}

GLuint OpenGLShaderProgram::GetAttributeLocation(const char* name)
{
    auto loc = glGetAttribLocation(_id, name);
    Console::WriteLine("OpenGLShaderProgram::GetAttributeLocation id = %u, name = %s, loc = %d", _id, name, loc);
    CheckGLError("glsp attr");
    return loc;
}

GLuint OpenGLShaderProgram::GetUniformLocation(const char* name)
{
    auto loc = glGetUniformLocation(_id, name);
    Console::WriteLine("OpenGLShaderProgram::GetUniformLocation id = %u, name = %s, loc = %d", _id, name, loc);
    CheckGLError("glsp uniform");
    return loc;
}

void OpenGLShaderProgram::Use()
{
    Console::WriteLine("OpenGLShaderProgram::Use");
    CheckGLError("glsp use");
    if (OpenGLState::CurrentProgram != _id)
    {
        OpenGLState::CurrentProgram = _id;

        Console::WriteLine("OpenGLShaderProgram::Use using = %d", _id);
        glUseProgram(_id);
        CheckGLError("glsp use 2");
    }
    else
    {
        Console::WriteLine("OpenGLShaderProgram::Use %d already current", _id);
    }
    Console::WriteLine("OpenGLShaderProgram::Use %d now in use", _id);
}

bool OpenGLShaderProgram::Link()
{
    CheckGLError("glsp link");
    glLinkProgram(_id);

    GLint linkStatus;
    glGetProgramiv(_id, GL_LINK_STATUS, &linkStatus);
    CheckGLError("glsp link 2");
    Console::WriteLine("OpenGLShaderProgram::Link status = %d", linkStatus);
    return linkStatus == GL_TRUE;
}

#endif /* DISABLE_OPENGL */
