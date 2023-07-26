#include "AssetProvider.h"

static constexpr std::string_view kAssetExtensions[gamelib::io::AssetKind::LAST_ASSET_KIND] = {
	"GMS", "PRP", "TEX", "PRM", "MAT", "OCT", "RMI", "RMC", "LOC", "ANM", "SND", "BUF", "ZGF"
};

AssetProvider::AssetProvider() = default;

AssetProvider::~AssetProvider() = default;

bool AssetProvider::LoadData(char* buf_data, uint32_t buf_data_size, char* gms_data, uint32_t gms_data_size, char* mat_data, uint32_t mat_data_size, char* prm_data, uint32_t prm_data_size, char* prp_data, uint32_t prp_data_size, char* tex_data, uint32_t tex_data_size)
{
	std::unordered_map<gamelib::io::AssetKind, Asset>().swap(assets);
	assets[gamelib::io::AssetKind::BUFFER].data = std::vector<char>(buf_data, buf_data + buf_data_size);
	assets[gamelib::io::AssetKind::SCENE].data = std::vector<char>(gms_data, gms_data + gms_data_size);
	assets[gamelib::io::AssetKind::MATERIALS].data = std::vector<char>(mat_data, mat_data + mat_data_size);
	assets[gamelib::io::AssetKind::GEOMETRY].data = std::vector<char>(prm_data, prm_data + prm_data_size);
	assets[gamelib::io::AssetKind::PROPERTIES].data = std::vector<char>(prp_data, prp_data + prp_data_size);
	assets[gamelib::io::AssetKind::TEXTURES].data = std::vector<char>(tex_data, tex_data + tex_data_size);
	return true;
}

std::unordered_map<gamelib::io::AssetKind, AssetProvider::Asset> AssetProvider::GetAssets()
{
	return assets;
}

bool AssetProvider::SetAssets(std::unordered_map<gamelib::io::AssetKind, AssetProvider::Asset> assets)
{
	this->assets = assets;
	return true;
}

char* AssetProvider::GetData(gamelib::io::AssetKind kind)
{
	if (assets.contains(kind))
	{
		return assets[kind].data.data();
	}
}

uint32_t AssetProvider::GetDataSize(gamelib::io::AssetKind kind)
{
	if (assets.contains(kind))
	{
		return (uint32_t)assets[kind].data.size();
	}
}

const uint8_t* AssetProvider::getAsset(gamelib::io::AssetKind kind, int64_t &bufferSize) const
{
	if (assets.contains(kind))
	{
		bufferSize = static_cast<int64_t>(assets.at(kind).data.size());
		return reinterpret_cast<const uint8_t*>(assets.at(kind).data.data());
	}

	return nullptr;
}

const std::string &AssetProvider::getLevelName() const
{
	return "";
}

bool AssetProvider::hasAssetOfKind(gamelib::io::AssetKind kind) const
{
	if (assets.contains(kind))
	{
		return true;
	}
	return false;
}

bool AssetProvider::saveAsset(gamelib::io::AssetKind kind, gamelib::Span<uint8_t> assetBody)
{
	return false;
}

bool AssetProvider::isValid() const
{
	return true;
}

bool AssetProvider::isEditable() const
{
	return true;
}