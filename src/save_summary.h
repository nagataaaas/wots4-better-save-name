#pragma once
#include "event_data.h"
#include <windows.h>
#include <cstddef>
#include <cstdint>

namespace better_save_name {
constexpr DWORD saveFileSize = 0x48bb0;  // Every save file, manual or not, has this size

// Offsets in a save file, from its start.
constexpr uint32_t scenarioStageOffset = 0x10;  // u16: 0 in the prologue (and after an ending), 100 on day 1 ...
constexpr uint32_t timeOfDayOffset = 0x14;  // u8: 0 daytime, 1 evening, 2 night
constexpr uint32_t elapsedDaysOffset = 0xbd8;  // u16: 0 on day 1
constexpr uint32_t eventStatesOffset = 0x36a8;  // u8 per event id
// i16: the event chart node of the event the player last advanced (accepted or finished), -1 for none.
// The game picks its "what to do next" hint from it.
constexpr uint32_t lastEventNodeOffset = 0x3900;
constexpr uint32_t summaryByteCount = lastEventNodeOffset + sizeof(int16_t);

struct SaveSummary {
    uint16_t scenarioStage;
    uint8_t timeOfDay;
    uint16_t elapsedDays;
    int16_t lastEventNode;
    uint8_t eventStates[eventStateCount];
};

// fileStart holds at least summaryByteCount bytes from the start of a save file.
SaveSummary readSaveSummary(const uint8_t* fileStart);

enum class Language : uint8_t { japanese, english };

// The last event as the event chart names it: its row and its title in the game's language.
struct LastEvent {
    ChartRow chartRow;
    const wchar_t* title;  // nullptr when the save has none or the master data is not loaded
};

// The label of a row, in two parts so that a long event title can be shortened alone:
// head is "01: 1日目夜 [外]" and eventTitle "電信あり" (empty when there is no event).
struct LabelParts {
    wchar_t head[48];
    wchar_t eventTitle[128];
};
LabelParts formatLabelParts(const wchar_t* slotName, const SaveSummary& summary, const LastEvent& lastEvent,
    Language language);

// The slot part of a file name: after the first '-' and then after the last '_'
// ("S4-DATA_01" is "01", "S4-AUTOSAVE" is "AUTOSAVE"). The whole name if that leaves nothing.
void slotNameFromFileName(wchar_t* slotName, size_t capacity, const char* fileName);
}
