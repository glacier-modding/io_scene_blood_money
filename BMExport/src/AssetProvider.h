#pragma once

#include "GameLib/IO/IOLevelAssetsProvider.h"
#include "GameLib/Span.h"
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>

class AssetProvider : public gamelib::io::IOLevelAssetsProvider
{
public:
	struct Asset
	{
		std::vector<char> data;
	};

	explicit AssetProvider();
	~AssetProvider() override;

	bool LoadData(char* buf_data, uint32_t buf_data_size, char* gms_data, uint32_t gms_data_size, char* mat_data, uint32_t mat_data_size, char* prm_data, uint32_t prm_data_size, char* prp_data, uint32_t prp_data_size, char* tex_data, uint32_t tex_data_size);
	std::unordered_map<gamelib::io::AssetKind, Asset> GetAssets();
	bool SetAssets(std::unordered_map<gamelib::io::AssetKind, Asset> assets);
	char* GetData(gamelib::io::AssetKind kind);
	uint32_t GetDataSize(gamelib::io::AssetKind kind);

	// Read API
	[[nodiscard]] const std::string &getLevelName() const override;
	[[nodiscard]] const uint8_t* getAsset(gamelib::io::AssetKind kind, int64_t &bufferSize) const override;
	[[nodiscard]] bool hasAssetOfKind(gamelib::io::AssetKind kind) const override;

	// Write API
	bool saveAsset(gamelib::io::AssetKind kind, gamelib::Span<uint8_t> assetBody) override;

	// Etc
	[[nodiscard]] bool isEditable() const override;
	[[nodiscard]] bool isValid() const override;

private:
	std::unordered_map<gamelib::io::AssetKind, Asset> assets;
};