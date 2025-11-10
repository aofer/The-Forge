#pragma once

//This header will contain all the components in the engine i.e mesh, scene, etc.

#include <vector>
#include "../../../../Common_3/Utilities/Math/MathTypes.h"

typedef unsigned int uint32_t;

class Buffer;
struct Geometry;
struct GeometryData;

struct MeshComponent
{

	enum FLAGS
    {

    };
    // cpu side data

    uint32_t flags;

	//GPU side data
	Geometry*     geom;
	//CPU side data
    GeometryData* geomData;
	//save these for future, right now we'll use the geometry and resource loader that's already here
	//std::vector<float3> vertexPos;
	//std::vector<float3> vertexNormals;
	//std::vector<float3> vertexUV1;

	////gpu buffers
 //   Buffer* posBuffer = nullptr;
 //   Buffer* NormalsBuffer = nullptr;
 //   Buffer* UVsBuffer = nullptr;

};

struct MaterialComponent
{
    char** textures;
    char** normalMaps;
    char** specularMaps;

};