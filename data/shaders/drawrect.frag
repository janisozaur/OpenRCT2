#version 330 core

// clang-format off
const int MASK_REMAP_COUNT          = 3;
const int FLAG_NO_TEXTURE           = (1 << 2);
const int FLAG_MASK                 = (1 << 3);
const int FLAG_CROSS_HATCH          = (1 << 4);
const int FLAG_TTF_TEXT             = (1 << 5);

uniform usampler2DArray uTexture;
uniform usampler2D      uPaletteTex;

uniform sampler2D       uPeelingTex;
uniform bool            uPeeling;

in vec4 gl_FragCoord;

in vec2                 vTexColourCoord;
in vec2                 vTexMaskCoord;
in vec2                 vPosition;
flat in int             fFlags;
flat in uint            fColour;
flat in vec3            fPalettes;

in vec3                 fPeelPos;
flat in int             fTexColourAtlas;
flat in int             fTexMaskAtlas;
// clang-format on

out uint oColour;

void main()
{
    if (uPeeling)
    {
        float peel = texture(uPeelingTex, fPeelPos.xy).r;
        if (peel == 0.0 || fPeelPos.z >= peel)
        {
            discard;
        }
    }

    uint texel;
    if ((fFlags & FLAG_NO_TEXTURE) == 0)
    {
        texel = texture(uTexture, vec3(vTexColourCoord, fTexColourAtlas)).r;
        if (texel == 0u)
        {
            discard;
        }
        if ((fFlags & FLAG_TTF_TEXT) == 0)
        {
            texel += fColour;
        }
        else
        {
            uint hint_thresh = uint(fFlags & 0xff00) >> 8;
            if (hint_thresh > 0u)
            {
                bool solidColor = texel > 180u;
                texel = (texel > hint_thresh) ? fColour : 0u;
                texel = texel << 8;
                if (solidColor)
                {
                    texel += 1u;
                }
            }
            else
            {
                texel = fColour;
            }
        }
    }
    else
    {
        texel = fColour;
    }

    int paletteCount = fFlags & MASK_REMAP_COUNT;
    if (paletteCount >= 3 && texel >= 0x2Eu && texel < 0x3Au)
    {
        texel = texture(uPaletteTex, vec2(texel + 0xC5u, fPalettes.z) / 256.0f).r;
    }
    else if (paletteCount >= 2 && texel >= 0xCAu && texel < 0xD6u)
    {
        texel = texture(uPaletteTex, vec2(texel + 0x29u, fPalettes.y) / 256.f).r;
    }
    else if (paletteCount >= 1)
    {
        texel = texture(uPaletteTex, vec2(texel, fPalettes.x) / 256.f).r;
    }

    if (texel == 0u)
    {
        discard;
    }

    if ((fFlags & FLAG_CROSS_HATCH) != 0)
    {
        if ((int(vPosition.x) + int(vPosition.y)) % 2 != 0)
        {
            discard;
        }
    }

    if ((fFlags & FLAG_MASK) != 0)
    {
        uint mask = texture(uTexture, vec3(vTexMaskCoord, fTexMaskAtlas)).r;
        if (mask == 0u)
        {
            discard;
        }
    }

    oColour = texel;
}
