#include "GPPOBJ.h"
#include "CEngine.h"

void GPPOBJ::draw(unsigned int texture)
{
	CEngine::engine().RenderCustomVericesFloat(&vertices[0], &uvs[0], &normals[0], vertices.size(), texture);
}

GPPOBJ::GPPOBJ(const std::string &path)
{
	bool res = loadOBJ(path.c_str(), vertices, uvs, normals);
}
