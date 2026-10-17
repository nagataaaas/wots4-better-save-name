// Tests of the label parts that need no game: reading a save, formatting, fitting and the
// row cache. Save files are synthetic and written to a fresh folder under %TEMP% only.
#include "event_data.h"
#include "label_fit.h"
#include "row_label_cache.h"
#include "save_summary.h"
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace better_save_name;

namespace {
unsigned tests = 0, failures = 0;
void check(const char* label, bool passed) {
    ++tests;
    if (!passed) ++failures;
    printf("%s %s\n", passed ? "PASS" : "FAIL", label);
}
bool same(const wchar_t* actual, const wchar_t* expected) { return !wcscmp(actual, expected); }

template <typename T> void writeAt(std::vector<uint8_t>& file, uint32_t offset, T value) {
    memcpy(file.data() + offset, &value, sizeof(value));
}
std::vector<uint8_t> makeSave(uint16_t stage, uint8_t timeOfDay, uint16_t elapsedDays, int16_t lastEventNode) {
    std::vector<uint8_t> file(saveFileSize, 0xcd);
    writeAt(file, scenarioStageOffset, stage);
    writeAt(file, timeOfDayOffset, timeOfDay);
    writeAt(file, elapsedDaysOffset, elapsedDays);
    writeAt(file, lastEventNodeOffset, lastEventNode);
    return file;
}

void testReadSummary() {
    const auto file = makeSave(120, 2, 0, 7);
    const SaveSummary summary = readSaveSummary(file.data());
    check("A save's stage, time of day, day count and last event node are read from their offsets",
        summary.scenarioStage == 120 && summary.timeOfDay == 2 && summary.elapsedDays == 0 && summary.lastEventNode == 7);
}

void testFormat() {
    const SaveSummary dayOneNight{120, 2, 0, 7};
    const LastEvent telegraph{ChartRow::foreigners, L"電信あり"};
    LabelParts parts = formatLabelParts(L"01", dayOneNight, telegraph, Language::japanese);
    check("Japanese: slot, day and time, route letter, then the event title",
        same(parts.head, L"01: 1日目夜 [外]") && same(parts.eventTitle, L"電信あり"));
    parts = formatLabelParts(L"01", dayOneNight, {ChartRow::foreigners, L"The Telegraph"}, Language::english);
    check("English: Day 1 Night [Frn] and the title", same(parts.head, L"01: Day 1 Night [Frn] ") && same(parts.eventTitle, L"The Telegraph"));
    check("English route labels are Pra, Sho, Frn and True",
        same(formatLabelParts(L"01", dayOneNight, {ChartRow::prajna, L"x"}, Language::english).head, L"01: Day 1 Night [Pra] ") &&
        same(formatLabelParts(L"01", dayOneNight, {ChartRow::shogunate, L"x"}, Language::english).head, L"01: Day 1 Night [Sho] ") &&
        same(formatLabelParts(L"01", dayOneNight, {ChartRow::trueRoute, L"x"}, Language::english).head, L"01: Day 1 Night [True] "));
    const SaveSummary dayThree{300, 0, 2, 32};
    parts = formatLabelParts(L"04", dayThree, {ChartRow::common, L"白昼の激突"}, Language::japanese);
    check("A common event has no route letter", same(parts.head, L"04: 3日目昼 ") && same(parts.eventTitle, L"白昼の激突"));
    parts = formatLabelParts(L"02", {220, 1, 1, 0}, {ChartRow::prajna, L"x"}, Language::japanese);
    check("Prajna, shogunate and true route letters", same(parts.head, L"02: 2日目夕 [攘]") &&
        same(formatLabelParts(L"02", dayThree, {ChartRow::shogunate, L"x"}, Language::japanese).head, L"02: 3日目昼 [幕]") &&
        same(formatLabelParts(L"02", dayThree, {ChartRow::trueRoute, L"x"}, Language::japanese).head, L"02: 3日目昼 [真]"));
    parts = formatLabelParts(L"00", {0, 0, 0, 78}, {ChartRow::common, L"船上の侍"}, Language::japanese);
    check("Stage 0 is shown as the prologue", same(parts.head, L"00: 序章 "));
    parts = formatLabelParts(L"00", {0, 0, 0, 78}, {ChartRow::common, nullptr}, Language::english);
    check("Without an event the label ends after the day", same(parts.head, L"00: Prologue") && !parts.eventTitle[0]);
    parts = formatLabelParts(L"03", {400, 9, 11, -1}, {ChartRow::common, nullptr}, Language::japanese);
    check("An unknown time of day is left out", same(parts.head, L"03: 12日目"));
}

void testSlotName() {
    wchar_t slot[64];
    slotNameFromFileName(slot, 64, "S4-DATA_01");
    const bool manual = same(slot, L"01");
    slotNameFromFileName(slot, 64, "S4-AUTOSAVE");
    const bool autosave = same(slot, L"AUTOSAVE");
    slotNameFromFileName(slot, 64, "PLAIN");
    const bool plain = same(slot, L"PLAIN");
    slotNameFromFileName(slot, 64, "S4-DATA_");
    check("Slot names: after '-', then after the last '_'; the whole name if nothing is left",
        manual && autosave && plain && same(slot, L"S4-DATA_"));
}

void testEventData() {
    const EventRecord records[] = {
        {7, 0, 1, ChartRow::foreigners, 2, 3, 8, 110},
        {7, 0, 1, ChartRow::common, 9, 3, 8, 110},
    };
    const EventTable table{2, 0, records};
    check("The first record of a node is found", findEventRecord(&table, 7) == &records[0]);
    const EventTable emptyTable{0, 0, records};
    check("A missing node, an empty table and no table find nothing", !findEventRecord(&table, 8) &&
        !findEventRecord(nullptr, 7) && !findEventRecord(&emptyTable, 7));
    const wchar_t* const strings[] = {L"first", L"second", L"third"};
    const GameTextTable texts{3, strings};
    check("A text id reads its text, an id past the table its first text",
        same(findGameText(&texts, 2), L"third") && same(findGameText(&texts, 40), L"first"));
    check("A negative id or no table has no text", !findGameText(&texts, -1) && !findGameText(nullptr, 1));
}

// Records shaped like EventData.l: node, ?, time, row, title, condition, event id, stage.
void testLastEvent() {
    const EventRecord records[] = {
        {78, 0, 0, ChartRow::common, 10, 0, 0, 0},  // 船上の侍
        {83, 0, 0, ChartRow::common, 11, 0, 1, 0},  // 阿弥浜港争乱
        {3, 0, 0, ChartRow::shogunate, 12, 0, 4, 100},  // 代官所の面々
        {14, 0, 0, ChartRow::shogunate, 13, 0, 14, 200},  // 病院廃業の企て
        {0, 0, 0, ChartRow::tutorial, 14, 0, 2, 999},  // A tutorial box
        {53, 0, 0, ChartRow::foreigners, 15, 0, 999, 320},  // A dummy box
    };
    const EventTable table{6, 0, records};
    uint8_t states[eventStateCount] = {};
    states[0] = 4;
    states[1] = 4;
    states[4] = 4;
    states[14] = 2;  // Declined: not reached
    states[2] = 4;
    check("A valid hint node wins", findLastEventRecord(&table, 78, states) == &records[0]);
    check("Without a hint, the reached event of the latest stage is taken",
        findLastEventRecord(&table, -1, states) == &records[2]);
    check("Tutorial and dummy boxes are not events, even as hints",
        findLastEventRecord(&table, 0, states) == &records[2] && findLastEventRecord(&table, 53, states) == &records[2]);
    states[4] = 0;
    check("In the same stage the larger event id is taken", findLastEventRecord(&table, -1, states) == &records[1]);
    states[14] = 5;
    check("A failed event counts as reached", findLastEventRecord(&table, -1, states) == &records[3]);
    const uint8_t none[eventStateCount] = {};
    check("Nothing reached and no hint finds nothing", !findLastEventRecord(&table, -1, none) &&
        !findLastEventRecord(nullptr, 78, states));
}

float tenPerCharacter(const wchar_t* text, void*) { return 10.0f * static_cast<float>(wcslen(text)); }

void testFit() {
    LabelParts parts{};
    wcscpy_s(parts.head, L"01: ");
    wcscpy_s(parts.eventTitle, L"abcdef");
    FittedLabel label = fitLabel(parts, L"~", 100, 0.5f, tenPerCharacter, nullptr);
    check("A label that fits keeps its scale", same(label.text, L"01: abcdef") && label.widthScale == 1.0f);
    label = fitLabel(parts, L"~", 80, 0.5f, tenPerCharacter, nullptr);
    check("A slightly long label is narrowed", same(label.text, L"01: abcdef") && label.widthScale == 0.8f);
    label = fitLabel(parts, L"~", 30, 0.5f, tenPerCharacter, nullptr);
    check("A label past the narrowest scale has its title cut with the ellipsis",
        same(label.text, L"01: a~") && label.widthScale == 0.5f);
    label = fitLabel(parts, L"~", 10, 0.5f, tenPerCharacter, nullptr);
    check("A head too wide alone drops the title and stays at the narrowest scale",
        same(label.text, L"01: ") && label.widthScale == 0.5f);
}

std::string fixtureFolder;
unsigned directoryCalls = 0;
bool fixtureDirectory(char* directory, size_t capacity) {
    ++directoryCalls;
    return strcpy_s(directory, capacity, fixtureFolder.c_str()) == 0;
}
Language japanese() { return Language::japanese; }
LastEvent fixtureEvent(const SaveSummary& summary) {
    if (summary.lastEventNode == 7) return {ChartRow::foreigners, L"電信あり"};
    return {ChartRow::common, nullptr};
}

void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

SaveSlotRecord makeRow(const char* label) {
    SaveSlotRecord row{};
    strcpy_s(row.label, label);
    row.hasDetails = 1;
    row.savedAt.wYear = 2026;
    row.savedAt.wMinute = 10;
    return row;
}

void testCache() {
    const auto folder = std::filesystem::temp_directory_path() /
        ("better_save_name_tests_" + std::to_string(GetCurrentProcessId()));
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    fixtureFolder = folder.string() + "\\";
    writeFile(folder / "S4-DATA_01", makeSave(120, 2, 0, 7));
    writeFile(folder / "S4-AUTOSAVE", makeSave(200, 0, 1, 1));
    writeFile(folder / "S4-DATA_02", std::vector<uint8_t>(100, 0));

    RowLabelCache cache({fixtureDirectory, japanese, fixtureEvent});
    SaveSlotRecord row = makeRow("S4-DATA_01");
    RowLabel* label = cache.find(row);
    check("A manual save's row gets its label", label && same(label->parts.head, L"01: 1日目夜 [外]") &&
        same(label->parts.eventTitle, L"電信あり") && label->language == Language::japanese && !label->fitted);

    writeFile(folder / "S4-DATA_01", makeSave(300, 1, 2, 7));
    const unsigned callsBefore = directoryCalls;
    label = cache.find(row);
    check("The same row, file and time reuse the label without reading the file again",
        label && same(label->parts.head, L"01: 1日目夜 [外]") && directoryCalls == callsBefore);
    row.savedAt.wMinute = 11;
    label = cache.find(row);
    check("A new save time reads the file again", label && same(label->parts.head, L"01: 3日目夕 [外]"));

    SaveSlotRecord autosave = makeRow("S4-AUTOSAVE");
    label = cache.find(autosave);
    check("An auto-save row named by its file gets its label, without a non-event node",
        label && same(label->parts.head, L"AUTOSAVE: 2日目昼") && !label->parts.eventTitle[0]);

    SaveSlotRecord wrongSize = makeRow("S4-DATA_02");
    check("A file of another size keeps the game's label", !cache.find(wrongSize));
    wrongSize.wrongSize = 1;
    check("A row the game marks as the wrong size keeps the game's label", !cache.find(wrongSize));
    SaveSlotRecord empty = makeRow("< Empty Slot >");
    empty.hasDetails = 0;
    check("An empty slot keeps the game's label", !cache.find(empty));
    SaveSlotRecord named = makeRow("Autosave");
    check("A row whose label is not a file keeps the game's label", !cache.find(named));
    SaveSlotRecord path = makeRow("..\\S4-DATA_01");
    check("A label with a path is not opened", !cache.find(path));

    SaveSlotRecord rows[10];
    for (auto& each : rows) each = makeRow("S4-DATA_01");
    for (auto& each : rows) cache.find(each);
    check("More rows than the cache holds still get labels", cache.find(rows[0]) && cache.find(rows[9]));
    std::filesystem::remove_all(folder);
}
}

int main() {
    testReadSummary();
    testFormat();
    testSlotName();
    testEventData();
    testLastEvent();
    testFit();
    testCache();
    printf("%u passed, %u failed. Wrote only a temporary folder under %%TEMP%%; no game folder, save or process was written.\n",
        tests - failures, failures);
    return failures ? 1 : 0;
}
