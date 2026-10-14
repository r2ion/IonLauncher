#pragma once

struct DTColumnData
{
    const char* name;
    int type;
    int byteOffset;
};

struct DataTableHeader
{
    int columnCount;
    int rowCount;
    DTColumnData* columnData;
    char* rows;
    int rowSize;
};
