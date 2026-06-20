#pragma once

#include <cstdint>

struct ObjectId
{
    uint64_t value = 0;

    bool operator==(ObjectId other) const { return value == other.value; }
    bool operator!=(ObjectId other) const { return value != other.value; }
    bool isValid() const { return value != 0; }
};
