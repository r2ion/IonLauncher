#pragma once

#include "materialsystem/imaterial.h"

class IMaterialInternal : public IMaterial
{
public:
	virtual bool IsErrorMaterial() const override = 0;
};
static_assert(sizeof(IMaterialInternal) == sizeof(void*));
