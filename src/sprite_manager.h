#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include <cstddef>

#include "basic_transform.h"
#include "texture_atlas.h"
#include "vector_util.h"
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#define GL_SILENCE_DEPRECATION  // silences macOS OpenGL deprecation warnings

#define glCheck(expr)                                                                              \
    expr;                                                                                          \
    if (int e = glGetError()) {                                                                    \
        std::cerr << "OpenGL error: " << e << " in file " __FILE__ << ", line number " << __LINE__ \
                  << '\n';                                                                         \
        abort();                                                                                   \
    }

using SpriteID = size_t;

constexpr size_t VERTICES_PER_SPRITE = 6;
constexpr GLint ATLAS_TEXTURE_UNIT = 0;
constexpr GLint TRANSFORMS_TEXTURE_UNIT = 1;

class SpriteManager {
public:
    SpriteManager(const sf::Vector2u& atlasSize, uint padding)
        : mVertices(sf::PrimitiveType::Triangles),
          mNumSprites{0},
          mAtlas(atlasSize, padding),
          mShader(std::filesystem::path{"main.vert"}, std::filesystem::path{"main.frag"}),
          mSfmlDrawShader(std::filesystem::path{"main_sfml.vert"},
                          std::filesystem::path{"main.frag"}) {
        mSfmlDrawShader.setUniform("texture", sf::Shader::CurrentTexture);

        GLuint shaderProgram = mShader.getNativeHandle();

        glCheck(mPositionAttribLocation = glGetAttribLocation(shaderProgram, "position"));
        glCheck(mTexCoordsAttribLocation = glGetAttribLocation(shaderProgram, "texCoords"));
        glCheck(mSpriteNumberAttribLocation = glGetAttribLocation(shaderProgram, "spriteNumber"));

        glCheck(mTextureUniformLocation = glGetUniformLocation(shaderProgram, "texture"));
        glCheck(mTransformsUniformLocation = glGetUniformLocation(shaderProgram, "transforms"));
        glCheck(mNumTransformsUniformLocation =
                    glGetUniformLocation(shaderProgram, "numTransforms"));

        glCheck(glGenBuffers(1, &mVerticesVBO));
        glCheck(glGenBuffers(1, &mSpriteNumberVBO));

        // using nearest so we get the exact numbers back
        // (default filter needs mipmaps which we dont have)
        glCheck(glGenTextures(1, &mTransformsTexture));
        glCheck(glBindTexture(GL_TEXTURE_1D, mTransformsTexture));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        glCheck(glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        glCheck(glBindTexture(GL_TEXTURE_1D, 0));
    }

    ~SpriteManager() {
        glDeleteBuffers(1, &mVerticesVBO);
        glDeleteBuffers(1, &mSpriteNumberVBO);
        glDeleteTextures(1, &mTransformsTexture);
    }

    // dont allow copies or both of them would end up deleting the same gl buffers
    SpriteManager(const SpriteManager&) = delete;
    SpriteManager& operator=(const SpriteManager&) = delete;

    TextureID registerTexture(const std::string& filepath) {
        return mAtlas.registerTexture(filepath);
    }

    SpriteID createSprite(TextureID textureID) {
        SpriteID newID = mNumSprites;
        mNumSprites++;

        mVertices.resize(mNumSprites * VERTICES_PER_SPRITE);
        updateSpriteQuad(newID, textureID);

        mTransforms.push_back(BasicTransform{});

        for (size_t i = 0; i < VERTICES_PER_SPRITE; i++) {
            mSpriteNumbers.push_back(static_cast<float>(newID));
        }

        return newID;
    }

    void setTexture(SpriteID sprite, TextureID newTexture) {
        assert(sprite < mNumSprites);
        updateSpriteQuad(sprite, newTexture);
    }

    void draw(sf::RenderTarget& target, sf::RenderStates states) const {
        sf::VertexArray tempVerts = mVertices;

        for (size_t i = 0; i < tempVerts.getVertexCount(); i++) {
            size_t spriteIdx = i / VERTICES_PER_SPRITE;
            tempVerts[i].position = mTransforms[spriteIdx].applyPoint(mVertices[i].position);
        }

        states.texture = &mAtlas.getTexture();
        states.shader = &mSfmlDrawShader;
        target.draw(tempVerts, states);
    }

    void drawGL() {
        if (mNumSprites == 0) {
            return;
        }

        glCheck(glUseProgram(mShader.getNativeHandle()));

        GLuint posLoc = static_cast<GLuint>(mPositionAttribLocation);
        GLuint uvLoc = static_cast<GLuint>(mTexCoordsAttribLocation);
        GLuint spriteNumLoc = static_cast<GLuint>(mSpriteNumberAttribLocation);
        GLsizei numVerts = static_cast<GLsizei>(mVertices.getVertexCount());
        GLsizeiptr vertexBytes =
            static_cast<GLsizeiptr>(mVertices.getVertexCount() * sizeof(sf::Vertex));
        GLsizeiptr spriteNumberBytes =
            static_cast<GLsizeiptr>(mSpriteNumbers.size() * sizeof(float));

        // sf::Vertex also has a color in it so the stride/offset are to skip over that
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mVerticesVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER, vertexBytes, &mVertices[0], GL_DYNAMIC_DRAW));
        glCheck(glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex),
                                      reinterpret_cast<void*>(offsetof(sf::Vertex, position))));
        glCheck(glEnableVertexAttribArray(posLoc));
        glCheck(glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(sf::Vertex),
                                      reinterpret_cast<void*>(offsetof(sf::Vertex, texCoords))));
        glCheck(glEnableVertexAttribArray(uvLoc));

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, mSpriteNumberVBO));
        glCheck(glBufferData(GL_ARRAY_BUFFER, spriteNumberBytes, mSpriteNumbers.data(),
                             GL_DYNAMIC_DRAW));
        glCheck(glVertexAttribPointer(spriteNumLoc, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr));
        glCheck(glEnableVertexAttribArray(spriteNumLoc));

        // a BasicTransform is just 4 floats so it fits in one RGBA texel
        glCheck(glActiveTexture(GL_TEXTURE0 + TRANSFORMS_TEXTURE_UNIT));
        glCheck(glBindTexture(GL_TEXTURE_1D, mTransformsTexture));
        glCheck(glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F_ARB, static_cast<GLsizei>(mNumSprites), 0,
                             GL_RGBA, GL_FLOAT, mTransforms.data()));
        glCheck(glUniform1i(mTransformsUniformLocation, TRANSFORMS_TEXTURE_UNIT));

        glCheck(glActiveTexture(GL_TEXTURE0 + ATLAS_TEXTURE_UNIT));
        glCheck(glBindTexture(GL_TEXTURE_2D, mAtlas.getTexture().getNativeHandle()));
        glCheck(glUniform1i(mTextureUniformLocation, ATLAS_TEXTURE_UNIT));

        glCheck(glUniform1i(mNumTransformsUniformLocation, static_cast<GLint>(mNumSprites)));

        glCheck(glDrawArrays(GL_TRIANGLES, 0, numVerts));

        glCheck(glDisableVertexAttribArray(posLoc));
        glCheck(glDisableVertexAttribArray(uvLoc));
        glCheck(glDisableVertexAttribArray(spriteNumLoc));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
        glCheck(glUseProgram(0));
    }

    BasicTransform& getTransform(SpriteID id) {
        assert(id < mNumSprites);
        return mTransforms[id];
    }

    const TextureAtlas& getAtlas() const { return mAtlas; }

private:
    void updateSpriteQuad(SpriteID sprite, TextureID textureID) {
        sf::Vector2f texSize(mAtlas.getSizePixels(textureID));
        sf::FloatRect uvRect = mAtlas.getBoundsUV(textureID);

        float width = texSize.x;
        float height = texSize.y;

        float left = uvRect.position.x;
        float right = uvRect.position.x + uvRect.size.x;
        float bottom = uvRect.position.y;
        float top = uvRect.position.y + uvRect.size.y;

        size_t startIdx = sprite * VERTICES_PER_SPRITE;

        // 1st triangle bl,br,tr
        mVertices[startIdx + 0].position = {0.f, 0.f};
        mVertices[startIdx + 0].texCoords = {left, bottom};
        mVertices[startIdx + 1].position = {width, 0.f};
        mVertices[startIdx + 1].texCoords = {right, bottom};
        mVertices[startIdx + 2].position = {width, height};
        mVertices[startIdx + 2].texCoords = {right, top};

        // 2nd triangle : bl, tr, tl
        mVertices[startIdx + 3].position = {0.f, 0.f};
        mVertices[startIdx + 3].texCoords = {left, bottom};
        mVertices[startIdx + 4].position = {width, height};
        mVertices[startIdx + 4].texCoords = {right, top};
        mVertices[startIdx + 5].position = {0.f, height};
        mVertices[startIdx + 5].texCoords = {left, top};
    }

    sf::VertexArray mVertices;
    std::vector<BasicTransform> mTransforms;
    size_t mNumSprites;
    TextureAtlas mAtlas;
    mutable sf::Shader mShader;

    // window.draw doesnt fill attr so draw cant use main.vert, it has own shader
    mutable sf::Shader mSfmlDrawShader;

    // Use the below only in Batched rendering (part 2):
    std::vector<float> mSpriteNumbers;

    GLint mTextureUniformLocation;
    GLint mTransformsUniformLocation;
    GLint mNumTransformsUniformLocation;

    GLint mPositionAttribLocation;
    GLint mTexCoordsAttribLocation;
    GLint mSpriteNumberAttribLocation;

    GLuint mVerticesVBO;
    GLuint mSpriteNumberVBO;
    GLuint mTransformsTexture;
};

#endif  // SPRITE_MANAGER_H
