#pragma once

#include "vscript/languages/squirrel_re/include/squirrel.h"
#include "vscript/languages/squirrel_re/squirrel/sqobject.h"

struct alignas(8) SQTable : public SQDelegable
{
	struct _HashNode
	{
		SQObject val;
		SQObject key;
		_HashNode* next;
	};

	_HashNode* _nodes;
	int _numOfNodes;
	int size;
	int field_48;
	int _usedNodes;
};
static_assert(std::is_base_of_v<SQDelegable, SQTable>);
static_assert(sizeof(SQTable::_HashNode) == 0x28);
static_assert(offsetof(SQTable::_HashNode, val) == 0x0);
static_assert(offsetof(SQTable::_HashNode, key) == 0x10);
static_assert(offsetof(SQTable::_HashNode, next) == 0x20);
static_assert(sizeof(SQTable) == 0x50);
static_assert(offsetof(SQTable, _nodes) == 0x38);
static_assert(offsetof(SQTable, _numOfNodes) == 0x40);
static_assert(offsetof(SQTable, size) == 0x44);
static_assert(offsetof(SQTable, field_48) == 0x48);
static_assert(offsetof(SQTable, _usedNodes) == 0x4C);
