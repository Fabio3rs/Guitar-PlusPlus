#include "GPPOBJ.h"
#include "CEngine.h"
#include <iostream>

void GPPOBJ::draw(unsigned int texture)
{
	vbodata.texture = texture;
	CEngine::engine().RenderCustomVerticesFloat(vbodata);
}

void GPPOBJ::load(const std::string &path)
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	auto cpy = [](std::vector<int8_t> &data, int8_t *ptr, int size)
	{
		data.insert(data.end(), ptr, ptr + size);
	};

	bool res = loadOBJ(path.c_str(), vertices, uvs, normals);

	//data.reserve(vertices.size() * sizeof(glm::vec3) + uvs.size() * sizeof(glm::vec3) + normals.size() * sizeof(glm::vec3));

	vbodata.vertexL = data.size();
	cpy(data, (int8_t*)&vertices[0], vertices.size() * sizeof(glm::vec3));

	vbodata.uvL = data.size();
	cpy(data, (int8_t*)&uvs[0], uvs.size() * sizeof(glm::vec2));

	vbodata.normalsL = data.size();
	cpy(data, (int8_t*)&normals[0], normals.size() * sizeof(glm::vec3));

	vbodata.pointer = &data[0];
	vbodata.sizebytes = data.size();

	vbodata.count = vertices.size();
}

GPPOBJ::GPPOBJ(const std::string &path)
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	auto cpy = [](std::vector<int8_t> &data, int8_t *ptr, int size)
	{
		data.insert(data.end(), ptr, ptr+size);
	};
	
	bool res = loadOBJ(path.c_str(), vertices, uvs, normals);

	//data.reserve(vertices.size() * sizeof(glm::vec3) + uvs.size() * sizeof(glm::vec3) + normals.size() * sizeof(glm::vec3));

	vbodata.vertexL = data.size();
	cpy(data, (int8_t*)&vertices[0], vertices.size() * sizeof(glm::vec3));

	vbodata.uvL = data.size();
	cpy(data, (int8_t*)&uvs[0], uvs.size() * sizeof(glm::vec2));

	vbodata.normalsL = data.size();
	cpy(data, (int8_t*)&normals[0], normals.size() * sizeof(glm::vec3));

	vbodata.pointer = &data[0];
	vbodata.sizebytes = data.size();

	vbodata.count = vertices.size();
}

GPPOBJ::GPPOBJ()
{

}

