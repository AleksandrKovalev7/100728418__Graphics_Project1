#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <direct.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <algorithm> // std::max

//--------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------
static void framebuffer_size_callback(GLFWwindow*, int w, int h);
static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void processInput(GLFWwindow* window);
static glm::vec3 screenToWorldRay(GLFWwindow* window, const glm::mat4& projection, const glm::mat4& view);
static bool raySphereIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& sphereCenter, float sphereRadius);

static std::string readTextFile(const char* path);
static GLuint compileShaderFromFile(GLenum type, const char* path);
static GLuint createProgram(const char* vsPath, const char* fsPath);

static GLuint loadTexture2D(const char* path);
static GLuint loadCubemap(const std::vector<std::string>& faces);

static std::vector<float> buildGridFloor(int halfSize, float cellSize, float y, float r, float g, float b);

 
static bool loadSwordToGPU(const char* path);
static void drawSword(GLuint program);
 
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

 
static float gLightRadius = 16.0f;   // bigger circle
static float gLightSpeed = 0.35f;   // smaller = slower
static float gLightHeight = 7.0f;

//----------------------------------------------------------
//  GLOBAL STATE (inputs / toggles / camera / scene objects)
//----------------------------------------------------------
// Coursour
static bool gCursorEnabled = false;
static bool gMouseLookEnabled = true;

//---------------------------
//  toggles
//---------------------------
static bool gObjectMode = false;
static bool gWireframe = false;
static bool gShowGrid = true;
static bool gUseBlinn = true;

//---------------------------
// Time
//---------------------------
static float gDeltaTime = 0.0f;
static float gLastFrame = 0.0f;

//---------------------------
// Camera state
//---------------------------
static glm::vec3 gCamPos(0.0f, 6.0f, 12.0f);
static glm::vec3 gCamFront(0.0f, 0.0f, -1.0f);
static glm::vec3 gCamUp(0.0f, 1.0f, 0.0f);

static float gYaw = -90.0f;
static float gPitch = -20.0f;
static float gFov = 60.0f;
static glm::mat4 gLastView(1.0f);
static glm::mat4 gLastProj(1.0f);

static bool  gFirstMouse = true;
static float gLastX = 400.0f, gLastY = 300.0f;

//---------------------------
// Sword transform state
//---------------------------
static glm::vec3 gSwordPos(0.0f, 0.05f, 0.0f);
static float gSwordYaw = 0.0f;
static float gSwordScale = 1.0f;
static bool gSwordSelected = false;
static float gSwordPickRadius = 3.0f; // tweak later

//---------------------------
// Collision params
//---------------------------
static float gCamRadius = 0.35f;
static float gSwordLocalRadius = 0.5f;
static glm::vec3 gSwordLocalCenter(0.0f);
static glm::vec3 gSwordLocalMin(0.0f);
static glm::vec3 gSwordLocalMax(0.0f);

//----------------------------------------------------------
//  MODEL LOADING (Assimp)
//----------------------------------------------------------
struct ModelVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ModelMeshGL {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    int indexCount = 0;
    GLuint diffuseTex = 0;
};

static std::vector<ModelMeshGL> gSwordMeshes;
static std::string gSwordDir;

static std::string getDirectory(const std::string& path)
{
    std::string p = path;
    for (char& c : p) if (c == '\\') c = '/';
    size_t slash = p.find_last_of('/');
    if (slash == std::string::npos) return ".";
    return p.substr(0, slash);
}
//----------------------------------------------------------
// Sword selection | sword center  
//----------------------------------------------------------
static void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{

    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

    glm::vec3 rayDir = screenToWorldRay(window, gLastProj, gLastView);
    glm::vec3 rayOrigin = gCamPos;

    glm::vec3 swordCenter = gSwordPos;
    float swordRadius = gSwordPickRadius * gSwordScale;

    if (raySphereIntersect(rayOrigin, rayDir, swordCenter, swordRadius)) {
        gSwordSelected = true;
        gObjectMode = true;    
    }
    else {
        gSwordSelected = false;
        gObjectMode = false;  
       
    }

   
}

static ModelMeshGL uploadModelMesh(const std::vector<ModelVertex>& verts,
    const std::vector<unsigned int>& indices)
{
    ModelMeshGL m;
    m.indexCount = (int)indices.size();

    glGenVertexArrays(1, &m.VAO);
    glGenBuffers(1, &m.VBO);
    glGenBuffers(1, &m.EBO);

    glBindVertexArray(m.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(ModelVertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // aPos (0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
    glEnableVertexAttribArray(0);

    // aNormal (1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));
    glEnableVertexAttribArray(1);

    // aUV (3)
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, uv));
    glEnableVertexAttribArray(3);

    // aColor (2) constant white
    glDisableVertexAttribArray(2);
    glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(0);
    return m;
}
//----------------------------------------------------------
// Load sword
//----------------------------------------------------------
static bool loadSwordToGPU(const char* path)
{
    gSwordMeshes.clear();

    glm::vec3 minV(1e9f);
    glm::vec3 maxV(-1e9f);

    std::string p = path;
    for (char& c : p) if (c == '\\') c = '/';
    gSwordDir = getDirectory(p);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        p,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs
    );

    if (!scene || !scene->mRootNode) {
        std::cerr << "Assimp failed: " << importer.GetErrorString() << "\n";
        return false;
    }

    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* mesh = scene->mMeshes[mi];

        std::vector<ModelVertex> verts;
        std::vector<unsigned int> indices;
        verts.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            ModelVertex v{};

            v.pos = glm::vec3(mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z);

            minV = glm::min(minV, v.pos);
            maxV = glm::max(maxV, v.pos);

            if (mesh->HasNormals())
                v.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            else
                v.normal = glm::vec3(0, 1, 0);

            if (mesh->mTextureCoords[0])
                v.uv = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            else
                v.uv = glm::vec2(0, 0);

            verts.push_back(v);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
                indices.push_back(face.mIndices[j]);
        }

        gSwordMeshes.push_back(uploadModelMesh(verts, indices));
    }

    gSwordLocalMin = minV;
    gSwordLocalMax = maxV;

    std::cout << "Sword uploaded. Meshes=" << gSwordMeshes.size() << "\n";

    return !gSwordMeshes.empty();
}

static void drawSword(GLuint program)
{
    if (gSwordSelected) glVertexAttrib3f(2, 1.0f, 0.2f, 0.2f);
    else               glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);

    GLint useTexLoc = glGetUniformLocation(program, "uUseTexture");
    GLint texLoc = glGetUniformLocation(program, "uTex");

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(texLoc, 0);
    if (gSwordSelected)
        glVertexAttrib3f(2, 1.0f, 1.0f, 0.2f);  
    else
        glVertexAttrib3f(2, 1.0f, 1.0f, 1.0f);  


    for (auto& m : gSwordMeshes)
    {
        if (m.diffuseTex != 0) {
            glBindTexture(GL_TEXTURE_2D, m.diffuseTex);
            glUniform1i(useTexLoc, 1);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1i(useTexLoc, 0);
        }

        glBindVertexArray(m.VAO);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}


//----------------------------------------------------------
//  SHADERS / FILE IO
//----------------------------------------------------------
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

//----------------------------------------------------------
//  TEXTURE LOADING
//----------------------------------------------------------
static GLuint loadCubemap(const std::vector<std::string>& faces)
{
    // order: +X, -X, +Y, -Y, +Z, -Z
    stbi_set_flip_vertically_on_load(false);

    GLuint texID = 0;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

    int w, h, channels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &channels, 0);
        if (!data) {
            std::cerr << "Cubemap failed to load: " << faces[i] << "\n";
            continue;
        }

        GLenum format = GL_RGB;
        if (channels == 1) format = GL_RED;
        else if (channels == 3) format = GL_RGB;
        else if (channels == 4) format = GL_RGBA;

        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data
        );

        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return texID;
}

static GLuint loadTexture2D(const char* path)
{
    stbi_set_flip_vertically_on_load(true);

    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

//----------------------------------------------------------
//  CALLBACKS
//----------------------------------------------------------
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}

static void mouse_callback(GLFWwindow*, double xpos, double ypos)
{
    if (gFirstMouse) {
        gLastX = (float)xpos;
        gLastY = (float)ypos;
        gFirstMouse = false;
    }

    float xoffset = (float)xpos - gLastX;
    float yoffset = gLastY - (float)ypos;
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

static void scroll_callback(GLFWwindow*, double, double yoffset)
{
    gFov -= (float)yoffset;
    if (gFov < 20.0f) gFov = 20.0f;
    if (gFov > 80.0f) gFov = 80.0f;
}

//----------------------------------------------------------
//  GRID
//----------------------------------------------------------
static std::vector<float> buildGridFloor(
    int halfSize, float cellSize, float y,
    float r, float g, float b
) {
    std::vector<float> v;
    v.reserve((2 * halfSize) * (2 * halfSize) * 6 * 11);

    const float nx = 0.0f, ny = 1.0f, nz = 0.0f;
    const float tile = 0.25f;

    auto pushVertex = [&](float x, float yy, float z, float u, float vv) {
        v.push_back(x);  v.push_back(yy); v.push_back(z);
        v.push_back(nx); v.push_back(ny); v.push_back(nz);
        v.push_back(r);  v.push_back(g);  v.push_back(b);
        v.push_back(u);  v.push_back(vv);
        };

    for (int iz = -halfSize; iz < halfSize; ++iz) {
        for (int ix = -halfSize; ix < halfSize; ++ix) {
            float x0 = ix * cellSize;
            float x1 = (ix + 1) * cellSize;
            float z0 = iz * cellSize;
            float z1 = (iz + 1) * cellSize;

            float u0 = (float)ix * tile;
            float u1 = (float)(ix + 1) * tile;
            float v0 = (float)iz * tile;
            float v1 = (float)(iz + 1) * tile;

            pushVertex(x0, y, z0, u0, v0);
            pushVertex(x1, y, z0, u1, v0);
            pushVertex(x1, y, z1, u1, v1);

            pushVertex(x0, y, z0, u0, v0);
            pushVertex(x1, y, z1, u1, v1);
            pushVertex(x0, y, z1, u0, v1);
        }
    }
    return v;
}

//----------------------------------------------------------
//  INPUT
//----------------------------------------------------------
static void processInput(GLFWwindow* window)
{
    static bool pWasDown = false;
    bool pDown = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pDown && !pWasDown) {
        gCursorEnabled = !gCursorEnabled;
        glfwSetInputMode(window, GLFW_CURSOR, gCursorEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        gFirstMouse = true; // prevent jump when enabling mouse look again
        std::cout << (gCursorEnabled ? "Cursor ON (picking)\n" : "Cursor OFF (camera look)\n");
    }
    pWasDown = pDown;

    static bool oWasDown = false;
    bool oDown = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    if (oDown && !oWasDown) {
        gObjectMode = !gObjectMode;
        std::cout << (gObjectMode ? "MODE: OBJECT | Press Q/E to rotate \n" : "MODE: CAMERA\n");
    }
    oWasDown = oDown;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float camSpeed = 8.0f * gDeltaTime;
    glm::vec3 right = glm::normalize(glm::cross(gCamFront, gCamUp));

    if (!gObjectMode) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gCamPos += camSpeed * gCamFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gCamPos -= camSpeed * gCamFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gCamPos -= right * camSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gCamPos += right * camSpeed;
    }
    else {
        float objMove = 4.0f * gDeltaTime;
        float objRot = 90.0f * gDeltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gSwordPos.z -= objMove;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gSwordPos.z += objMove;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gSwordPos.x -= objMove;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gSwordPos.x += objMove;

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gSwordYaw += objRot;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gSwordYaw -= objRot;
    }

    float floorY = 0.0f;
    float eyeHeight = 0.3f;
    if (gCamPos.y < floorY + eyeHeight) gCamPos.y = floorY + eyeHeight;

    static bool f1WasDown = false;
    bool f1Down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    if (f1Down && !f1WasDown) {
        gWireframe = !gWireframe;
        glPolygonMode(GL_FRONT_AND_BACK, gWireframe ? GL_LINE : GL_FILL);
    }
    f1WasDown = f1Down;

    static bool gWasDown = false;
    bool gDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (gDown && !gWasDown) gShowGrid = !gShowGrid;
    gWasDown = gDown;

    static bool bWasDown = false;
    bool bDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
    if (bDown && !bWasDown) gUseBlinn = !gUseBlinn;
    bWasDown = bDown;
}
static glm::vec3 screenToWorldRay(
    GLFWwindow* window,
    const glm::mat4& projection,
    const glm::mat4& view
) {
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    int w, h;
    glfwGetWindowSize(window, &w, &h);

    // Normalized Device Coordinates (-1..1)
    float x = (2.0f * (float)mx) / (float)w - 1.0f;
    float y = 1.0f - (2.0f * (float)my) / (float)h; // flip Y
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);

    // Eye space
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    // World space
    glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));
    return rayWorld;
}

static bool raySphereIntersect(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& sphereCenter,
    float sphereRadius
) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float b = glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    float h = b * b - c;
    return h >= 0.0f;
}

//==============================================================
//  MAIN
//==============================================================
int main()
{
    //----------------------------------------------------------
    // 1) Window + GL init
    //----------------------------------------------------------
    if (!glfwInit()) { std::cout << "Failed to init GLFW\n"; return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(800, 600, "CrimsonSword", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to init GLAD\n";
        glfwTerminate();
        return -1;
    }
    

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    //----------------------------------------------------------
    // 2) Programs
    //----------------------------------------------------------
    GLuint program = createProgram("shaders/vertex.glsl", "shaders/fragment.glsl");
    GLuint skyboxProgram = createProgram("shaders/skybox.vert", "shaders/skybox.frag");
    glUseProgram(skyboxProgram);
    GLint skyLoc = glGetUniformLocation(skyboxProgram, "skybox");
     
    glUniform1i(skyLoc, 0); // texture unit 0

    //----------------------------------------------------------
    // 3) Assets
    //----------------------------------------------------------
    
  
    //
    std::cout << "------------Menu------------" << gSwordMeshes.size() << "\n";
    std::cout << "press O, to toggle between camera and object (sword)" << gSwordMeshes.size() << "\n";
    std::cout << "press G to remmove floor" << gSwordMeshes.size() << "\n";
    std::cout << "press B to switch to BillPhong Lighting" << gSwordMeshes.size() << "\n";
    std::cout << "press F1 to  see wireframe" << gSwordMeshes.size() << "\n";
    std::cout << "----------------------------" << gSwordMeshes.size() << "\n";

    // texture loading
    
    bool swordOK = loadSwordToGPU("assets/models/myModel/sword.obj");
    if (!swordOK) std::cerr << "Sword load/upload failed.\n";
    GLuint floorTex = loadTexture2D("assets/textures/floor.jpg");
    if (floorTex == 0) std::cerr << "Texture failed to load.\n";
    GLuint swordTex = loadTexture2D("assets/textures/sword.png");
    if (swordTex == 0) std::cerr << "Sword texture failed to load.\n";
    //
    for (auto& m : gSwordMeshes) m.diffuseTex = swordTex;

    std::vector<std::string> faces = {
       "assets/skybox/right.png",
       "assets/skybox/left.png",
       "assets/skybox/top.png",
       "assets/skybox/bottom.png",
       "assets/skybox/front.png",
       "assets/skybox/back.png"
    };

    GLuint cubemapTex = loadCubemap(faces);
    if (cubemapTex == 0) {
        std::cerr << "Cubemap texture is 0 (failed). Skybox will be black.\n";
    }

    glUseProgram(skyboxProgram);
    glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 0);

    //----------------------------------------------------------
    // 4) Build Grid VAO/VBO
    //----------------------------------------------------------
    std::vector<float> verts = buildGridFloor(25, 1.0f, 0.0f, 0.6f, 0.6f, 0.65f);

    GLuint gridVAO = 0, gridVBO = 0;
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    //----------------------------------------------------------
    // 5) Build Skybox VAO/VBO  
    //----------------------------------------------------------
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    GLuint skyboxVAO = 0, skyboxVBO = 0;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    //----------------------------------------------------------
    // 6) Uniform locations for main program
    //----------------------------------------------------------
    glUseProgram(program);

    glUniform1f(glGetUniformLocation(program, "constantAtt"), 1.0f);
    glUniform1f(glGetUniformLocation(program, "linearAtt"), 0.09f);
    glUniform1f(glGetUniformLocation(program, "quadraticAtt"), 0.032f);

    int viewLoc = glGetUniformLocation(program, "view");
    int projLoc = glGetUniformLocation(program, "projection");
    int modelLoc = glGetUniformLocation(program, "model");
    int normalMatrixLoc = glGetUniformLocation(program, "normalMatrix");

    int viewPosLoc = glGetUniformLocation(program, "viewPos");
    int lightPosLoc = glGetUniformLocation(program, "lightPos");
    int lightColorLoc = glGetUniformLocation(program, "lightColor");

    int ambientLoc = glGetUniformLocation(program, "ambientStrength");
    int specStrLoc = glGetUniformLocation(program, "specStrength");
    int shininessLoc = glGetUniformLocation(program, "shininess");
    int useBlinnLoc = glGetUniformLocation(program, "useBlinnPhong");

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //----------------------------------------------------------
    // 7) Render loop
    //----------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Build view/proj
        glm::mat4 view = glm::lookAt(gCamPos, gCamPos + gCamFront, gCamUp);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        float aspect = (fbh == 0) ? 1.0f : (float)fbw / (float)fbh;

        glm::mat4 projection = glm::perspective(glm::radians(gFov), aspect, 0.1f, 500.0f);
        gLastView = view;
        gLastProj = projection;

        //----------------------------------------------------------
        // Draw: Sword + Grid (main shader)
        //----------------------------------------------------------
        glUseProgram(program);

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(gCamPos));

        // moving light
        
        float t = (float)glfwGetTime();
        float ang = t * gLightSpeed;

        glm::vec3 lightPos(
            gLightRadius* cos(ang),
            gLightHeight,
            gLightRadius* sin(ang)
        );

        glm::vec3 lightColor(1.9f, 1.4f, 1.2f);
        glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPos));
        glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

        glUniform1f(ambientLoc, 0.18f);
        if (gUseBlinn) {
            glUniform1f(specStrLoc, 2.0f);     
            glUniform1f(shininessLoc, 256.0f);  
        }
        else {
            glUniform1f(specStrLoc, 0.8f);     
            glUniform1f(shininessLoc, 16.0f); 
        }

        glUniform1i(useBlinnLoc, gUseBlinn ? 1 : 0);

        // sword model matrix
        glm::mat4 swordModel(1.0f);
        swordModel = glm::translate(swordModel, gSwordPos);
        swordModel = glm::rotate(swordModel, glm::radians(gSwordYaw), glm::vec3(0, 1, 0));
        swordModel = glm::scale(swordModel, glm::vec3(gSwordScale));
        glm::mat3 swordNormal = glm::transpose(glm::inverse(glm::mat3(swordModel)));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(swordModel));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(swordNormal));
        glUniform1i(glGetUniformLocation(program, "uSelected"), gSwordSelected ? 1 : 0);

        drawSword(program);
        glUniform1i(glGetUniformLocation(program, "uSelected"), 0);

        // grid
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTex);
        glUniform1i(glGetUniformLocation(program, "uTex"), 0);
        glUniform1i(glGetUniformLocation(program, "uUseTexture"), 1);

        glm::mat4 gridModel(1.0f);
        glm::mat3 gridNormal = glm::transpose(glm::inverse(glm::mat3(gridModel)));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(gridModel));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(gridNormal));

        if (gShowGrid) {
            glBindVertexArray(gridVAO);
            glDrawArrays(GL_TRIANGLES, 0, (int)(verts.size() / 11));
            glBindVertexArray(0);
        }

        //----------------------------------------------------------
  //  skybox  
  //----------------------------------------------------------
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glUseProgram(skyboxProgram);

    
        glm::mat4 skyView = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "view"), 1, GL_FALSE, glm::value_ptr(skyView));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
         
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTex);

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    //----------------------------------------------------------
    // Cleanup
    //----------------------------------------------------------
    glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &gridVAO);

    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteTextures(1, &cubemapTex);

    glDeleteProgram(skyboxProgram);
    glDeleteProgram(program);

    for (auto& m : gSwordMeshes) {
        if (m.EBO) glDeleteBuffers(1, &m.EBO);
        if (m.VBO) glDeleteBuffers(1, &m.VBO);
        if (m.VAO) glDeleteVertexArrays(1, &m.VAO);
    }
    gSwordMeshes.clear();

    glfwTerminate();
    return 0;
}
