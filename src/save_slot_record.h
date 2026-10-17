#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace better_save_name {
// One row of the save list, as 0x53f380 fills it (0x94 bytes each).
#pragma pack(push, 1)
struct SaveSlotRecord {
    // File name of an existing save, which is both shown and used to load (0x53eac8),
    // or a text in the game's ANSI code page.
    char label[0x40];
    uint8_t wrongSize;  // Drawn dimmed (0x53fd25)
    uint8_t hasDetails;  // Shows the date, description and difficulty below
    SYSTEMTIME savedAt;  // Local time of the last write
    char description[0x40];
    uint8_t difficulty;
    uint8_t padding;
};
#pragma pack(pop)
static_assert(sizeof(SaveSlotRecord) == 0x94);
static_assert(offsetof(SaveSlotRecord, wrongSize) == 0x40 && offsetof(SaveSlotRecord, savedAt) == 0x42 &&
    offsetof(SaveSlotRecord, difficulty) == 0x92);
}
