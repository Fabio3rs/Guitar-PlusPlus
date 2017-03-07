#include "GPPOBJ.h"
#include "CEngine.h"
#include <iostream>
#include "CShader.h"

bool GPPOBJ::loadInternalObj(const std::string &path, const std::string &objName, bool loadMtlLib)
{
	std::fstream obj(path, std::ios::in | std::ios::binary);

	if (!obj.is_open())
		return false;

	multiData.clear();

	modelData mData;

	std::deque<modelPartIndex> modelPartIndexes;

	std::array<char, 1024> buffer;
	std::array<char, 32> bufferLn;
	std::array<char, 256> bufferMtlName;

	bool acceptPosData = true;
	int64_t lines = -1;

	while (obj.getline(buffer.data(), buffer.size() - 1))
	{
		++lines;
		gppVec3f v{0.0f, 0.0f, 0.0f};

		int readResult = sscanf(buffer.data(), "%31s %f %f %f", bufferLn.data(), &v.x, &v.y, &v.z);

		if (readResult > 0)
		{
			std::cout << bufferLn.data() << std::endl;

			if (strcmp(bufferLn.data(), "v") == 0)
			{
				if (readResult != 4)
				{
					std::cout << "read fail\n";
					return false;
				}

				mData.vertices.push_back(v);
			}else if (strcmp(bufferLn.data(), "vt") == 0)
			{
				if (readResult != 3)
				{
					std::cout << "read fail\n";
					return false;
				}

				mData.uvs.push_back({v.x, v.y});
			}else if (strcmp(bufferLn.data(), "vn") == 0)
			{
				if (readResult != 4)
				{
					std::cout << "read fail\n";
					return false;
				}

				mData.normals.push_back(v);
			}
			else if (strcmp(bufferLn.data(), "usemtl") == 0)
			{
				readResult = sscanf(buffer.data(), "%31s %255s", bufferLn.data(), bufferMtlName.data());
				
				if (readResult == 2)
				{
					modelPartIndexes.push_back({});

					auto &index = modelPartIndexes.back();

					index.name = bufferMtlName.data();
				}
			}
			else if (strcmp(bufferLn.data(), "f") == 0)
			{
				if (modelPartIndexes.size() == 0)
				{
					std::cout << "f without usemtl\n";
					return false;
				}

				auto &index = modelPartIndexes.back();
				unsigned int vertexIndex[3], uvIndex[3] = { 0, 0, 0 }, normalIndex[3];

				int matches = sscanf(buffer.data(), "%31s %d/%d/%d %d/%d/%d %d/%d/%d\n", bufferLn.data(), &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);

				if (matches != 10)
				{
					std::cout << "matches fail " << matches << " at line " << lines << std::endl;
					return false;
				}

				index.vertexIndices.push_back(vertexIndex[0]);
				index.vertexIndices.push_back(vertexIndex[1]);
				index.vertexIndices.push_back(vertexIndex[2]);
				index.uvIndices.push_back(uvIndex[0]);
				index.uvIndices.push_back(uvIndex[1]);
				index.uvIndices.push_back(uvIndex[2]);
				index.normalIndices.push_back(normalIndex[0]);
				index.normalIndices.push_back(normalIndex[1]);
				index.normalIndices.push_back(normalIndex[2]);
			}
		}
	}

	auto out_part = [&mData](modelPartIndex &part, std::vector<gppVec3f> &out_vertices,
		std::vector<gppVec2f> &out_uvs,
		std::vector<gppVec3f> &out_normals)
	{
		for (unsigned int i = 0; i< part.vertexIndices.size(); i++)
		{
			out_vertices.push_back(mData.vertices[part.vertexIndices[i] - 1]);
			out_uvs.push_back(mData.uvs[part.uvIndices[i] - 1]);
			out_normals.push_back(mData.normals[part.normalIndices[i] - 1]);
		}
	};

	auto cpy = [](std::vector<int8_t> &data, int8_t *ptr, int size)
	{
		data.insert(data.end(), ptr, ptr + size);
	};

	for (auto &part : modelPartIndexes)
	{
		std::vector<gppVec3f> vertices;
		std::vector<gppVec2f> uvs;
		std::vector<gppVec3f> normals;

		multiData.push_back({});
		auto &adata = multiData.back();

		out_part(part, vertices, uvs, normals);

		auto &data = adata.data;

		{
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
	}

	return false;
}

void GPPOBJ::draw(unsigned int texture, bool autoBindZeroVBO)
{
	vbodata.texture = texture;
	//CShader::inst().processEvent(0);
	CEngine::engine().RenderCustomVerticesFloat(vbodata, autoBindZeroVBO);
}

void GPPOBJ::load(const std::string &path)
{
	lastPath = path;
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

void GPPOBJ::load(const char *path)
{
	lastPath = path;
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	auto cpy = [](std::vector<int8_t> &data, int8_t *ptr, int size)
	{
		data.insert(data.end(), ptr, ptr + size);
	};

	bool res = loadOBJ(path, vertices, uvs, normals);

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

void GPPOBJ::reload(const std::string &path)
{
	load(lastPath);
}

void GPPOBJ::unload()
{
	data.clear();
	vbodata.pointer = nullptr;
	vbodata.destroy();
}

GPPOBJ::GPPOBJ(const std::string &path) : GPPOBJ()
{
	lastPath = path;
	load(lastPath);
	/*std::vector<glm::vec3> vertices;
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

	vbodata.count = vertices.size();*/
}

GPPOBJ::GPPOBJ()
{
	keepModelDataLoaded = false;
}

GPPOBJ::~GPPOBJ()
{
	unload();
}

