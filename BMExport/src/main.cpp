#include "GameLib/TypeRegistry.h"
#include "GameLib/TypeNotFoundException.h"
#include "GameLib/GMS/GMSStructureError.h"
#include "GameLib/PRP/PRPStructureError.h"
#include "GameLib/Scene/SceneObjectVisitorException.h"
#include "GameLib/TypeNotFoundException.h"
#include "GameLib/Level.h"
#include "AssetProvider.h"
#include "nlohmann/json.hpp"
#include "mati.h"
#include "Types.hpp"
#include "Prm.hpp"
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_set>

extern "C"
{ 
	__declspec(dllexport) int LoadData(char* buf_data, uint32_t buf_data_size, char* gms_data, uint32_t gms_data_size, char* mat_data, uint32_t mat_data_size, char* prm_data, uint32_t prm_data_size, char* prp_data, uint32_t prp_data_size, char* tex_data, uint32_t tex_data_size, char* tex_json, uint32_t tex_json_size);
	__declspec(dllexport) char* GetMaterialJson();
	__declspec(dllexport) char* GetSceneJson();
	__declspec(dllexport) char* GetTexJson();
	__declspec(dllexport) int FreeJson();
	__declspec(dllexport) int WriteObj(char* output_path, uint32_t primitive_id, uint32_t* sub_meshes, uint32_t sub_meshes_count);
}

AssetProvider asset_provider;
mati mat;
Prm prm;
nlohmann::ordered_json mat_json;
nlohmann::ordered_json scene_json;
nlohmann::ordered_json tex_json;
std::string json_string = "";

int FreeJson()
{
	json_string = "";
	return 0;
}

int LoadData(char* buf_data, uint32_t buf_data_size, char* gms_data, uint32_t gms_data_size, char* mat_data, uint32_t mat_data_size, char* prm_data, uint32_t prm_data_size, char* prp_data, uint32_t prp_data_size, char* tex_data, uint32_t tex_data_size, char* tex_json_data, uint32_t tex_json_data_size)
{
	asset_provider.LoadData(buf_data, buf_data_size, gms_data, gms_data_size, mat_data, mat_data_size, prm_data, prm_data_size, prp_data, prp_data_size, tex_data, tex_data_size);
	
	mat_json = nlohmann::ordered_json::object();
	scene_json = nlohmann::ordered_json::object();

	char* data = asset_provider.GetData(gamelib::io::AssetKind::MATERIALS);
	uint32_t size = asset_provider.GetDataSize(gamelib::io::AssetKind::MATERIALS);
	mat = mati();
	mat.load(data, size);
	mat.generate_json(mat_json);

	data = asset_provider.GetData(gamelib::io::AssetKind::GEOMETRY);
	size = asset_provider.GetDataSize(gamelib::io::AssetKind::GEOMETRY);
	prm = Prm();
	prm.Load(data, size);

	std::string tex_json_string = std::string(tex_json_data, tex_json_data_size);
	tex_json = nlohmann::ordered_json::parse(tex_json_string, nullptr, false, true);
	if (tex_json.is_discarded())
	{
		std::cout << "ERROR: Failed to parse file '%1'" << std::endl;
		return 0;
	}

	gamelib::TypeRegistry::getInstance().reset();

	std::string registry_string = types_registry;

	auto registryFile = nlohmann::json::parse(registry_string, nullptr, false, true);
	if (registryFile.is_discarded()) {
		std::cout << "Failed to load types database: invalid JSON format" << std::endl;
		return 0;
	}

	if (!registryFile.contains("inc") || !registryFile.contains("db"))
	{
		std::cout << "Invalid types database format" << std::endl;
		return 0;
	}

	auto &registry = gamelib::TypeRegistry::getInstance();

	std::unordered_map<std::string, std::string> typesToHashes;
	for (const auto &[hash, typeNameObj]: registryFile["db"].items())
	{
		typesToHashes[typeNameObj.get<std::string>()] = hash;
	}

	auto incPath = registryFile["inc"].get<std::string>();

	std::vector<nlohmann::json> typeInfos;
	for (auto& file : g1_types)
	{
		std::string json_string = file;

		auto typeInfoContents = json_string;

		auto &jsonContents = typeInfos.emplace_back();
		jsonContents = nlohmann::json::parse(typeInfoContents, nullptr, false, true);
		if (jsonContents.is_discarded())
		{
			std::cout << "ERROR: Failed to parse file '%1'" << std::endl;
			return 0;
		}
	}

	try
	{
		registry.registerTypes(std::move(typeInfos), std::move(typesToHashes));

		std::vector<std::string> allAvailableTypes;
		//gamelib::TypeRegistry::getInstance().forEachType([&allAvailableTypes](const gamelib::Type *type) { allAvailableTypes.push_back(std::string(type->getName())); });
	}
	catch (const gamelib::TypeNotFoundException &typeNotFoundException)
	{
		std::cout << "ERROR: Unable to load types database: %1" << std::endl;
		std::cout << "Unable to load types database" << std::endl;
	}
	catch (const std::exception &somethingGoesWrong)
	{
		std::cout << "ERROR: Unknown exception in type loader: %1" << std::endl;
		std::cout << "Unable to load types database" << std::endl;
	}

	auto provider = std::make_unique<AssetProvider>();
	provider->SetAssets(asset_provider.GetAssets());
	if (!provider)
	{
		std::cout << "Unable to load file %1" << std::endl;
		return 0;
	}

	if (!provider->isValid())
	{
		std::cout << "Invalid ZIP file instance!" << std::endl;
		return 0;
	}

	auto m_currentLevel = std::make_unique<gamelib::Level>(std::move(provider));
	if (!m_currentLevel)
	{
		return 0;
	}

	{
		if (!m_currentLevel->loadSceneData())
		{
			std::cout << "Unable to load scene data!" << std::endl;
			return 0;
		}
	}
	
	scene_json["Scene"] = nlohmann::ordered_json::array();

	int index = 0;
	for (auto& object : m_currentLevel->getSceneObjects())
	{
		nlohmann::ordered_json object_json;
		object_json["Index"] = index;
		object_json["Name"] = object->getName();
		object_json["Parent"] = nullptr;
		object_json["Parent Index"] = nullptr;
		if (std::shared_ptr<gamelib::scene::SceneObject> obj = object->getParent().lock())
		{
			object_json["Parent"] = obj->getName();
		}
		auto parentIndex = object->getGeomInfo().getParentGeomIndex();
		if (parentIndex != gamelib::gms::GMSGeomEntity::kInvalidParent)
		{
			object_json["Parent Index"] = parentIndex;
		}
		object_json["Children"] = nlohmann::ordered_json::array();
		for (auto& child : object->getChildren())
		{
			if (std::shared_ptr<gamelib::scene::SceneObject> obj = child.lock())
			{
				nlohmann::ordered_json child_json;
				child_json["Name"] = obj->getName();
				child_json["Index"] = obj->getGeomInfo().getGeomIndex();
				object_json["Children"].push_back(child_json);
			}
		}
		object_json["Type"] = object->getType()->getName();
		object_json["Primitive Id"] = object->getGeomInfo().getPrimitiveId();
		prm.GetModelJson(object_json["Primitive Id"], object_json, mat_json, tex_json);
		if (object->getProperties().getInstructions().size() > 16)
		{
			if (object->getProperties().getInstructions()[1].isBeginArray() && object->getProperties().getInstructions()[11].isEndArray())
			{
				object_json["Rotation"]["X Axis"]["X"] = object->getProperties().getInstructions()[2].getOperand().get<float>();
				object_json["Rotation"]["X Axis"]["Y"] = object->getProperties().getInstructions()[3].getOperand().get<float>();
				object_json["Rotation"]["X Axis"]["Z"] = object->getProperties().getInstructions()[4].getOperand().get<float>();
				object_json["Rotation"]["Y Axis"]["X"] = object->getProperties().getInstructions()[5].getOperand().get<float>();
				object_json["Rotation"]["Y Axis"]["Y"] = object->getProperties().getInstructions()[6].getOperand().get<float>();
				object_json["Rotation"]["Y Axis"]["Z"] = object->getProperties().getInstructions()[7].getOperand().get<float>();
				object_json["Rotation"]["Z Axis"]["X"] = object->getProperties().getInstructions()[8].getOperand().get<float>();
				object_json["Rotation"]["Z Axis"]["Y"] = object->getProperties().getInstructions()[9].getOperand().get<float>();
				object_json["Rotation"]["Z Axis"]["Z"] = object->getProperties().getInstructions()[10].getOperand().get<float>();
			}
			if (object->getProperties().getInstructions()[12].isBeginArray() && object->getProperties().getInstructions()[16].isEndArray())
			{
				object_json["Position"]["X"] = object->getProperties().getInstructions()[13].getOperand().get<float>();
				object_json["Position"]["Y"] = object->getProperties().getInstructions()[14].getOperand().get<float>();
				object_json["Position"]["Z"] = object->getProperties().getInstructions()[15].getOperand().get<float>();
			}
		}
		scene_json["Scene"].push_back(object_json);
		index++;
	}

	return 0;
}

char* GetMaterialJson()
{
	json_string = mat_json.dump();
	return &json_string[0];
}

char* GetSceneJson()
{
	json_string = scene_json.dump();
	return &json_string[0];
}

char* GetTexJson()
{
	json_string = tex_json.dump();
	return &json_string[0];
}

int WriteObj(char* output_path, uint32_t primitive_id, uint32_t* sub_meshes, uint32_t sub_meshes_count)
{
	prm.WriteObj(std::string(output_path), primitive_id, sub_meshes, sub_meshes_count);
	return 0;
}