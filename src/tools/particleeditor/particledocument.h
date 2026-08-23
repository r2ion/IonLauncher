#pragma once
#include "datamodel/dmattributetypes.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ParticleTools
{

std::string FormatObjectId(const DmObjectId_t& id);
bool ParseObjectId(std::string_view text, DmObjectId_t& id);

struct DmxAttribute
{
	std::string m_Name;
	DmAttributeType_t m_Type = AT_STRING;
	std::string m_Value;
	std::vector<DmObjectId_t> m_ElementIds;

	bool IsElementReference() const;
	bool IsArray() const;
};

struct DmxElement
{
	DmObjectId_t m_Id;
	std::string m_Type;
	std::string m_Name;
	std::vector<DmxAttribute> m_Attributes;

	DmxAttribute* FindAttribute(std::string_view name);
	const DmxAttribute* FindAttribute(std::string_view name) const;
};

class ParticleDocument
{
public:
	static ParticleDocument CreateEmpty();

	bool Load(const std::filesystem::path& path, std::string& error);
	bool Save(const std::filesystem::path& path, std::string& error) const;
	std::vector<DmxElement>& Elements();
	const std::vector<DmxElement>& Elements() const;
	DmxElement* Root();
	const DmxElement* Root() const;
	DmxElement* FindElement(const DmObjectId_t& id);
	const DmxElement* FindElement(const DmObjectId_t& id) const;
	DmxElement& CreateElement(std::string type, std::string name);
	bool RemoveElement(const DmObjectId_t& id);

	bool ValidateAndCanonicalize(std::string& error);
	static bool CanonicalizeAttribute(DmxAttribute& attribute, std::string& error);

private:
	std::vector<DmxElement> m_Elements;
};

} // namespace ParticleTools
