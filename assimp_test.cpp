#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdio.h>

int main() {
    Assimp::Importer importer;
    const char *path = "../sponza-scene/source/glTF/Sponza.gltf";
    unsigned flags = 0;
    flags |= aiProcess_Triangulate;
    flags |= aiProcess_PreTransformVertices;
    const aiScene *scene = importer.ReadFile(path, flags);
    if (!scene) {
        printf("Couldn't load %s!\n", path);
        return 1;
    }
    printf("Mesh count: %d\n", scene->mNumMeshes);
    printf("Material count: %d\n", scene->mNumMaterials);
    for (int i=0; i<scene->mNumMeshes; ++i) {
        printf("Mesh %d/%d\n", i, scene->mNumMeshes);
        aiMesh *mesh = scene->mMeshes[i];
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        aiString diffuseMapPath;
        aiString normalMapPath;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &diffuseMapPath);
        material->GetTexture(aiTextureType_NORMALS, 0, &normalMapPath);
        printf("Triangle count: %d\n", mesh->mNumFaces);
        printf("Diffuse map: %s\n", diffuseMapPath.C_Str());
        printf("Normal map: %s\n", normalMapPath.C_Str());
    }
}
