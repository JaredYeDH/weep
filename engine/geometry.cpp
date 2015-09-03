#include "geometry.hpp"
#include "image.hpp"
#include <sstream>
#include <fstream>

namespace {
	std::vector<std::string> split(const std::string &str, char delim) {
		std::stringstream ss(str);
		std::vector<std::string> elems;
		std::string item;
		while (std::getline(ss, item, delim))
			elems.push_back(item);
		return elems;
	}
}

Geometry::Geometry(const string& path)
{
	uint lineNumber = 0;
	std::string row;
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		logError("Failed to load object %s", path.c_str());
		return;
	}

	batches.emplace_back();
	Batch* batch = &batches.back();

	std::map<string, uint> mtlMap;
	std::vector<vec3> tmpVerts;
	std::vector<vec2> tmpUvs;
	std::vector<vec3> tmpNormals;
	while (std::getline(file, row)) {
		++lineNumber;
		std::istringstream srow(row);
		float x,y,z;
		std::string tempst;
		if (row.substr(0, 7) == "usemtl ") { // Material
			std::string materialName;
			srow >> tempst >> materialName;
			if (mtlMap.empty()) {
				mtlMap[materialName] = 0;
			} else if (mtlMap.find(materialName) == mtlMap.end()) {
				mtlMap[materialName] = batches.size();
				batches.emplace_back();
				batch = &batches.back();
			} else {
				batch = &batches.at(mtlMap[materialName]);
			}
		} else if (row.substr(0,2) == "v ") {  // Vertices
			srow >> tempst >> x >> y >> z;
			tmpVerts.push_back(vec3(x, y, z));
		} else if (row.substr(0,2) == "vt") {  // Texture Coordinates
			srow >> tempst >> x >> y;
			tmpUvs.push_back(vec2(x, -y));
		} else if (row.substr(0,2) == "vn") {  // Normals
			srow >> tempst >> x >> y >> z;
			tmpNormals.push_back(glm::normalize(vec3(x, y, z)));
		} else if (row.substr(0,2) == "f ") {  // Faces
			srow >> tempst; // Eat away prefix
			// Parse face point's coordinate references
			for (int i = 1; srow >> tempst; ++i) {
				if (i > 4) {
					logError("Only triangle and quad faces are supported in %s:%d", path.c_str(), lineNumber);
					break;
				}
				std::vector<std::string> indices = split(tempst, '/');
				if (indices.size() == 0 || indices.size() > 3) {
					logError("Invalid face definition in %s:%d", path.c_str(), lineNumber);
					continue;
				} else if (indices.size() < 3) {
					indices.resize(3);
				}
				// Vertex indices are 1-based in the file
				if (i <= 3) {
					if (!indices[0].empty()) batch->positions.push_back(tmpVerts.at(std::stoi(indices[0]) - 1));
					if (!indices[1].empty()) batch->texcoords.push_back(tmpUvs.at(std::stoi(indices[1]) - 1));
					if (!indices[2].empty()) batch->normals.push_back(tmpNormals.at(std::stoi(indices[2]) - 1));
				} else if (i == 4) {
					// Manually create a new triangle
					if (!indices[0].empty()) {
						size_t lastPos = batch->positions.size();
						batch->positions.push_back(tmpVerts.at(std::stoi(indices[0]) - 1));
						batch->positions.push_back(batch->positions.at(lastPos - 3));
						batch->positions.push_back(batch->positions.at(lastPos - 1));
					}
					if (!indices[1].empty()) {
						size_t lastPos = batch->texcoords.size();
						batch->texcoords.push_back(tmpUvs.at(std::stoi(indices[1]) - 1));
						batch->texcoords.push_back(batch->texcoords.at(lastPos - 3));
						batch->texcoords.push_back(batch->texcoords.at(lastPos - 1));
					}
					if (!indices[2].empty()) {
						size_t lastPos = batch->normals.size();
						batch->normals.push_back(tmpNormals.at(std::stoi(indices[2]) - 1));
						batch->normals.push_back(batch->normals.at(lastPos - 3));
						batch->normals.push_back(batch->normals.at(lastPos - 1));
					}
				}
			}
		}
	}
	calculateBoundingSphere();
	calculateBoundingBox();
	logDebug("Loaded mesh %s with %d batches, bound r: %f",
		path.c_str(), batches.size(), bounds.radius);
	if (batch[0].normals.empty())
		calculateNormals();
	else normalizeNormals();
	for (auto& batch : batches)
		batch.setupAttributes();
}

Geometry::Geometry(const Image& heightmap)
{
	batches.emplace_back();
	Batch& batch = batches.back();
	auto& indices = batch.indices;
	auto& positions = batch.positions;
	auto& texcoords = batch.texcoords;
	auto& normals = batch.normals;

	int wpoints = heightmap.width + 1;
	int numVerts = wpoints * (heightmap.height + 1);
	positions.resize(numVerts);
	texcoords.resize(numVerts);
	normals.resize(numVerts);
	for (int j = 0; j <= heightmap.height; ++j) {
		for (int i = 0; i <= heightmap.width; ++i) {
			// Create the vertex
			float x = i - heightmap.width * 0.5f;
			float z = j - heightmap.height * 0.5f;
			float y = heightmap.data[heightmap.channels * (j * heightmap.width + i)] / 255.0f;
			int vert = j * wpoints + i;
			positions[vert] = vec3(x, y, z);
			texcoords[vert] = vec2((float)i / heightmap.width, (float)j / heightmap.height);
			normals[vert] = vec3(0, 1, 0);
			// Indexed faces
			if (i == heightmap.width || j == heightmap.height)
				continue;
			uint a = i + wpoints * j;
			uint b = i + wpoints * (j + 1);
			uint c = (i + 1) + wpoints * (j + 1);
			uint d = (i + 1) + wpoints * j;
			uint triangles[] = { a, b, d, b, c, d };
			indices.insert(indices.end(), triangles, triangles + 6);
		}
	}
	calculateBoundingSphere();
	calculateBoundingBox();
	calculateNormals();
	batch.setupAttributes();
}

Geometry::~Geometry()
{
}

void Geometry::calculateBoundingSphere()
{
	float maxRadiusSq = 0;
	for (auto& batch : batches)
		for (auto& pos : batch.positions)
			maxRadiusSq = glm::max(maxRadiusSq, glm::length2(pos));
	bounds.radius = glm::sqrt(maxRadiusSq);
}

void Geometry::calculateBoundingBox()
{
	Bounds& bb = bounds;
	bb.min = vec3();
	bb.max = vec3();

	for (auto& batch : batches) {
		auto& positions = batch.positions;
		for (uint i = 0, len = positions.size(); i < len; ++i) {
			float x = positions[i].x;
			float y = positions[i].y;
			float z = positions[i].z;
			if (x < bb.min.x) bb.min.x = x;
			else if ( x > bb.max.x ) bb.max.x = x;
			if (y < bb.min.y) bb.min.y = y;
			else if ( y > bb.max.y ) bb.max.y = y;
			if (z < bb.min.z) bb.min.z = z;
			else if (z > bb.max.z) bb.max.z = z;
		}
	}
}

void Geometry::calculateNormals()
{
	for (auto& batch : batches) {
		auto& indices = batch.indices;
		auto& positions = batch.positions;
		auto& normals = batch.normals;
		// Indexed elements
		if (!indices.empty()) {
			// Reset existing normals
			for (auto& normal : normals)
				normal = vec3();
			for (uint i = 0, len = indices.size(); i < len; i += 3) {
				vec3 normal = glm::triangleNormal(positions[indices[i]], positions[indices[i+1]], positions[indices[i+2]]);
				normals[indices[i+0]] += normal;
				normals[indices[i+1]] += normal;
				normals[indices[i+2]] += normal;
			}
			normalizeNormals();
		// Non-indexed elements
		} else {
			normals.resize(positions.size());
			for (uint i = 0, len = positions.size(); i < len; i += 3) {
				vec3 normal = glm::triangleNormal(positions[i], positions[i+1], positions[i+2]);
				normals[i+0] = normal;
				normals[i+1] = normal;
				normals[i+2] = normal;
			}
		}
	}
}

void Geometry::normalizeNormals()
{
	for (auto& batch : batches)
		for (auto& normal : batch.normals)
			normal = glm::normalize(normal);
}


Batch::~Batch()
{
	ASSERT(renderId == -1);
}

void Batch::setupAttributes()
{
	ASSERT(positions.empty() || positions2d.empty());
	int offset = 0;
	const char* dataArrays[ATTR_MAX] = {0};
	if (!positions.empty()) {
		Attribute& attr = attributes[ATTR_POSITION];
		attr.components = 3;
		attr.offset = offset;
		offset += sizeof(positions[0]);
		numVertices = positions.size();
		dataArrays[ATTR_POSITION] = (char*)&positions[0];
	}
	if (!positions2d.empty()) {
		Attribute& attr = attributes[ATTR_POSITION];
		attr.components = 2;
		attr.offset = offset;
		offset += sizeof(positions2d[0]);
		numVertices = positions2d.size();
		dataArrays[ATTR_POSITION] = (char*)&positions2d[0];
	}
	if (!texcoords.empty()) {
		Attribute& attr = attributes[ATTR_TEXCOORD];
		attr.components = 2;
		attr.offset = offset;
		offset += sizeof(texcoords[0]);
		dataArrays[ATTR_TEXCOORD] = (char*)&texcoords[0];
	}
	if (!normals.empty()) {
		Attribute& attr = attributes[ATTR_NORMAL];
		attr.components = 3;
		attr.offset = offset;
		offset += sizeof(normals[0]);
		dataArrays[ATTR_NORMAL] = (char*)&normals[0];
	}
	vertexSize = offset;
	vertexData.resize(numVertices * vertexSize);
	for (uint i = 0; i < numVertices; ++i) {
		char* dst = &vertexData[i * vertexSize];
		for (uint a = 0; a < ATTR_MAX; ++a) {
			if (dataArrays[a]) {
				uint elementSize = attributes[a].components * sizeof(float);
				std::memcpy(dst + attributes[a].offset, dataArrays[a] + i * elementSize, elementSize);
			}
		}
	}
}
