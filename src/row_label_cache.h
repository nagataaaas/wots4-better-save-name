#pragma once
#include "label_fit.h"
#include "save_slot_record.h"

namespace better_save_name {
// What the cache needs from the game, so that tests can provide it without one.
struct GameAccess {
    bool (*copySaveDirectory)(char* directory, size_t capacity);  // Ends with a separator
    Language (*language)();
    LastEvent (*findLastEvent)(const SaveSummary& summary);
};

struct RowLabel {
    const SaveSlotRecord* row;
    char fileName[sizeof(SaveSlotRecord::label)];
    SYSTEMTIME savedAt;
    bool decorated;  // False keeps the game's label (not a save file, or it could not be read)
    bool fitted;  // Set by the drawer once label holds parts fitted to the row
    Language language;
    LabelParts parts;
    FittedLabel label;
};

// The labels of the rows the save list draws, made once per file and kept while the row shows the same
// file at the same time. The list draws every frame, so a hit only compares the row with the entry.
// Game thread only.
class RowLabelCache {
public:
    explicit RowLabelCache(const GameAccess& game) : game(game) {}
    // The label to draw instead of the row's, or nullptr to draw the game's own.
    RowLabel* find(const SaveSlotRecord& row);

private:
    static constexpr size_t capacity = 8;  // The Load list has six rows with Autosave, five otherwise
    RowLabel* entryFor(const SaveSlotRecord& row);
    void make(RowLabel& entry, const SaveSlotRecord& row);

    GameAccess game;
    RowLabel entries[capacity] = {};
    size_t nextReplaced = 0;
};
}
