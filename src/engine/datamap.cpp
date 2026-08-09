#include "datamap.h"

typedescription_t* DataMapHandler::FindFieldInDataMap(datamap_t* map, std::string_view fieldName)
{
	while (map)
	{
		for (std::int32_t index = 0; index < map->dataNumFields; ++index)
		{
			typedescription_t& description = map->dataDesc[index];
			if (description.fieldName && fieldName == description.fieldName)
				return &description;

			if (description.fieldType == FIELD_EMBEDDED && description.td)
			{
				if (typedescription_t* field = FindFieldInDataMap(description.td, fieldName))
					return field;
			}
		}

		map = map->baseMap;
	}

	return nullptr;
}

std::int32_t DataMapHandler::FindOffsetForField(datamap_t* map, std::string_view fieldName)
{
	const typedescription_t* field = FindFieldInDataMap(map, fieldName);
	return field ? field->fieldOffset : 0;
}
