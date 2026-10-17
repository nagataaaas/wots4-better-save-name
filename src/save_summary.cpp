#include "save_summary.h"
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>

namespace better_save_name {
namespace {
constexpr uint16_t prologueStage = 0;

template <typename T> T readAt(const uint8_t* fileStart, uint32_t offset) {
    T value;
    memcpy(&value, fileStart + offset, sizeof(value));
    return value;
}

const wchar_t* timeOfDayName(uint8_t timeOfDay, Language language) {
    static const wchar_t* const japanese[] = {L"昼", L"夕", L"夜"};
    static const wchar_t* const english[] = {L" Morning", L" Evening", L" Night"};
    if (timeOfDay >= std::size(japanese)) return L"";
    return language == Language::japanese ? japanese[timeOfDay] : english[timeOfDay];
}

void formatDay(wchar_t* day, size_t capacity, const SaveSummary& summary, Language language) {
    const bool japanese = language == Language::japanese;
    if (summary.scenarioStage == prologueStage) {
        wcscpy_s(day, capacity, japanese ? L"序章" : L"Prologue");
        return;
    }
    const unsigned dayNumber = summary.elapsedDays + 1u;
    const wchar_t* time = timeOfDayName(summary.timeOfDay, language);
    if (japanese) swprintf_s(day, capacity, L"%u日目%s", dayNumber, time);
    else swprintf_s(day, capacity, L"Day %u%s", dayNumber, time);
}

// The route in brackets, for the rows that belong to a route: one kanji in Japanese, a short word in English.
const wchar_t* routeMarker(ChartRow row, Language language) {
    const bool japanese = language == Language::japanese;
    switch (row) {
    case ChartRow::prajna: return japanese ? L"[攘]" : L"[Pra] ";
    case ChartRow::shogunate: return japanese ? L"[幕]" : L"[Sho] ";
    case ChartRow::foreigners: return japanese ? L"[外]" : L"[Frn] ";
    case ChartRow::trueRoute: return japanese ? L"[真]" : L"[True] ";
    default: return L"";
    }
}
}

SaveSummary readSaveSummary(const uint8_t* fileStart) {
    SaveSummary summary{readAt<uint16_t>(fileStart, scenarioStageOffset), readAt<uint8_t>(fileStart, timeOfDayOffset),
        readAt<uint16_t>(fileStart, elapsedDaysOffset), readAt<int16_t>(fileStart, lastEventNodeOffset)};
    memcpy(summary.eventStates, fileStart + eventStatesOffset, sizeof(summary.eventStates));
    return summary;
}

LabelParts formatLabelParts(const wchar_t* slotName, const SaveSummary& summary, const LastEvent& lastEvent,
    Language language) {
    LabelParts parts{};
    wchar_t day[32];
    formatDay(day, std::size(day), summary, language);
    if (!lastEvent.title || !*lastEvent.title) {
        swprintf_s(parts.head, L"%s: %s", slotName, day);
        return parts;
    }
    swprintf_s(parts.head, L"%s: %s %s", slotName, day, routeMarker(lastEvent.chartRow, language));
    // Longer titles are cut here and shortened again to the row's width when drawn.
    wcsncpy_s(parts.eventTitle, lastEvent.title, _TRUNCATE);
    return parts;
}

void slotNameFromFileName(wchar_t* slotName, size_t capacity, const char* fileName) {
    const char* slot = fileName;
    if (const char* hyphen = strchr(slot, '-')) slot = hyphen + 1;
    if (const char* underscore = strrchr(slot, '_')) slot = underscore + 1;
    if (!*slot) slot = fileName;
    size_t length = 0;
    // File names of saves are ASCII; anything else is not a slot name worth showing.
    for (; slot[length] && length + 1 < capacity; ++length)
        slotName[length] = static_cast<unsigned char>(slot[length]) < 0x80 ? slot[length] : L'?';
    slotName[length] = L'\0';
}
}
