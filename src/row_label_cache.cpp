#include "row_label_cache.h"
#include <cstdio>
#include <cstring>
#include <iterator>

namespace better_save_name {
namespace {
// The label is a file name only for a row with a usable save; a name with a path or wildcard in it
// is not one of the save folder's files.
bool isPlainFileName(const char* label, size_t capacity) {
    const size_t length = strnlen(label, capacity);
    return length > 0 && length < capacity && !strpbrk(label, "\\/:*?\"<>|");
}

bool readSummary(const char* path, SaveSummary& summary) {
    const HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    // Static: the cache runs on the game thread only, and the stack of a draw call is no place for 14 KB.
    static uint8_t fileStart[summaryByteCount];
    DWORD read = 0;
    const bool usable = GetFileSize(file, nullptr) == saveFileSize &&
        ReadFile(file, fileStart, sizeof(fileStart), &read, nullptr) && read == sizeof(fileStart);
    CloseHandle(file);
    if (usable) summary = readSaveSummary(fileStart);
    return usable;
}
}

RowLabel* RowLabelCache::find(const SaveSlotRecord& row) {
    if (!row.hasDetails || row.wrongSize) return nullptr;
    RowLabel* entry = entryFor(row);
    return entry->decorated ? entry : nullptr;
}

RowLabel* RowLabelCache::entryFor(const SaveSlotRecord& row) {
    for (auto& entry : entries) {
        if (entry.row != &row) continue;
        if (!strncmp(entry.fileName, row.label, sizeof(entry.fileName)) &&
            !memcmp(&entry.savedAt, &row.savedAt, sizeof(entry.savedAt))) return &entry;
        make(entry, row);
        return &entry;
    }
    RowLabel& entry = entries[nextReplaced];
    nextReplaced = (nextReplaced + 1) % capacity;
    make(entry, row);
    return &entry;
}

void RowLabelCache::make(RowLabel& entry, const SaveSlotRecord& row) {
    entry = {};
    entry.row = &row;
    memcpy(entry.fileName, row.label, sizeof(entry.fileName));
    entry.savedAt = row.savedAt;
    if (!isPlainFileName(row.label, sizeof(row.label))) return;
    char directory[MAX_PATH], path[MAX_PATH];
    if (!game.copySaveDirectory(directory, sizeof(directory)) || sprintf_s(path, "%s%s", directory, entry.fileName) < 0)
        return;
    SaveSummary summary;
    if (!readSummary(path, summary)) return;
    wchar_t slotName[sizeof(SaveSlotRecord::label)];
    slotNameFromFileName(slotName, std::size(slotName), entry.fileName);
    entry.language = game.language();
    entry.parts = formatLabelParts(slotName, summary, game.findLastEvent(summary), entry.language);
    entry.decorated = true;
}
}
