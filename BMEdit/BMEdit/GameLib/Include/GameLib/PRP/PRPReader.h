#pragma once

#include <GameLib/PRP/PRPHeader.h>
#include <GameLib/PRP/PRPTokenTable.h>
#include <GameLib/PRP/PRPZDefines.h>
#include <GameLib/PRP/PRPByteCode.h>
#include <cstdint>
#include <vector>


namespace gamelib::prp {
	class PRPReader
	{
	public:
		PRPReader() = default;

		bool parse(const uint8_t *prpFile, int64_t prpFileSize);

		[[nodiscard]] const PRPHeader &getHeader() const;
		[[nodiscard]] const PRPTokenTable &getTokenTable() const;
		[[nodiscard]] uint32_t getObjectsCount() const;
		[[nodiscard]] const PRPZDefines &getDefinitions() const;
		[[nodiscard]] const PRPByteCode &getByteCode() const;

	private:
		PRPHeader m_header {};
		PRPTokenTable m_tokenTable {};
		uint32_t m_objectsCount { 0 };
		PRPZDefines m_ZDefines {};
		PRPByteCode m_byteCode {};
	};
}