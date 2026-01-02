//
//  core.h
//  FFMPEG OpenGL
//
//  Created by Dmitri Wamback on 2025-03-30.
//

#ifndef core_h
#define core_h

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>

}

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>

#include <GL/glew.h>
#include <glfw3.h>

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>

std::mutex frameMutex;
std::atomic<bool> isRunning(true);
std::atomic<double> audioClock(0.0);
uint8_t* frameData;

#include "render/shader.h"
#include "render/texture.h"
#include "render/video_player.h"

Texture texture;
Shader shader;
GLFWwindow* window;

bool firstFrame = false;

#include "object/plane.h"
#include "object/camera.h"

Plane       plane;

#include <OpenAL/al.h>
#include <OpenAL/alc.h>

VideoPlayer videoPlayer;

void initialize() {
    if (!glfwInit()) throw std::runtime_error("Couldn't initialize glfw");

#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window = glfwCreateWindow(1200, 800, "FFMPEG OpenGL", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    Camera::Initialize();

    glewExperimental = GL_TRUE;
    glewInit();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    const char* path = "/Users/dmitriwamback/Documents/Projects/FFMPEG OpenGL/FFMPEG OpenGL/test2.mp4";
    if (!videoPlayer.load(path)) {
        throw std::runtime_error("Failed to load video");
    }

    int width  = videoPlayer.getWidth();
    int height = videoPlayer.getHeight();

    texture = Texture::LoadTextureWithBytes(nullptr, width, height);
    videoPlayer.play();

    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    shader = Shader::Create("/Users/dmitriwamback/Documents/Projects/FFMPEG OpenGL/FFMPEG OpenGL/shader/main");

    plane = Plane::Create();

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.3f, 0.3f, 0.3f, 0.0f);

        glm::vec4 movement(0.0f);
        movement.z = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ?  0.05f : 0;
        movement.w = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ? -0.05f : 0;
        movement.x = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ?  0.05f : 0;
        movement.y = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? -0.05f : 0;

        float up   = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS ?  0.05f : 0;
        float down = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS ? -0.05f : 0;

        camera.Update(movement, up, down);

        shader.Use();
        shader.SetMatrix4("projection", camera.projection);
        shader.SetMatrix4("lookAt",      camera.lookAt);

        videoPlayer.updateTexture(texture);

        texture.Bind();
        plane.Render(shader);

        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    videoPlayer.stop();
}

#endif /* core_h */
