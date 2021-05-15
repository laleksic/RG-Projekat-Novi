#include <stdio.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glfw/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "stb_image.h"
#include <array>
#include <algorithm>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <iostream>
using namespace glm;
using namespace std;

class Texture;
class InputMaster;
class Model;
class Mesh;
class Shader;
class FPSCamera;
typedef shared_ptr<Texture> TexturePtr;
typedef shared_ptr<InputMaster> InputMasterPtr;
typedef shared_ptr<Model> ModelPtr;
typedef shared_ptr<Mesh> MeshPtr;
typedef shared_ptr<Shader> ShaderPtr;
typedef shared_ptr<FPSCamera> FPSCameraPtr;