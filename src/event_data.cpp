#include "event_data.h"

namespace better_save_name {
namespace {
// Event states as the scripts write them (save 0x36a8 + id).
constexpr uint8_t eventAccepted = 3;
constexpr uint8_t eventSucceeded = 4;
constexpr uint8_t eventFailed = 5;

// A box that names an event of the chart: not a tutorial box and not a dummy id.
bool isChartEvent(const EventRecord& record) {
    return record.chartRow != ChartRow::tutorial && record.eventId >= 0 && record.eventId < eventStateCount;
}

bool reached(uint8_t state) { return state == eventAccepted || state == eventSucceeded || state == eventFailed; }

bool laterThan(const EventRecord& record, const EventRecord& other) {
    if (record.scenarioStage != other.scenarioStage) return record.scenarioStage > other.scenarioStage;
    return record.eventId > other.eventId;
}
}

const EventRecord* findEventRecord(const EventTable* table, int16_t node) {
    if (!table || !table->records) return nullptr;
    for (int16_t index = 0; index < table->recordCount; ++index)
        if (table->records[index].node == node) return &table->records[index];
    return nullptr;
}

const EventRecord* findLastEventRecord(const EventTable* table, int16_t hintNode, const uint8_t* eventStates) {
    const EventRecord* hinted = findEventRecord(table, hintNode);
    if (hinted && isChartEvent(*hinted)) return hinted;
    if (!table || !table->records) return nullptr;
    const EventRecord* latest = nullptr;
    for (int16_t index = 0; index < table->recordCount; ++index) {
        const EventRecord& record = table->records[index];
        if (!isChartEvent(record) || !reached(eventStates[record.eventId])) continue;
        if (!latest || laterThan(record, *latest)) latest = &record;
    }
    return latest;
}

const wchar_t* findGameText(const GameTextTable* table, int16_t id) {
    if (!table || !table->strings || id < 0) return nullptr;
    const auto index = static_cast<uint32_t>(id);
    return table->strings[index < table->count ? index : 0];
}
}
