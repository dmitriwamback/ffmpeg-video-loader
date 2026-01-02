//
//  texture.h
//  FFMPEG OpenGL
//
//  Created by Dmitri Wamback on 2025-03-30.
//

#ifndef texture_h
#define texture_h

class Texture {
public:
    uint32_t textureID;
    uint32_t pbo[2];
    int width, height;

    static Texture LoadTextureWithBytes(const void* bytes, int width, int height);
    void UpdateWithBytes(const void* bytes);
    void Bind();
    void Unbind();
};

Texture Texture::LoadTextureWithBytes(const void* bytes, int width, int height) {

    Texture texture = Texture();

    glGenTextures(1, &texture.textureID);
    glBindTexture(GL_TEXTURE_2D, texture.textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    texture.width = width;
    texture.height = height;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenBuffers(2, texture.pbo);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, texture.pbo[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 3, nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    if (bytes) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, bytes);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}

void Texture::UpdateWithBytes(const void* bytes) {

    static int index = 0;
    int next = (index + 1) % 2;

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[index]);
    uint8_t* pboMem = static_cast<uint8_t*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
    if (pboMem && bytes) {
        memcpy(pboMem, bytes, width * height * 3);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[next]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    width, height, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    index = next;
}

void Texture::Bind() {
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::Unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
}

#endif /* texture_h */
