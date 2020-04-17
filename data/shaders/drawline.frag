#version 300 es

flat in uint fColour;

out uint oColour;

void main()
{
    oColour = fColour;
}
