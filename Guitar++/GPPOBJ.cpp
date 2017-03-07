#include "GPPOBJ.h"
#include "CEngine.h"
#include <iostream>
#include "CShader.h"
#include "GPPGame.h"

void GPPOBJ::loadMtlLibData(const std::string &path, const std::string &file)
{
	mtlLib.clear();

	std::fstream mtl(path + "/" + file, std::ios::in | std::ios::binary);

	if (!mtl.is_open())
		return;

	mtlLoaded = true;

	std::array<char, 1024> buffer;
	std::array<char, 32> bufferLn;
	std::array<char, 256> bufferMtlName;

	bool acceptPosData = true;
	bool newmtllt = true;
	int64_t lines = -1;

	std::string usingmtl;

	while (mtl.getline(buffer.data(), buffer.size() - 1))
	{
		int readResult = sscanf(buffer.data(), "%31s %255s", bufferLn.data(), bufferMtlName.data());

		if (readResult == 2)
		{
			if (strcmp(bufferLn.data(), "newmtl") == 0)
			{
				if (!newmtllt)
				{
					auto &mtl = mtlLib[usingmtl];
					mtl.textureName = usingmtl;

					if (mtl.textureName.find(".tga") == std::string::npos)
					{
						mtl.textureName += ".tga";
					}

					mtl.textureID = GPPGame::GuitarPP().loadTexture(path, mtl.textureName).getTextId();
				}

				usingmtl = bufferMtlName.data();
				newmtllt = false;
				continue;
			}

			if (strcmp(bufferLn.data(), "map_Kd") == 0)
			{
				auto &mtl = mtlLib[usingmtl];
				mtl.textureName = bufferMtlName.data();
				mtl.textureID = GPPGame::GuitarPP().loadTexture(path, mtl.textureName).getTextId();
				newmtllt = true;
				//std::cout << path + " / " + mtl.textureName + std::to_string(mtl.textureID) << std::endl;
				continue;
			}
		}
	}

	if (!newmtllt)
	{
		auto &mtl = mtlLib[usingmtl];
		mtl.textureName = usingmtl;

		if (mtl.textureName.find(".tga") == std::string::npos)
		{
			mtl.textureName += ".tga";
		}

		mtl.textureID = GPPGame::GuitarPP().loadTexture(path, mtl.textureName).getTextId();
	}
}

bool GPPOBJ::loadInternalObj(const std::string &path, const std::string &file, const std::string &objName, bool loadMtlLib)
{
	std::fstream obj(path + "/" + file, std::ios::in | std::ios::binary);

	if (!obj.is_open())
		return false;

	multiData.clear();

	loadMtlLibData(path, "garage_gpp.mtl");

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
			//std::cout << bufferLn.data() << std::endl;

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
					modelPartIndexes.push_back({});

					auto &index = modelPartIndexes.back();

					index.name = "None";
				}

				auto &index = modelPartIndexes.back();
				unsigned int vertexIndex[3], uvIndex[3] = { 0, 0, 0 }, normalIndex[3];

				int matches = sscanf(buffer.data(), "%31s %d/%d/%d %d/%d/%d %d/%d/%d\n", bufferLn.data(), &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2]);

				if (matches != 10)
				{
					matches = sscanf(buffer.data(), "%31s %d//%d %d//%d %d//%d\n", bufferLn.data(), &vertexIndex[0], &normalIndex[0], &vertexIndex[1], &normalIndex[1], &vertexIndex[2], &normalIndex[2]);
					if (matches != 7)
					{
						std::cout << "matches fail " << matches << " at line " << lines << std::endl;
						return false;
					}
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
		for (unsigned int i = 0; i < part.vertexIndices.size(); i++)
		{
			unsigned int vertexIndex = part.vertexIndices[i];
			unsigned int uvIndex = part.uvIndices[i];
			unsigned int normalIndex = part.normalIndices[i];

			if (uvIndex != 0)
			{
				// Get the attributes thanks to the index
				auto vertex = mData.vertices[vertexIndex - 1];
				auto uv = mData.uvs[uvIndex - 1];
				auto normal = mData.normals[normalIndex - 1];

				// Put the attributes in buffers
				out_vertices.push_back(vertex);
				out_uvs.push_back(uv);
				out_normals.push_back(normal);
			}
			else
			{
				auto vertex = mData.vertices[vertexIndex - 1];
				auto normal = mData.normals[normalIndex - 1];

				// Put the attributes in buffers
				out_vertices.push_back(vertex);
				out_uvs.push_back({0, 0});
				out_normals.push_back(normal);
			}
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

		if (mtlLoaded)
		{
			adata.textureID = mtlLib[part.name].textureID;

			if (adata.textureID == 0)
				std::cout << part.name << std::endl;
		}
		else
		{
			adata.textureID = 0;
		}

		auto &data = adata.data;
		adata.vbodata.texture = adata.textureID;

		{
			adata.vbodata.vertexL = data.size();
			cpy(data, (int8_t*)&vertices[0], vertices.size() * sizeof(gppVec3f));

			adata.vbodata.uvL = data.size();
			cpy(data, (int8_t*)&uvs[0], uvs.size() * sizeof(gppVec2f));

			adata.vbodata.normalsL = data.size();
			cpy(data, (int8_t*)&normals[0], normals.size() * sizeof(gppVec3f));

			adata.vbodata.pointer = &data[0];
			adata.vbodata.sizebytes = data.size();

			adata.vbodata.count = vertices.size();
		}
	}

	std::cout << "multiData " << multiData.size() << std::endl;

	return true;
}

void GPPOBJ::draw(unsigned int texture, bool autoBindZeroVBO)
{
	//vbodata.texture = texture;
	//CShader::inst().processEvent(0);
	//CEngine::engine().RenderCustomVerticesFloat(vbodata, autoBindZeroVBO);

	for (auto &modelPart : multiData)
	{
		if (texture != 0)
			modelPart.vbodata.texture = texture;

		CEngine::engine().RenderCustomVerticesFloat(modelPart.vbodata, autoBindZeroVBO);
	}
}

void GPPOBJ::load(const std::string &path, const std::string &file)
{
	lastPath = path;
	loadInternalObj(path, file);
}

/*void GPPOBJ::reload(const std::string &path)
{
	load(lastPath);
}*/

void GPPOBJ::unload()
{
	//data.clear();
	//vbodata.pointer = nullptr;
	//vbodata.destroy();
	multiData.clear();
}

/*GPPOBJ::GPPOBJ(const std::string &path) : GPPOBJ()
{
	lastPath = path;
	load(lastPath);
}*/

GPPOBJ::GPPOBJ()
{
	keepModelDataLoaded = false;
	mtlLoaded = false;
}

GPPOBJ::~GPPOBJ()
{
	unload();
}

