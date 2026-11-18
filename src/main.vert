#version 120

uniform sampler1D transforms;
uniform int numTransforms;

attribute vec2 position;
attribute vec2 texCoords;
attribute float spriteNumber;

varying vec2 texCoordsOut;

// Given a world position (for this lab, bottom-left is (0, 0), top-right of window is (1280, 720)),
// convert it to be in [-1, 1]^2, where y = -1 is the bottom and x = -1 is the left side.
vec2 cameraPos(vec2 worldPos) {
    float x = (worldPos.x / 1280.0) * 2.0 - 1.0;
    float y = (worldPos.y / 720.0) * 2.0 - 1.0;
    return vec2(x, y);
}

void main() {
    // +0.5 to hit middle of texel
    float texelCoord = (spriteNumber + 0.5) / float(numTransforms);
    vec4 transform = texture1D(transforms, texelCoord);

    // xy is scale, zw is translation
    vec2 scale = transform.xy;
    vec2 translation = transform.zw;
    vec2 worldPos = position * scale + translation;

    gl_Position = vec4(cameraPos(worldPos), 0.0, 1.0);

    // v has to be flipped as sfml stores image top row first but uvs start from bottom
    texCoordsOut = vec2(texCoords.x, 1.0 - texCoords.y);
}
