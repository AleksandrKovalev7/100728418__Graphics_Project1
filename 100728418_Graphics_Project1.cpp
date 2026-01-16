#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
// --- Camera state ---
static glm::vec3 gCamPos(0.0f, 6.0f, 12.0f);
static glm::vec3 gCamFront(0.0f, 0.0f, -1.0f);
static glm::vec3 gCamUp(0.0f, 1.0f, 0.0f);

static float gYaw = -90.0f;   // looking toward -Z
static float gPitch = -20.0f; // slightly down toward floor
static float gFov = 60.0f;

static bool gFirstMouse = true;
static float gLastX = 400.0f, gLastY = 300.0f;

static float gDeltaTime = 0.0f;
static float gLastFrame = 0.0f;
static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse) {
        gLastX = (float)xpos;
        gLastY = (float)ypos;
        gFirstMouse = false;
    }

    float xoffset = (float)xpos - gLastX;
    float yoffset = gLastY - (float)ypos; // reversed: y goes down on screen
    gLastX = (float)xpos;
    gLastY = (float)ypos;

    float sensitivity = 0.10f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    gYaw += xoffset;
    gPitch += yoffset;

    if (gPitch > 89.0f) gPitch = 89.0f;
    if (gPitch < -89.0f) gPitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(gYaw)) * cos(glm::radians(gPitch));
    front.y = sin(glm::radians(gPitch));
    front.z = sin(glm::radians(gYaw)) * cos(glm::radians(gPitch));
    gCamFront = glm::normalize(front);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    gFov -= (float)yoffset;
    if (gFov < 20.0f) gFov = 20.0f;
    if (gFov > 80.0f) gFov = 80.0f;
}

// Builds a flat XZ grid made of triangles, centered at origin.
// Each cell becomes 2 triangles (6 vertices). Vertex format: pos(3) + color(3)
static std::vector<float> buildGridFloor(
    int halfSize,      // e.g. 50 -> grid spans [-50..50]
    float cellSize,    // e.g. 1.0
    float y,           // floor height, e.g. 0.0
    float r, float g, float b // color
) {
    std::vector<float> v;
    v.reserve((2 * halfSize) * (2 * halfSize) * 6 * 6);

    auto pushVertex = [&](float x, float yy, float z) {
        v.push_back(x);  v.push_back(yy); v.push_back(z);
        v.push_back(r);  v.push_back(g);  v.push_back(b);
        };

    // cells in range: [-halfSize..halfSize-1] in both x and z
    for (int iz = -halfSize; iz < halfSize; ++iz) {
        for (int ix = -halfSize; ix < halfSize; ++ix) {
            float x0 = ix * cellSize;
            float x1 = (ix + 1) * cellSize;
            float z0 = iz * cellSize;
            float z1 = (iz + 1) * cellSize;

            // Triangle 1: (x0,z0) (x1,z0) (x1,z1)
            pushVertex(x0, y, z0);
            pushVertex(x1, y, z0);
            pushVertex(x1, y, z1);

            // Triangle 2: (x0,z0) (x1,z1) (x0,z1)
            pushVertex(x0, y, z0);
            pushVertex(x1, y, z1);
            pushVertex(x0, y, z1);
        }
    }
    return v;
}

static void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

static std::string readTextFile(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static GLuint compileShaderFromFile(GLenum type, const char* path)
{
    std::string code = readTextFile(path);
    const char* src = code.c_str();

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compile error (" << path << "):\n" << log << "\n";
    }
    return shader;
}

static GLuint createProgram(const char* vsPath, const char* fsPath)
{
    GLuint vs = compileShaderFromFile(GL_VERTEX_SHADER, vsPath);
    GLuint fs = compileShaderFromFile(GL_FRAGMENT_SHADER, fsPath);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// Simple toggles
static bool gWireframe = false;
static bool gShowGrid = true;

static void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    float speed = 8.0f * gDeltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gCamPos += speed * gCamFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gCamPos -= speed * gCamFront;

    glm::vec3 right = glm::normalize(glm::cross(gCamFront, gCamUp));
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gCamPos -= right * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gCamPos += right * speed;

    // Toggle wireframe (F1)
    static bool f1WasDown = false;
    bool f1Down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    if (f1Down && !f1WasDown) {
        gWireframe = !gWireframe;
        glPolygonMode(GL_FRONT_AND_BACK, gWireframe ? GL_LINE : GL_FILL);
        std::cout << "Wireframe: " << (gWireframe ? "ON" : "OFF") << "\n";
    }
    f1WasDown = f1Down;

    // Toggle grid (G)
    static bool gWasDown = false;
    bool gDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gDown && !gWasDown) {
        gShowGrid = !gShowGrid;
        std::cout << "Show grid: " << (gShowGrid ? "YES" : "NO") << "\n";
    }
    gWasDown = gDown;
}

int main()
{
    if (!glfwInit()) { std::cout << "Failed to init GLFW\n"; return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // IMPORTANT: request a depth buffer
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Grid Floor", nullptr, nullptr);
    if (!window) { std::cout << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to init GLAD\n";
        glfwTerminate();
        return -1;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // NOTE: shader paths are relative to the working directory.
    // In Visual Studio, set: Project Properties -> Debugging -> Working Directory = $(ProjectDir)
    GLuint program = createProgram("shaders/vertex.glsl", "shaders/fragment.glsl");

    // Camera: closer + clearer than (0,25,25) for debugging
    glm::vec3 camPos(0.0f, 6.0f, 12.0f);
   

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    float aspect = (fbh == 0) ? 1.0f : (float)fbw / (float)fbh;



    // Build a slightly smaller grid first for visibility debugging
    std::vector<float> verts = buildGridFloor(
        25,     // halfSize
        1.0f,   // cell size
        0.0f,   // y
        0.6f, 0.6f, 0.65f // brighter grey so it's not "same as clear"
    );

    GLuint VAO = 0, VBO = 0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    // pos
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Cache uniform locations
    glUseProgram(program);
    int viewLoc = glGetUniformLocation(program, "view");
    int projLoc = glGetUniformLocation(program, "projection");
    if (viewLoc < 0) std::cerr << "Uniform 'view' not found (check shader)\n";
    if (projLoc < 0) std::cerr << "Uniform 'projection' not found (check shader)\n";
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // capture mouse

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        processInput(window);

        // Clear both color and depth
        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        // build view/projection from the CURRENT camera state each frame
        glm::mat4 view = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        float aspect = (fbh == 0) ? 1.0f : (float)fbw / (float)fbh;

        glm::mat4 projection = glm::perspective(glm::radians(gFov), aspect, 0.1f, 500.0f);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));


        if (gShowGrid) {
            glBindVertexArray(VAO);
            int vertexCount = (int)(verts.size() / 6); // 6 floats per vertex
            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteProgram(program);

    glfwTerminate();
    return 0;
}
