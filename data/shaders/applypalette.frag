#version 300 es

precision mediump float;
precision mediump usampler2D;

uniform vec4 uPalette[256];
uniform usampler2D uPaletteTexture;

in vec2 fTextureCoordinate;

out vec4 oColour;

void main()
{
    oColour = uPalette[texture(uPaletteTexture, fTextureCoordinate).r];
}
