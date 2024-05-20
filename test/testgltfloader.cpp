#include <vector>
#include <string>
#include <cstdio>
#include <array>
#include <optional>
#include <cassert>
#include <ios>

typedef unsigned int uint;
typedef unsigned short ushort;

#include "../src/engine/model/gltfloader.h"

constexpr float tolerance = 0.001;

void testmeshnames()
{
    std::printf("testing GLTF file mesh names\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);

    std::vector<std::string> names = mi.getnodenames(GLTFModelInfo::NodeType_Mesh);
    assert(names[0] == "Cube");
    names = mi.getnodenames(GLTFModelInfo::NodeType_All);
    assert(names[0] == "Bone.001");
    assert(names[1] == "Bone");
    assert(names[2] == "Cube");
    assert(names[3] == "Armature");
}

void testbounds()
{
    std::printf("testing GLTF file data bounds\n");

    std::string modelname = "gltf/box.gltf";

    GLTFModelInfo mi(modelname, true);

    //get GLTF data from file/binary
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<float, 3>> positions = mi.getpositions(meshname);
    std::vector<std::array<float, 3>> normals = mi.getnormals(meshname);
    std::vector<std::array<float, 2>> texcoords = mi.gettexcoords(meshname);
    std::vector<std::array<uint, 4>> joints = mi.getjoints(meshname);
    std::vector<std::array<float, 4>> weights = mi.getweights(meshname);
    std::vector<std::array<uint, 3>> indices = mi.getindices(meshname);

    std::printf("GLTF data size: %lu positions %lu normals %lu texcoords %lu joints %lu weights %lu indices\n",
        positions.size(), normals.size(), texcoords.size(), joints.size(), weights.size(), indices.size());

    assert(positions.size() == 80);
    assert(normals.size() == 80);
    assert(texcoords.size() == 80);
    assert(joints.size() == 80);
    assert(weights.size() == 80);
    assert(indices.size() == 68);
}

void testpositions()
{
    std::printf("testing GLTF file vertex positions loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);

    //get GLTF data from file/binary
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<float, 3>> positions = mi.getpositions(meshname);
    const std::vector<std::array<float, 3>> correctpositions =
    {
        { 1.000000, 1.000000, -1.000000 },{ 1.000000, 1.000000, -1.000000 },{ 1.000000, -1.000000, -1.000000 },
        { 1.000000, -1.000000, -1.000000 },{ 1.000000, -1.000000, -1.000000 },{ 1.000000, 1.000000, 1.000000 },
        { 1.000000, 1.000000, 1.000000 },{ 1.000000, -1.000000, 1.000000 },{ 1.000000, -1.000000, 1.000000 },
        { 1.000000, -1.000000, 1.000000 },{ -1.000000, 1.000000, -1.000000 },{ -1.000000, 1.000000, -1.000000 },
        { -1.000000, -1.000000, -1.000000 },{ -1.000000, -1.000000, -1.000000 },{ -1.000000, -1.000000, -1.000000 },
        { -1.000000, 1.000000, 1.000000 },{ -1.000000, 1.000000, 1.000000 },{ -1.000000, -1.000000, 1.000000 },
        { -1.000000, -1.000000, 1.000000 },{ -1.000000, -1.000000, 1.000000 },{ 1.000000, 2.000000, -1.000000 },
        { 1.000000, 2.000000, -1.000000 },{ 1.000000, 2.000000, 1.000000 },{ 1.000000, 2.000000, 1.000000 },
        { -1.000000, 2.000000, -1.000000 },{ -1.000000, 2.000000, -1.000000 },{ -1.000000, 2.000000, 1.000000 },
        { -1.000000, 2.000000, 1.000000 },{ 1.000000, 3.000000, -1.000000 },{ 1.000000, 3.000000, -1.000000 },
        { 1.000000, 3.000000, -1.000000 },{ 1.000000, 3.000000, 1.000000 },{ 1.000000, 3.000000, 1.000000 },
        { 1.000000, 3.000000, 1.000000 },{ -1.000000, 3.000000, -1.000000 },{ -1.000000, 3.000000, -1.000000 },
        { -1.000000, 3.000000, -1.000000 },{ -1.000000, 3.000000, 1.000000 },{ -1.000000, 3.000000, 1.000000 },
        { -1.000000, 3.000000, 1.000000 },{ 1.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -1.000000 },
        { -1.000000, 0.000000, 1.000000 },{ -1.000000, 0.000000, 1.000000 },{ 1.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, 1.000000 },{ -1.000000, 0.000000, -1.000000 },{ -1.000000, 0.000000, -1.000000 },
        { 1.000000, 2.500000, 1.000000 },{ 1.000000, 2.500000, 1.000000 },{ -1.000000, 2.500000, 1.000000 },
        { -1.000000, 2.500000, 1.000000 },{ -1.000000, 2.500000, -1.000000 },{ -1.000000, 2.500000, -1.000000 },
        { 1.000000, 2.500000, -1.000000 },{ 1.000000, 2.500000, -1.000000 },{ -1.000000, 1.500000, -1.000000 },
        { -1.000000, 1.500000, -1.000000 },{ 1.000000, 1.500000, -1.000000 },{ 1.000000, 1.500000, -1.000000 },
        { 1.000000, 1.500000, 1.000000 },{ 1.000000, 1.500000, 1.000000 },{ -1.000000, 1.500000, 1.000000 },
        { -1.000000, 1.500000, 1.000000 },{ -1.000000, 0.496070, 1.000000 },{ -1.000000, 0.496070, 1.000000 },
        { 1.000000, 0.496070, -1.000000 },{ 1.000000, 0.496070, -1.000000 },{ 1.000000, 0.496070, 1.000000 },
        { 1.000000, 0.496070, 1.000000 },{ -1.000000, 0.496070, -1.000000 },{ -1.000000, 0.496070, -1.000000 },
        { 1.000000, -0.500000, -1.000000 },{ 1.000000, -0.500000, -1.000000 },{ 1.000000, -0.500000, 1.000000 },
        { 1.000000, -0.500000, 1.000000 },{ -1.000000, -0.500000, -1.000000 },{ -1.000000, -0.500000, -1.000000 },
        { -1.000000, -0.500000, 1.000000 },{ -1.000000, -0.500000, 1.000000 }
    };

    for(uint i = 0; i < positions.size(); ++i)
    {
        assert(correctpositions[i][0] - positions[i][0] < tolerance);
        assert(correctpositions[i][1] - positions[i][1] < tolerance);
        assert(correctpositions[i][2] - positions[i][2] < tolerance);
    }
}

void testnormals()
{
    std::printf("testing GLTF file vertex normals loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<float, 3>> normals = mi.getnormals(meshname);

    const std::vector<std::array<float, 3>> correctnormals =
    {
        { 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },{ 0.000000, -1.000000, -0.000000 },
        { 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ 0.000000, -1.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { -1.000000, 0.000000, -0.000000 },{ 0.000000, -1.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },{ -1.000000, 0.000000, -0.000000 },
        { 0.000000, -1.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },{ 0.000000, 0.000000, -1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },{ 1.000000, 0.000000, -0.000000 },
        { -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },{ -1.000000, 0.000000, -0.000000 },
        { 0.000000, 0.000000, 1.000000 },{ 0.000000, 0.000000, -1.000000 },{ 0.000000, 1.000000, -0.000000 },
        { 1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },{ 0.000000, 1.000000, -0.000000 },
        { 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { 0.000000, 1.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 0.000000, 1.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },
        { -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { 0.000000, 0.000000, 1.000000 },{ 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },
        { 0.000000, 0.000000, 1.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },
        { 0.000000, 0.000000, -1.000000 },{ 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },
        { 0.000000, 0.000000, 1.000000 },{ 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },
        { 0.000000, 0.000000, 1.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { 0.000000, 0.000000, -1.000000 },{ 1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 },
        { 1.000000, 0.000000, -0.000000 },{ -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, -1.000000 },
        { -1.000000, 0.000000, -0.000000 },{ 0.000000, 0.000000, 1.000000 }
    };

    for(uint i = 0; i < normals.size(); ++i)
    {
        assert(correctnormals[i][0] - normals[i][0] < tolerance);
        assert(correctnormals[i][1] - normals[i][1] < tolerance);
        assert(correctnormals[i][2] - normals[i][2] < tolerance);
    }
}

void testtexcoords()
{
    std::printf("testing GLTF file texture coordinates loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<float, 2>> texcoords = mi.gettexcoords(meshname);

    const std::vector<std::array<float, 2>> correcttexcoords =
    {
        { 0.625000, 0.500000 },{ 0.625000, 0.500000 },{ 0.375000, 0.500000 },{ 0.375000, 0.500000 },{ 0.375000, 0.500000 },
        { 0.625000, 0.250000 },{ 0.625000, 0.250000 },{ 0.375000, 0.250000 },{ 0.375000, 0.250000 },{ 0.375000, 0.250000 },
        { 0.625000, 0.750000 },{ 0.625000, 0.750000 },{ 0.375000, 0.750000 },{ 0.125000, 0.500000 },{ 0.375000, 0.750000 },
        { 0.625000, 1.000000 },{ 0.625000, 0.000000 },{ 0.375000, 1.000000 },{ 0.125000, 0.250000 },{ 0.375000, 0.000000 },
        { 0.625000, 0.500000 },{ 0.625000, 0.500000 },{ 0.625000, 0.250000 },{ 0.625000, 0.250000 },{ 0.625000, 0.750000 },
        { 0.625000, 0.750000 },{ 0.625000, 1.000000 },{ 0.625000, 0.000000 },{ 0.625000, 0.500000 },{ 0.625000, 0.500000 },
        { 0.625000, 0.500000 },{ 0.625000, 0.250000 },{ 0.625000, 0.250000 },{ 0.625000, 0.250000 },{ 0.625000, 0.750000 },
        { 0.625000, 0.750000 },{ 0.875000, 0.500000 },{ 0.625000, 1.000000 },{ 0.625000, 0.000000 },{ 0.875000, 0.250000 },
        { 0.500000, 0.500000 },{ 0.500000, 0.500000 },{ 0.500000, 1.000000 },{ 0.500000, 0.000000 },{ 0.500000, 0.250000 },
        { 0.500000, 0.250000 },{ 0.500000, 0.750000 },{ 0.500000, 0.750000 },{ 0.625000, 0.250000 },{ 0.625000, 0.250000 },
        { 0.625000, 1.000000 },{ 0.625000, 0.000000 },{ 0.625000, 0.750000 },{ 0.625000, 0.750000 },{ 0.625000, 0.500000 },
        { 0.625000, 0.500000 },{ 0.625000, 0.750000 },{ 0.625000, 0.750000 },{ 0.625000, 0.500000 },{ 0.625000, 0.500000 },
        { 0.625000, 0.250000 },{ 0.625000, 0.250000 },{ 0.625000, 1.000000 },{ 0.625000, 0.000000 },{ 0.562009, 1.000000 },
        { 0.562009, 0.000000 },{ 0.562009, 0.500000 },{ 0.562009, 0.500000 },{ 0.562009, 0.250000 },{ 0.562009, 0.250000 },
        { 0.562009, 0.750000 },{ 0.562009, 0.750000 },{ 0.437500, 0.500000 },{ 0.437500, 0.500000 },{ 0.437500, 0.250000 },
        { 0.437500, 0.250000 },{ 0.437500, 0.750000 },{ 0.437500, 0.750000 },{ 0.437500, 1.000000 },{ 0.437500, 0.000000 }
    };

    for(uint i = 0; i < texcoords.size(); ++i)
    {
        assert(correcttexcoords[i][0] - texcoords[i][0] < tolerance);
        assert(correcttexcoords[i][1] - texcoords[i][1] < tolerance);
    }
}

void testjoints()
{
    std::printf("testing GLTF file joints data loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<uint, 4>> joints = mi.getjoints(meshname);

    const std::vector<std::array<uint, 4>> correctjoints =
    {
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },
        { 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },
        { 0, 1, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },
        { 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },{ 1, 0, 0, 0 },
        { 1, 0, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },
        { 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 },
        { 0, 1, 0, 0 },{ 0, 1, 0, 0 },{ 0, 1, 0, 0 }
    };

    for(uint i = 0; i < joints.size(); ++i)
    {
        assert(correctjoints[i][0] == joints[i][0]);
        assert(correctjoints[i][1] == joints[i][1]);
        assert(correctjoints[i][2] == joints[i][2]);
        assert(correctjoints[i][3] == joints[i][3]);
    }
}

void testweights()
{
    std::printf("testing GLTF file vertex weights loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<float, 4>> weights = mi.getweights(meshname);

    std::vector<std::array<float, 4>> correctweights =
    {
        { 0.500338, 0.499662, 0.000000, 0.000000 },{ 0.500338, 0.499662, 0.000000, 0.000000 },
        { 0.967030, 0.032970, 0.000000, 0.000000 },{ 0.967030, 0.032970, 0.000000, 0.000000 },
        { 0.967030, 0.032970, 0.000000, 0.000000 },{ 0.500479, 0.499521, 0.000000, 0.000000 },
        { 0.500479, 0.499521, 0.000000, 0.000000 },{ 0.962019, 0.037981, 0.000000, 0.000000 },
        { 0.962019, 0.037981, 0.000000, 0.000000 },{ 0.962019, 0.037981, 0.000000, 0.000000 },
        { 0.500479, 0.499521, 0.000000, 0.000000 },{ 0.500479, 0.499521, 0.000000, 0.000000 },
        { 0.962019, 0.037981, 0.000000, 0.000000 },{ 0.962019, 0.037981, 0.000000, 0.000000 },
        { 0.962019, 0.037981, 0.000000, 0.000000 },{ 0.500338, 0.499662, 0.000000, 0.000000 },
        { 0.500338, 0.499662, 0.000000, 0.000000 },{ 0.967030, 0.032970, 0.000000, 0.000000 },
        { 0.967030, 0.032970, 0.000000, 0.000000 },{ 0.967030, 0.032970, 0.000000, 0.000000 },
        { 0.872278, 0.127722, 0.000000, 0.000000 },{ 0.872278, 0.127722, 0.000000, 0.000000 },
        { 0.872757, 0.127243, 0.000000, 0.000000 },{ 0.872757, 0.127243, 0.000000, 0.000000 },
        { 0.872757, 0.127243, 0.000000, 0.000000 },{ 0.872757, 0.127243, 0.000000, 0.000000 },
        { 0.872278, 0.127722, 0.000000, 0.000000 },{ 0.872278, 0.127722, 0.000000, 0.000000 },
        { 0.962183, 0.037817, 0.000000, 0.000000 },{ 0.962183, 0.037817, 0.000000, 0.000000 },
        { 0.962183, 0.037817, 0.000000, 0.000000 },{ 0.967185, 0.032815, 0.000000, 0.000000 },
        { 0.967185, 0.032815, 0.000000, 0.000000 },{ 0.967185, 0.032815, 0.000000, 0.000000 },
        { 0.967185, 0.032815, 0.000000, 0.000000 },{ 0.967185, 0.032815, 0.000000, 0.000000 },
        { 0.967185, 0.032815, 0.000000, 0.000000 },{ 0.962183, 0.037817, 0.000000, 0.000000 },
        { 0.962183, 0.037817, 0.000000, 0.000000 },{ 0.962183, 0.037817, 0.000000, 0.000000 },
        { 0.872516, 0.127484, 0.000000, 0.000000 },{ 0.872516, 0.127484, 0.000000, 0.000000 },
        { 0.872516, 0.127484, 0.000000, 0.000000 },{ 0.872516, 0.127484, 0.000000, 0.000000 },
        { 0.872036, 0.127964, 0.000000, 0.000000 },{ 0.872036, 0.127964, 0.000000, 0.000000 },
        { 0.872036, 0.127964, 0.000000, 0.000000 },{ 0.872036, 0.127964, 0.000000, 0.000000 },
        { 0.932556, 0.067444, 0.000000, 0.000000 },{ 0.932556, 0.067444, 0.000000, 0.000000 },
        { 0.931463, 0.068537, 0.000000, 0.000000 },{ 0.931463, 0.068537, 0.000000, 0.000000 },
        { 0.932556, 0.067444, 0.000000, 0.000000 },{ 0.932556, 0.067444, 0.000000, 0.000000 },
        { 0.931463, 0.068537, 0.000000, 0.000000 },{ 0.931463, 0.068537, 0.000000, 0.000000 },
        { 0.749334, 0.250666, 0.000000, 0.000000 },{ 0.749334, 0.250666, 0.000000, 0.000000 },
        { 0.749108, 0.250892, 0.000000, 0.000000 },{ 0.749108, 0.250892, 0.000000, 0.000000 },
        { 0.749334, 0.250666, 0.000000, 0.000000 },{ 0.749334, 0.250666, 0.000000, 0.000000 },
        { 0.749108, 0.250892, 0.000000, 0.000000 },{ 0.749108, 0.250892, 0.000000, 0.000000 },
        { 0.750079, 0.249921, 0.000000, 0.000000 },{ 0.750079, 0.249921, 0.000000, 0.000000 },
        { 0.750079, 0.249921, 0.000000, 0.000000 },{ 0.750079, 0.249921, 0.000000, 0.000000 },
        { 0.749852, 0.250148, 0.000000, 0.000000 },{ 0.749852, 0.250148, 0.000000, 0.000000 },
        { 0.749852, 0.250148, 0.000000, 0.000000 },{ 0.749852, 0.250148, 0.000000, 0.000000 },
        { 0.932428, 0.067572, 0.000000, 0.000000 },{ 0.932428, 0.067572, 0.000000, 0.000000 },
        { 0.931333, 0.068667, 0.000000, 0.000000 },{ 0.931333, 0.068667, 0.000000, 0.000000 },
        { 0.931333, 0.068667, 0.000000, 0.000000 },{ 0.931333, 0.068667, 0.000000, 0.000000 },
        { 0.932428, 0.067572, 0.000000, 0.000000 },{ 0.932428, 0.067572, 0.000000, 0.000000 }
    };

    for(uint i = 0; i < weights.size(); ++i)
    {
        assert(correctweights[i][0] - weights[i][0] < tolerance);
        assert(correctweights[i][1] - weights[i][1] < tolerance);
        assert(correctweights[i][2] - weights[i][2] < tolerance);
        assert(correctweights[i][3] - weights[i][3] < tolerance);
    }
}

void testindices()
{
    std::printf("testing GLTF file vertex index loading\n");
    std::string modelname = "gltf/box.gltf";
    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];
    std::vector<std::array<uint, 3>> indices = mi.getindices(meshname);

    std::vector<std::array<uint, 3>> correctindices =
    {
        { 58, 57, 25},{ 58, 25, 20},{ 68, 5, 16},{ 68, 16, 65},{ 64, 15, 10},{ 64, 10, 70},{ 13, 2, 7},{ 13, 7, 18},
        { 67, 1, 6},{ 67, 6, 69},{ 71, 11, 0},{ 71, 0, 66},{ 51, 48, 31},{ 51, 31, 38},{ 63, 60, 22},{ 63, 22, 27},
        { 56, 62, 26},{ 56, 26, 24},{ 61, 59, 21},{ 61, 21, 23},{ 29, 36, 39},{ 29, 39, 32},{ 54, 53, 35},{ 54, 35, 28},
        { 49, 55, 30},{ 49, 30, 33},{ 52, 50, 37},{ 52, 37, 34},{ 77, 47, 40},{ 77, 40, 72},{ 73, 41, 45},{ 73, 45, 75},
        { 78, 42, 46},{ 78, 46, 76},{ 74, 44, 43},{ 74, 43, 79},{ 24, 26, 50},{ 24, 50, 52},{ 23, 21, 55},{ 23, 55, 49},
        { 20, 25, 53},{ 20, 53, 54},{ 27, 22, 48},{ 27, 48, 51},{ 6, 1, 59},{ 6, 59, 61},{ 10, 15, 62},{ 10, 62, 56},
        { 16, 5, 60},{ 16, 60, 63},{ 0, 11, 57},{ 0, 57, 58},{ 47, 71, 66},{ 47, 66, 40},{ 41, 67, 69},{ 41, 69, 45},
        { 42, 64, 70},{ 42, 70, 46},{ 44, 68, 65},{ 44, 65, 43},{ 8, 74, 79},{ 8, 79, 19},{ 17, 78, 76},{ 17, 76, 12},
        { 4, 73, 75},{ 4, 75, 9},{ 14, 77, 72},{ 14, 72, 3}
    };

    for(uint i = 0; i < indices.size(); ++i)
    {
        assert(correctindices[i][0] == indices[i][0]);
        assert(correctindices[i][1] == indices[i][1]);
        assert(correctindices[i][2] == indices[i][2]);
    }
}

void testmissingskeletal()
{
    std::printf("testing GLTF file with no skeletal or weights data\n");
    std::string modelname = "gltf/obj_cube.gltf";

    GLTFModelInfo mi(modelname, true);
    std::string meshname = mi.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];

    std::vector<std::array<float, 4>> weights = mi.getweights(meshname);
    std::vector<std::array<uint, 4>> joints = mi.getjoints(meshname);

    assert(weights.size() == 0);
    assert(joints.size() == 0);
}

void testminified()
{
    std::printf("testing minified GLTF file loading\n");
    std::string modelname1 = "gltf/obj_cube.gltf";
    std::string modelname2 = "gltf/obj_cube_minified.gltf";

    GLTFModelInfo mi1(modelname1);
    GLTFModelInfo mi2(modelname2);

    assert(mi1 == mi2);
}

void testtabulated()
{
    std::printf("testing tab-indented GLTF file loading\n");
    std::string modelname1 = "gltf/obj_cube.gltf";
    std::string modelname2 = "gltf/obj_cube_tabs.gltf";

    GLTFModelInfo mi1(modelname1);
    GLTFModelInfo mi2(modelname2);

    assert(mi1 == mi2);
}

void testinvalidfile()
{
    std::printf("testing exception throwing for invalid GLTF file path\n");
    std::string modelname1 = "gltf/not_a_gltf.gltf";
    bool exceptioncaught = false;
    try
    {
        GLTFModelInfo mi1(modelname1);
    }
    catch(const std::ios_base::failure &e)
    {
        exceptioncaught = true;
        std::printf("Exception thrown: %s\n", e.what());
    }
    assert(exceptioncaught);
}

void testbraceoverflow()
{
    std::printf("testing GLTF file with too many closing braces\n");
    std::string modelname1 = "gltf/braces.gltf";
    bool exceptioncaught = false;
    try
    {
        GLTFModelInfo mi1(modelname1);
    }
    catch(const std::logic_error &e)
    {
        exceptioncaught = true;
        std::printf("Exception thrown: %s\n", e.what());
    }
    assert(exceptioncaught);
}

void testbracketunderflow()
{
    std::printf("testing GLTF file with too few closing brackets\n");
    std::string modelname1 = "gltf/brackets.gltf";
    bool exceptioncaught = false;
    try
    {
        GLTFModelInfo mi1(modelname1);
    }
    catch(const std::logic_error &e)
    {
        exceptioncaught = true;
        std::printf("Exception thrown: %s\n", e.what());
    }
    assert(exceptioncaught);
}


//test object with vertex indices out of bounds of the vertex positions array
void testvertexmismatch()
{
    std::printf("testing GLTF file with invalid vertex indices\n");
    std::string modelname1 = "gltf/obj_cube_missing_vertex.gltf";
    GLTFModelInfo mi1(modelname1);
    std::string meshname = mi1.getnodenames(GLTFModelInfo::NodeType_Mesh)[0];

    bool exceptioncaught = false;
    try
    {
        mi1.getindices(meshname);
    }
    catch(const std::logic_error &e)
    {
        exceptioncaught = true;
        std::printf("Exception thrown: %s\n", e.what());
    }
    assert(exceptioncaught);
}

void testmultimesh()
{
    std::printf("test loading of GLTF file with multiple meshes\n");
    std::string modelname = "gltf/twocube.gltf";
    GLTFModelInfo mi(modelname);
    std::vector<std::string> meshnames = mi.getnodenames(GLTFModelInfo::NodeType_Mesh);
    assert(meshnames.size() == 2);
}

void testequals()
{
    std::printf("test GLTF file equality\n");
    std::string modelname = "gltf/twocube.gltf",
                modelname2 = "gltf/box.gltf";
    GLTFModelInfo mi(modelname),
                  mi2(modelname2);
    assert((mi == mi2) == false);
}

void testnodetranslate()
{
    std::printf("test GLTF node translate\n");
    std::string modelname = "gltf/twocube.gltf";
    GLTFModelInfo mi(modelname);
    std::vector<std::string> nodenames = mi.getnodenames(GLTFModelInfo::NodeType_Mesh);
    std::vector<std::array<float, 3>> positions0 = mi.getpositions(nodenames[0]);
    std::vector<std::array<float, 3>> positions1 = mi.getpositions(nodenames[1]);

    //check that all of node 0 is untranslated
    for(std::array<float, 3> p : positions0)
    {
        assert(p[1] <= 1);
    }
    //check that all of node 1 is translated
    for(std::array<float, 3> p : positions1)
    {
        assert(p[1] >= 2);
    }
}

void test_gltf()
{
    std::printf(
"===============================================================\n\
testing GLTF file loader\n\
===============================================================\n"
    );

    testmeshnames();
    testbounds();
    testpositions();
    testnormals();
    testtexcoords();
    testjoints();
    testweights();
    testindices();
    testmissingskeletal();
    testminified();
    testtabulated();
    testinvalidfile();
    testbraceoverflow();
    testbracketunderflow();
    testvertexmismatch();
    testmultimesh();
    testequals();
    testnodetranslate();
};
