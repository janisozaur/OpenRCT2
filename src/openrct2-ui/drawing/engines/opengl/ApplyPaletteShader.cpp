/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_OPENGL

#    include "ApplyPaletteShader.h"

#    include <openrct2/core/Console.hpp>

namespace
{
    struct VDStruct
    {
        GLfloat position[2];
        GLfloat texturecoordinate[2];
    };
} // namespace

constexpr VDStruct VertexData[4] = {
    { -1.0f, -1.0f, 0.0f, 0.0f },
    { 1.0f, -1.0f, 1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f, 1.0f },
    { 1.0f, 1.0f, 1.0f, 1.0f },
};

ApplyPaletteShader::ApplyPaletteShader()
    : OpenGLShaderProgram("applypalette")
{
    Console::WriteLine("ApplyPaletteShader::ApplyPaletteShader start");
    CheckGLError("apply shader 1");

    GetLocations();
    CheckGLError("apply shader 2");

    glGenBuffers(1, &_vbo);
    CheckGLError("apply shader 3");
    glGenVertexArrays(1, &_vao);
    CheckGLError("apply shader 4");

    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    CheckGLError("apply shader 5");
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexData), VertexData, GL_STATIC_DRAW);
    CheckGLError("apply shader 6");

    glBindVertexArray(_vao);
    CheckGLError("apply shader 7");
    glVertexAttribPointer(
        vPosition, 2, GL_FLOAT, GL_FALSE, sizeof(VDStruct), reinterpret_cast<void*>(offsetof(VDStruct, position)));
    glVertexAttribPointer(
        vTextureCoordinate, 2, GL_FLOAT, GL_FALSE, sizeof(VDStruct),
        reinterpret_cast<void*>(offsetof(VDStruct, texturecoordinate)));
    CheckGLError("apply shader 8");

    glEnableVertexAttribArray(vPosition);
    CheckGLError("apply shader 9");
    glEnableVertexAttribArray(vTextureCoordinate);
    CheckGLError("apply shader 10");

    Use();

    CheckGLError("apply shader 11");
    glUniform1i(uPaletteTexture, 0);
    CheckGLError("apply shader 12");
    Console::WriteLine("ApplyPaletteShader::ApplyPaletteShader done");
}

ApplyPaletteShader::~ApplyPaletteShader()
{
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

void ApplyPaletteShader::GetLocations()
{
    uPaletteTexture = GetUniformLocation("uPaletteTexture");
    uPalette = GetUniformLocation("uPalette");

    vPosition = GetAttributeLocation("vPosition");
    vTextureCoordinate = GetAttributeLocation("vTextureCoordinate");
}

void ApplyPaletteShader::SetTexture(GLuint texture)
{
    Console::WriteLine("ApplyPaletteShader::SetTexture start");
    OpenGLAPI::SetTexture(0, GL_TEXTURE_2D, texture);
    CheckGLError("ApplyPaletteShader::SetTexture");
}

void ApplyPaletteShader::SetPalette(const vec4* glPalette)
{
    Console::WriteLine("ApplyPaletteShader::SetPalette start");
    glUniform4fv(uPalette, 256, reinterpret_cast<const GLfloat*>(glPalette));
    CheckGLError("ApplyPaletteShader::SetPalette");
}

void ApplyPaletteShader::Draw()
{
    Console::WriteLine("ApplyPaletteShader::Draw start");
    CheckGLError("apply shader draw 1");
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CheckGLError("apply shader draw 2");
}

#endif /* DISABLE_OPENGL */
