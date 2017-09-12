#version 150

const int FLAG_COLOUR              = (1 << 0);
const int FLAG_REMAP               = (1 << 1);
const int FLAG_TRANSPARENT         = (1 << 2);
const int FLAG_TRANSPARENT_SPECIAL = (1 << 3);

uniform vec4            uPalette[256];
uniform usampler2DArray uTexture;
uniform usampler2DArray uDisplacementTexture;
uniform sampler3D       uLightmap;

flat in ivec4           fClip;
flat in int             fFlags;
in vec4                 fColour;
flat in int             fTexColourAtlas;
in vec2                 fTexColourCoords;
flat in int             fTexDisplacementAtlas;
in vec2                 fTexDisplacementCoords;
flat in int             fTexMaskAtlas;
in vec2                 fTexMaskCoords;
flat in int             fTexPaletteAtlas;
flat in vec4            fTexPaletteBounds;
flat in int             fMask;
flat in vec4            fWorldIn;

in vec2 fPosition;
in vec2 fTextureCoordinate;

out vec4 oColour;

void main()
{
    if (fPosition.x < fClip.x || fPosition.x > fClip.z ||
        fPosition.y < fClip.y || fPosition.y > fClip.w)
    {
        discard;
    }

    vec4 texel;

    vec3 worldPos = fWorldIn.xyz;
    float prelight = fWorldIn.w;

    // reads uvec!
    vec3 worldOffset = vec3(texture(uDisplacementTexture, vec3(fTexDisplacementCoords, float(fTexDisplacementAtlas))).xyz) / vec3(64.0, 64.0, 8.0);
    worldPos += worldOffset;
    
    vec3 lmPos = worldPos * vec3(2.0, 2.0, 1.0);
    vec3 sampleVec = lmPos / vec3(512.0, 512.0, 128.0);
    vec3 lightValue = texture(uLightmap, sampleVec).rgb;
    vec4 lmmultint = mix(vec4(lightValue * 5.0 + 0.2, 1), vec4(1, 1, 1, 1), prelight);
    vec4 lmaddint = mix(vec4(max(lightValue - 0.25, 0) * 2.0, 0), vec4(0, 0, 0, 0), prelight);

    // If remap palette used
    if ((fFlags & FLAG_REMAP) != 0)
    {
        // z is the size of each x pixel in the atlas
        float x = fTexPaletteBounds.x + texture(uTexture, vec3(fTexColourCoords, float(fTexColourAtlas))).r * fTexPaletteBounds.z;
        texel = uPalette[texture(uTexture, vec3(x, fTexPaletteBounds.y, float(fTexPaletteAtlas))).r];
    } // If transparent or special transparent
    else if ((fFlags & (FLAG_TRANSPARENT | FLAG_TRANSPARENT_SPECIAL)) != 0)
    {
        float line = texture(uTexture,vec3(fTexColourCoords, float(fTexColourAtlas))).r;
        if (line == 0.0)
        {
            discard;
        }
        float alpha = 0.5;
        if ((fFlags & FLAG_TRANSPARENT_SPECIAL) != 0)
        {
            alpha = 0.5 + (line - 1.0) / 10.0;
        }
    
        // z is the size of each x pixel in the atlas
        float x = fTexPaletteBounds.x + fTexPaletteBounds.z * 50.0;
        oColour = vec4(uPalette[texture(uTexture, vec3(x, fTexPaletteBounds.y, float(fTexPaletteAtlas))).r].rgb, alpha) * lmmultint + lmaddint;
        
        return;
    }
    else
    {
        texel = uPalette[texture(uTexture, vec3(fTexColourCoords, float(fTexColourAtlas))).r];
    }

    if (fMask != 0)
    {
        float mask = texture(uTexture, vec3(fTexMaskCoords, float(fTexMaskAtlas))).r;
        if ( mask == 0.0 )
        {
            discard;
        }

        oColour = texel * lmmultint + lmaddint;
    }
    else
    {
        if ((fFlags & FLAG_COLOUR) != 0)
        {
            oColour = vec4(fColour.rgb, fColour.a * texel.a) * lmmultint + lmaddint;
        }
        else
        {
            oColour = texel * lmmultint + lmaddint;
        }
    }
}
