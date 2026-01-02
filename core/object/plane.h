//
//  plane.h
//  FFMPEG OpenGL
//
//  Created by Dmitri Wamback on 2025-03-30.
//

#ifndef plane_h
#define plane_h

class Plane {
public:
    std::vector<float> vertices;
    uint32_t vao, vbo;
    glm::vec3 position, scale, rotation, color;
    
    static Plane Create();
    void Render(Shader shader);
};

Plane Plane::Create() {
    Plane plane = Plane();
    
    plane.vertices = {
       -0.5f, 0.0f,-0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
       -0.5f, 0.0f, 0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
        0.5f, 0.0f, 0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        
       -0.5f, 0.0f,-0.5f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        0.5f, 0.0f, 0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        0.5f, 0.0f,-0.5f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
    };
    
    plane.position = glm::vec3(0.0f, 4.0f, -25.0f);
    plane.rotation = glm::vec3(90.0f, 0.0f, 0.0f);
    plane.scale = glm::vec3(9.0f * 10.0f, 6.0f * 10.0f, 6.0f * 10.0f);
    
    plane.color = glm::vec3(1.0f);
    
    glGenVertexArrays(1, &plane.vao);
    glBindVertexArray(plane.vao);
    
    glGenBuffers(1, &plane.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, plane.vbo);
    glBufferData(GL_ARRAY_BUFFER, plane.vertices.size() * sizeof(float), &plane.vertices[0], GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    
    return plane;
}

void Plane::Render(Shader shader) {
    
    shader.Use();
    
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 translationMatrix = glm::mat4(1.0f);
    translationMatrix = glm::translate(translationMatrix, position);
    
    glm::mat4 scaleMatrix = glm::mat4(1.0f);
    scaleMatrix = glm::scale(scaleMatrix, scale);
    
    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0)) *
                               glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0)) *
                               glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1));
    
    model = translationMatrix * rotationMatrix * scaleMatrix;
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
    
    shader.SetMatrix4("model", model);
    shader.SetVector3("color", color);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
}

#endif /* plane_h */
