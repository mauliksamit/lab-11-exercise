#version 120

varying vec2 texCoordsOut;

// Given a world position (for this lab, bottom-left is (0, 0), top-right of window is (1280, 720)),
// convert it to be in [-1, 1]^2, where y = -1 is the bottom and x = -1 is the left side.
vec2 cameraPos(vec2 worldPos) {
    float x = (worldPos.x / 1280.0) * 2.0 - 1.0;
    float y = (worldPos.y / 720.0) * 2.0 - 1.0;
    return vec2(x, y);
}

void main() {
    // output to gl_Position and to texCoordsOut.
    // In this case, using window.draw, SFML provides tex coords in gl_MultiTexCoord0.xy, and
    //  vertex positions in gl_Vertex.xy.
    gl_Position = vec4(cameraPos(gl_Vertex.xy), 0.0, 1.0);

    // v has to be flipped as sfml stores image top row first but uvs start from bottom
    vec2 uv = gl_MultiTexCoord0.xy;
    texCoordsOut = vec2(uv.x, 1.0 - uv.y);
}
