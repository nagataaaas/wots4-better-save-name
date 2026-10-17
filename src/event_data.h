#pragma once
#include <cstdint>

namespace better_save_name {
// The event chart's master data (Common/Script/EventData.l), which the game loads at startup
// and keeps at [0x9eb5e8]. One record per box of the chart; an event may have several boxes.
enum class ChartRow : int16_t {
    tutorial = 2,
    prajna = 3,
    shogunate = 4,
    foreigners = 5,
    common = 6,  // Also the boxes of the unaffiliated
    trueRoute = 7,
};

#pragma pack(push, 1)
struct EventRecord {
    int16_t node;  // The number the save keeps as its last advanced event (0x3900)
    int16_t unknown;
    int16_t timeOfDay;  // -1 when any
    ChartRow chartRow;
    int16_t titleText;  // In the event text table (the chart shows this title, 0x668846)
    int16_t conditionText;
    int16_t eventId;  // Index of the event's state in the save (0x36a8 + id)
    int16_t scenarioStage;
};
struct EventTable {
    int16_t recordCount;
    int16_t padding;
    const EventRecord* records;
};
// A table of the game's texts, like GameTextUI: the count, then the strings.
struct GameTextTable {
    uint32_t count;
    const wchar_t* const* strings;
};
#pragma pack(pop)
static_assert(sizeof(EventRecord) == 0x10 && sizeof(EventTable) == 8);

constexpr int16_t eventStateCount = 166;  // Event ids index the save's 166 event states; others are dummies

// The first record of node, searched like 0x661f60. nullptr if the table is not loaded or has no such node.
const EventRecord* findEventRecord(const EventTable* table, int16_t node);

// The box of the event the player reached last, or nullptr if none.
// hintNode is the save's last advanced node (0x3900). The game clears it to -1 whenever the time of day
// passes (0x50fb50, 0x50fbf0, 0x50fc70) and when an event is declined (0x510132), so without it the event
// is the reached one (state accepted, succeeded or failed) of the latest stage, then of the largest id.
// eventStates holds the save's eventStateCount states (0x36a8).
const EventRecord* findLastEventRecord(const EventTable* table, int16_t hintNode, const uint8_t* eventStates);

// The text like the game reads it (0x668855): an id past the table falls back to its first text.
// nullptr for a negative id or a table that is not loaded.
const wchar_t* findGameText(const GameTextTable* table, int16_t id);
}
