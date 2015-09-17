#include "scene.hpp"
#include "material.hpp"
#include "geometry.hpp"
#include "resources.hpp"
#include "image.hpp"
#include "engine.hpp"
#include "physics.hpp"

#define USE_DEBUG_NAMES

namespace {

	Json assign(Json lhs, const Json& rhs) {
		ASSERT(rhs.is_object());
		Json::object object;
		if (lhs.is_object())
			object = lhs.object_items();
		for (auto& item : rhs.object_items()) {
			if (item.second.is_object())
				object[item.first] = assign(lhs[item.first], item.second);
			else object[item.first] = item.second;
		}
		return Json(object);
	}

	Material* parseMaterial(Material* material, const Json& def, Resources& resources) {
		ASSERT(def.is_object());
		if (def["shaderName"].is_string())
			material->shaderName = def["shaderName"].string_value();
		if (def["tessellate"].bool_value())
			material->tessellate = true;
		if (!def["ambient"].is_null())
			material->ambient = colorToVec3(def["ambient"]);
		if (!def["diffuse"].is_null())
			material->diffuse = colorToVec3(def["diffuse"]);
		if (!def["specular"].is_null())
			material->specular = colorToVec3(def["specular"]);
		if (!def["shininess"].is_null())
			material->shininess = def["shininess"].number_value();

		if (!def["uvOffset"].is_null())
			material->uvOffset = toVec2(def["uvOffset"]);
		if (!def["uvRepeat"].is_null())
			material->uvRepeat = toVec2(def["uvRepeat"]);

		if (!def["diffuseMap"].is_null()) {
			material->map[Material::DIFFUSE_MAP] = resources.getImage(def["diffuseMap"].string_value());
			material->map[Material::DIFFUSE_MAP]->sRGB = true;
		}
		if (!def["specularMap"].is_null()) {
			material->map[Material::SPECULAR_MAP] = resources.getImage(def["specularMap"].string_value());
			material->map[Material::SPECULAR_MAP]->sRGB = true;
		}
		if (!def["emissionMap"].is_null()) {
			material->map[Material::EMISSION_MAP] = resources.getImage(def["emissionMap"].string_value());
			material->map[Material::EMISSION_MAP]->sRGB = true;
		}
		if (!def["normalMap"].is_null())
			material->map[Material::NORMAL_MAP] = resources.getImage(def["normalMap"].string_value());
		if (!def["heightMap"].is_null())
			material->map[Material::HEIGHT_MAP] = resources.getImage(def["heightMap"].string_value());
		if (!def["reflectionMap"].is_null())
			material->map[Material::REFLECTION_MAP] = resources.getImage(def["reflectionMap"].string_value());

		return material;
	}

	void parseModel(Model& model, const Json& def, Resources& resources) {
		// Parse geometry
		if (!def["geometry"].is_null()) {
			const Json& defGeom = def["geometry"];
			if (defGeom.is_string())
				model.geometry = resources.getGeometry(defGeom.string_value());
			else model.geometry = resources.getHeightmap(defGeom["heightmap"].string_value());
		}

		// Parse transform
		if (!def["position"].is_null()) {
			model.position = toVec3(def["position"]);
		}
		if (!def["rotation"].is_null()) {
			model.rotation = quat(toVec3(def["rotation"]));
		}
		if (!def["scale"].is_null()) {
			const Json& scaleDef = def["scale"];
			model.scale = scaleDef.is_number() ? vec3(scaleDef.number_value()) : toVec3(scaleDef);
		}

		if (model.geometry) {
			model.bounds.min = model.geometry->bounds.min * model.scale;
			model.bounds.max = model.geometry->bounds.max * model.scale;
			model.bounds.radius = model.geometry->bounds.radius * glm::compMax(model.scale);
		}

		// Parse material
		const Json& materialDef = def["material"];
		if (materialDef.is_object()) {
			ASSERT(model.materials.size() <= 1);
			if (model.materials.empty())
				model.materials.push_back(new Material());
			parseMaterial(model.materials.back(), materialDef, resources);
		} else if (materialDef.is_array()) {
			if (model.materials.empty()) {
				for (auto& matDef : materialDef.array_items())
					model.materials.push_back(parseMaterial(new Material(), matDef, resources));
			} else {
				ASSERT(model.materials.size() == materialDef.array_items().size());
				for (uint i = 0; i < materialDef.array_items().size(); ++i)
					parseMaterial(model.materials[i], materialDef[i], resources);
			}
		}
	}
}

void Scene::load(const string& path, Resources& resources)
{
	uint t0 = Engine::timems();
	std::string err;
	Json jsonScene = Json::parse(resources.getText(path, Resources::NO_CACHE), err);
	if (!err.empty())
		panic("Failed to read scene %s: %s", path.c_str(), err.c_str());

	uint numModels = 0, numBodies = 0, numLights = 0;

	// Parse prefabs
	if (jsonScene.is_object() && jsonScene["prefabs"].is_object()) {
		const Json::object& prefabs = jsonScene["prefabs"].object_items();
		for (auto& it : prefabs)
			m_prefabs[it.first] = it.second;
	}

	// Parse objects
	const Json::array& objects = jsonScene.is_array() ? jsonScene.array_items() : jsonScene["objects"].array_items();
	for (uint i = 0; i < objects.size(); ++i) {
		Json def = objects[i];
		ASSERT(def.is_object());

		if (def["prefab"].is_string()) {
			auto prefabIter = m_prefabs.find(def["prefab"].string_value());
			if (prefabIter != m_prefabs.end()) {
				def = assign(prefabIter->second, def);
			} else {
				logWarning("Could not find prefab \"%s\"", def["prefab"].string_value().c_str());
			}
		}

		entity::Entity entity = world.create();

		if (def["name"].is_string()) {
			entity.tag(def["name"].string_value());
		} else {
#ifdef USE_DEBUG_NAMES
			static uint debugId = 0;
			string name;
			if (def["prefab"].is_string())
				name = def["prefab"].string_value() + "#";
			else name = "object#";
			name += std::to_string(debugId++);
			entity.tag(name);
#endif
		}

		// Parse light
		const Json& lightDef = def["light"];
		if (!lightDef.is_null()) {
			Light light;
			const string& lightType = lightDef["type"].string_value();
			if (lightType == "ambient") light.type = Light::AMBIENT_LIGHT;
			else if (lightType == "point") light.type = Light::POINT_LIGHT;
			else if (lightType == "directional") light.type = Light::DIRECTIONAL_LIGHT;
			else if (lightType == "spot") light.type = Light::SPOT_LIGHT;
			else if (lightType == "area") light.type = Light::AREA_LIGHT;
			else if (lightType == "hemisphere") light.type = Light::HEMISPHERE_LIGHT;
			else logError("Unknown light type \"%s\"", lightType.c_str());
			if (!lightDef["color"].is_null())
				light.color = colorToVec3(lightDef["color"]);
			if (!def["position"].is_null())
				light.position = toVec3(def["position"]);
			if (!lightDef["direction"].is_null())
				light.direction = toVec3(lightDef["direction"]);
			if (!lightDef["distance"].is_null())
				light.distance = lightDef["distance"].number_value();
			if (!lightDef["decay"].is_null())
				light.decay = lightDef["decay"].number_value();
			entity.add(light);
			numLights++;
		}

		if (!def["geometry"].is_null()) {
			Model model;
			parseModel(model, def, resources);
			entity.add(model);
			numModels++;
		}

		// Parse body (needs to be after geometry, transform, bounds...)
		if (!def["body"].is_null()) {
			const Json& bodyDef = def["body"];
			ASSERT(bodyDef.is_object());
			ASSERT(entity.has<Model>());
			const Model& model = entity.get<Model>();

			btCollisionShape* shape = NULL;
			const string& shapeStr = bodyDef["shape"].string_value();
			if (shapeStr == "box") {
				Bounds aabb = model.bounds;
				shape = new btBoxShape(convert((aabb.max - aabb.min) * 0.5f));
			} else if (shapeStr == "sphere") {
				shape = new btSphereShape(model.bounds.radius);
			} else {
				logError("Unknown shape %s", shapeStr.c_str());
			}
			ASSERT(shape);

			float mass = 0.f;
			if (bodyDef["mass"].is_number())
				mass = bodyDef["mass"].number_value();

			btVector3 inertia(0, 0, 0);
			shape->calculateLocalInertia(mass, inertia);

			btRigidBody::btRigidBodyConstructionInfo info(mass, NULL, shape, inertia);
			info.m_startWorldTransform = btTransform(convert(model.rotation), convert(model.position));
			btRigidBody* body = new btRigidBody(info);
			entity.add(body);
			numBodies++;
		}
	}
	uint t1 = Engine::timems();
	logDebug("Loaded scene in %dms with %d models, %d bodies, %d lights, %d prefabs", t1 - t0, numModels, numBodies, numLights, m_prefabs.size());
}

void Scene::reset()
{
	m_prefabs.clear();
}
