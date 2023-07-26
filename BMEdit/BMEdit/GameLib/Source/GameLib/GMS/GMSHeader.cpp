#include <GameLib/GMS/GMSHeader.h>
#include <GameLib/GMS/GMSStructureError.h>
#include <GameLib/GMS/GMSSectionOffsets.h>
#include <GameLib/BinaryReaderSeekScope.h>
#include <GameLib/TypeRegistry.h>
#include <GameLib/TypeComplex.h>
#include <ZBinaryReader.hpp>

#include <sstream>
#include <array>


namespace gamelib::gms
{
	GMSHeader::GMSHeader() = default;

	GMSEntries &GMSHeader::getEntries()
	{
		return m_geomEntities;
	}

	const GMSEntries &GMSHeader::getEntries() const
	{
		return m_geomEntities;
	}

	const GMSGeomStats &GMSHeader::getGeomStats() const
	{
		return m_geomStats;
	}

	const GMSGroupsCluster &GMSHeader::getGeomClusters() const
	{
		return m_geomClusters;
	}

	void GMSHeader::deserialize(GMSHeader &header, ZBio::ZBinaryReader::BinaryReader *gmsFileReader, ZBio::ZBinaryReader::BinaryReader *bufFileReader)
	{
		//TODO: https://github.com/ReGlacier/ReHitmanTools/issues/3#issuecomment-769654029

		{
			// Validate format
			BinaryReaderSeekScope rootScope { gmsFileReader };
			gmsFileReader->seek(4);

			static constexpr std::array<uint32_t, 3> kExpectedSignature = { 0, 0, 4 };
			std::array<uint32_t, 3> sections = { 0, 0, 0 };

			gmsFileReader->read<uint32_t, ZBio::Endianness::LE>(sections.data(), 3);

			if (sections != kExpectedSignature)
			{
				// Invalid format, stop deserialization
				throw GMSStructureError("Invalid GMS format: expected 0x4=0, 0x8=0, 0xC=4");
			}
		}

		{
			// Check physics data tag (for Hitman Blood Money should be 0xFFFFFFFFu)
			BinaryReaderSeekScope physicsScope { gmsFileReader };
			gmsFileReader->seek(GMSSectionOffsets::LEGACY_PHYSICS_DATA);

			constexpr uint32_t kValidPhysicsOffset = 0xFFFFFFFFu;

			const auto physicsOffset = gmsFileReader->read<uint32_t, ZBio::Endianness::LE>();
			if (physicsOffset != kValidPhysicsOffset && physicsOffset >= gmsFileReader->size())
			{
				throw GMSStructureError("Invalid GMS format: unsupported pointer to physics data (expected 0xFFFFFFFF)");
			}
		}

		{
			// Read clusters
			BinaryReaderSeekScope depthLevelsScope { gmsFileReader };
			gmsFileReader->seek(GMSSectionOffsets::CLUSTER_INFO);

			const auto clustersRegionOffset = gmsFileReader->read<uint32_t, ZBio::Endianness::LE>();
			gmsFileReader->seek(clustersRegionOffset);

			GMSGroupsCluster::deserialize(header.m_geomClusters, gmsFileReader);
		}

		{
			// Read Entities
			BinaryReaderSeekScope entitiesScope { gmsFileReader };
			gmsFileReader->seek(GMSSectionOffsets::ENTITIES);

			const auto geomTableOffset = gmsFileReader->read<uint32_t, ZBio::Endianness::LE>();
			gmsFileReader->seek(geomTableOffset);

			GMSEntries::deserialize(header.m_geomEntities, gmsFileReader, bufFileReader);
		}

		{
			// Read GeomStats
			BinaryReaderSeekScope entitiesScope { gmsFileReader };
			gmsFileReader->seek(GMSSectionOffsets::GEOM_STATS);

			const auto geomStatsOffset = gmsFileReader->read<uint32_t, ZBio::Endianness::LE>();
			gmsFileReader->seek(geomStatsOffset);

			GMSGeomStats::deserialize(header.m_geomStats, gmsFileReader);
		}

		{
			// Build hierarchy
			GMSHeader::buildSceneHierarchy(header);
		}
	}

	struct CachedRuntimeTypes
	{
		const Type *ZSHAPE { nullptr };
		const GeomBasedTypeInfo *ZSHAPE_Runtime { nullptr };

		const Type *ZSTDOBJ { nullptr };
		const GeomBasedTypeInfo *ZSTDOBJ_Runtime { nullptr };

		const Type *ZBOUND { nullptr };
		const GeomBasedTypeInfo *ZBOUND_Runtime { nullptr };

		const Type *ZSNDOBJ { nullptr };
		const GeomBasedTypeInfo *ZSNDOBJ_Runtime { nullptr };

		const Type *ZGROUP { nullptr };
		const GeomBasedTypeInfo *ZGROUP_Runtime { nullptr };

		const Type *ZLIGHT { nullptr };
		const GeomBasedTypeInfo *ZLIGHT_Runtime { nullptr };

		CachedRuntimeTypes();

		[[nodiscard]] explicit operator bool() const noexcept;
	};

	uint8_t GetBaseGeomListType(const CachedRuntimeTypes *cachedRuntimeTypes, GMSGeomEntity *entity)
	{
		if (!entity) {
			return 0;
		}

		std::stringstream s;
		s << "0x" << std::uppercase << std::hex << entity->getTypeId();
		auto tp = TypeRegistry::getInstance().findTypeByHash(s.str());
		if (!tp || tp->getKind() != TypeKind::COMPLEX || !reinterpret_cast<const TypeComplex *>(tp)->hasGeomInfo())
		{
			return 3;
		}

		const auto &entityTypeRuntime = reinterpret_cast<const TypeComplex *>(tp)->getGeomInfo();
		const auto entityTypeId = entityTypeRuntime.getId();

		if ((cachedRuntimeTypes->ZSHAPE_Runtime->getMask() & entityTypeId) == cachedRuntimeTypes->ZSHAPE_Runtime->getId())
		{
			return 1;
		}

		if ((cachedRuntimeTypes->ZSTDOBJ_Runtime->getMask() & entityTypeId) == cachedRuntimeTypes->ZSTDOBJ_Runtime->getId())
		{
			if ((cachedRuntimeTypes->ZBOUND_Runtime->getMask() & entityTypeId) == cachedRuntimeTypes->ZBOUND_Runtime->getId())
			{
				return 3;
			}

			if ((cachedRuntimeTypes->ZSNDOBJ_Runtime->getMask() & entityTypeId) != cachedRuntimeTypes->ZSNDOBJ_Runtime->getId())
			{
				return 1;
			}

			return 3;
		}
		else if ((cachedRuntimeTypes->ZGROUP_Runtime->getMask() & entityTypeId) == cachedRuntimeTypes->ZGROUP_Runtime->getId())
		{
			return 0;
		}
		else
		{
			if ((cachedRuntimeTypes->ZLIGHT_Runtime->getMask() & entityTypeId) != cachedRuntimeTypes->ZLIGHT_Runtime->getId())
			{
				return 3;
			}
		}

		return 2;
	}

	void GMSHeader::buildSceneHierarchy(GMSHeader &header)
	{
		std::vector<GMSGeomEntity*> currentPath {};
		currentPath.resize(1);
		currentPath.reserve(128);

		auto& entities = header.getEntries().m_entities;
		currentPath[0] = &entities[0];

		for (int i = 1; i < entities.size(); ++i)
		{
			auto& geom = entities[i];

			size_t rd = geom.getRelativeDepthLevel();
			size_t newLength = currentPath.size() - rd;

			// build new path
			std::vector<GMSGeomEntity*> newPath { currentPath.begin(), currentPath.begin() + static_cast<std::ptrdiff_t>(newLength) };

			geom.m_geomIndex = &entities[i] - &entities[0];
			// save parent
			geom.m_parentGeomIndex = newPath.back() - &entities[0];

			// save new path
			currentPath = std::move(newPath);

			if (geom.isRootOfGroup())
			{
				currentPath.push_back(&entities[i]);
			}
		}
	}

	CachedRuntimeTypes::CachedRuntimeTypes()
	{
		auto &rttiRegistry = TypeRegistry::getInstance();

#define IS_VALID_TYPE_INSTANCE(x) ((x) && (x->getKind() == TypeKind::COMPLEX) && reinterpret_cast<const TypeComplex *>((x))->hasGeomInfo())

		ZSHAPE = rttiRegistry.findTypeByName("ZSHAPE");
		if (IS_VALID_TYPE_INSTANCE(ZSHAPE)) {
			ZSHAPE_Runtime = &reinterpret_cast<const TypeComplex *>(ZSHAPE)->getGeomInfo();
		}

		ZSTDOBJ = rttiRegistry.findTypeByName("ZSTDOBJ");
		if (IS_VALID_TYPE_INSTANCE(ZSTDOBJ)) {
			ZSTDOBJ_Runtime = &reinterpret_cast<const TypeComplex *>(ZSTDOBJ)->getGeomInfo();
		}

		ZBOUND = rttiRegistry.findTypeByName("ZBOUND");
		if (IS_VALID_TYPE_INSTANCE(ZBOUND)) {
			ZBOUND_Runtime = &reinterpret_cast<const TypeComplex *>(ZBOUND)->getGeomInfo();
		}

		ZSNDOBJ = rttiRegistry.findTypeByName("ZSNDOBJ");
		if (IS_VALID_TYPE_INSTANCE(ZSNDOBJ)) {
			ZSNDOBJ_Runtime = &reinterpret_cast<const TypeComplex *>(ZSNDOBJ)->getGeomInfo();
		}

		ZGROUP = rttiRegistry.findTypeByName("ZGROUP");
		if (IS_VALID_TYPE_INSTANCE(ZGROUP)) {
			ZGROUP_Runtime = &reinterpret_cast<const TypeComplex *>(ZGROUP)->getGeomInfo();
		}

		ZLIGHT = rttiRegistry.findTypeByName("ZLIGHT");
		if (IS_VALID_TYPE_INSTANCE(ZLIGHT)) {
			ZLIGHT_Runtime = &reinterpret_cast<const TypeComplex *>(ZLIGHT)->getGeomInfo();
		}

#undef IS_VALID_TYPE_INSTANCE
	}

	CachedRuntimeTypes::operator bool() const noexcept
	{
		return ZSHAPE && ZSHAPE_Runtime
			&& ZSTDOBJ && ZSTDOBJ_Runtime
			&& ZBOUND && ZBOUND_Runtime
			&& ZSNDOBJ && ZSNDOBJ_Runtime
			&& ZGROUP && ZGROUP_Runtime
			&& ZLIGHT && ZLIGHT_Runtime;
	}
}