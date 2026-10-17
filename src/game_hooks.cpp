#include "game_hooks.h"
#include "event_data.h"
#include "game_code.h"
#include "row_label_cache.h"
#include <cstddef>
#include <cstring>

namespace better_save_name {
namespace {
using namespace game;

LogFunction log = nullptr;
void logMessage(const char* message) { if (log) log(message); }

// ---- What the label cache reads from the game ---------------------------------------------

bool copySaveDirectory(char* directory, size_t capacity) {
    const uintptr_t function = relocated(findSaveDirectory);
    const char* folder;
    __asm {
        call function
        mov folder, eax
    }
    return folder && strcpy_s(directory, capacity, folder) == 0;
}

const wchar_t*(WINAPI* findUiText)(uint32_t number) = nullptr;

bool hasNonAscii(const wchar_t* text) {
    for (; *text; ++text)
        if (*text > 0x7f) return true;
    return false;
}

// The texts tell which glyphs the font has: the game's own empty slot text, else the flag its
// text drawing reads (null before a game is made, when the game draws as in Japanese).
Language gameLanguage() {
    if (const wchar_t* emptySlot = findUiText ? findUiText(noDataText) : nullptr)
        return hasNonAscii(emptySlot) ? Language::japanese : Language::english;
    const uintptr_t state = read<uintptr_t>(gameState);
    return state && *reinterpret_cast<const uint8_t*>(state + gameLanguageOffset) ? Language::english : Language::japanese;
}

LastEvent findLastEvent(const SaveSummary& summary) {
    constexpr LastEvent none{ChartRow::common, nullptr};
    const EventRecord* record = findLastEventRecord(read<const EventTable*>(eventTable), summary.lastEventNode,
        summary.eventStates);
    if (!record) return none;
    const uintptr_t texts = read<uintptr_t>(textManager);
    if (!texts) return none;
    const auto* titles = *reinterpret_cast<const GameTextTable* const*>(texts + eventTextTableOffset);
    return {record->chartRow, findGameText(titles, record->titleText)};
}

RowLabelCache labels({copySaveDirectory, gameLanguage, findLastEvent});

// ---- Drawing the label --------------------------------------------------------------------

// The label's underline (FileSlot.nif) ends about 312 units after the label's left edge, well before the
// date column (measured on an in-game screenshot). 0x6833f0 measured a label about 3% narrower than it
// was drawn, so the label is fitted into less than the underline.
constexpr float labelUnderlineWidth = 312.0f;
constexpr float measuredWidthMargin = 0.95f;
constexpr float availableLabelWidth = labelUnderlineWidth * measuredWidthMargin;
static_assert(labelUnderlineWidth < dateLeft - labelLeft);
// Narrower than this is hard to read; past it the event title is shortened instead.
constexpr float minimumWidthScale = 0.7f;

using DrawFontText = void(__stdcall*)(uintptr_t fontManager, int32_t x, int32_t y, const wchar_t* text);
using MeasureUiTextWidth = float(__cdecl*)(const wchar_t* text, float scale);
uintptr_t nativeDrawAnsiText = 0;

float measureAtScale(const wchar_t* text, void* scale) {
    return reinterpret_cast<MeasureUiTextWidth>(relocated(measureUiTextWidth))(text, *static_cast<float*>(scale));
}

void setColor(uintptr_t manager, uint32_t rgba) {
    const uintptr_t function = relocated(setFontColor);
    __asm {
        mov eax, manager
        mov ecx, rgba
        call function
    }
}

// Like 0x53fa60, English text is drawn narrower than Japanese.
float nativeWidthScale() {
    const uintptr_t state = read<uintptr_t>(gameState);
    return state && *reinterpret_cast<const uint8_t*>(state + gameLanguageOffset) ? read<float>(englishWidthScale) : 1.0f;
}

RowLabel* __cdecl findRowLabel(const SaveSlotRecord* row) { return labels.find(*row); }

// Draws like 0x53fb00 (shadow, then text) with the label's own horizontal scale. The list saved the
// font state before its rows (0x53f9a0) and restores it after them, so the scale is not put back here.
void __cdecl drawRowLabel(RowLabel* label, const int32_t* position, uint32_t rgba, float scale) {
    const uintptr_t manager = read<uintptr_t>(fontManager);
    if (!manager) return;
    if (!label->fitted) {
        label->label = fitLabel(label->parts, label->language == Language::japanese ? L"…" : L"...",
            availableLabelWidth, minimumWidthScale, measureAtScale, &scale);
        label->fitted = true;
    }
    auto* fontScale = *reinterpret_cast<float**>(manager + fontResourceOffset);
    fontScale[0] = scale * nativeWidthScale() * label->label.widthScale;
    fontScale[1] = scale;
    const auto draw = reinterpret_cast<DrawFontText>(relocated(drawFontText));
    setColor(manager, shadowRgba);
    draw(manager, position[0] + shadowOffset, position[1] + shadowOffset, label->label.text);
    setColor(manager, rgba);
    draw(manager, position[0], position[1], label->label.text);
}

// Replaces the call at 0x53fd4c: ebx = row, edi = int32 {x, y}, stack = rgba, scale; ret 8.
// The caller keeps using ebx as the row after the call (0x53fd51), which the C functions preserve.
__declspec(naked) void rowLabelDrawHook() {
    __asm {
        push ebx
        call findRowLabel
        add esp, 4
        test eax, eax
        jz drawNative
        push dword ptr [esp+8]  // scale
        push dword ptr [esp+8]  // rgba
        push edi
        push eax
        call drawRowLabel
        add esp, 16
        mov al, 1  // 0x53fa60 returns 1 when it drew
        ret 8
    drawNative:
        jmp dword ptr [nativeDrawAnsiText]
    }
}

// ---- Verification -------------------------------------------------------------------------

struct CodeBytes {
    uintptr_t address;
    const uint8_t* bytes;
    uint32_t length;
};
const uint8_t rowInEbx[] = {0x8b, 0x5d, 0x0c};  // 0x53fcf4: mov ebx, [ebp+0xc]
const uint8_t labelLeftBytes[] = {0x83, 0xc2, labelLeft};  // 0x53fd3c: add edx, 0x46
const uint8_t positionInEdi[] = {0x8d, 0x7d, 0xa4};  // 0x53fd43: lea edi, [ebp-0x5c]
const uint8_t rowKeptInEbx[] = {0x8b, 0xfb};  // 0x53fd51: mov edi, ebx
const uint8_t dateLeftBytes[] = {0x8d, 0x90, 0xae, 0x01, 0x00, 0x00};  // 0x53fd6b: lea edx, [eax+0x1ae]
const uint8_t languageFlagBytes[] = {0x0f, 0xb6, 0x80, 0xce, 0x93, 0x04, 0x00};  // 0x53fa7e: movzx eax, byte [eax+0x493ce]
const uint8_t eventRecordsBytes[] = {0x8b, 0x71, 0x04};  // 0x55e635: mov esi, [ecx+4]
const uint8_t eventCountBytes[] = {0x0f, 0xbf, 0x11};  // 0x55e63c: movsx edx, word [ecx]
const uint8_t eventIdBytes[] = {0x8d, 0x4e, 0x0c};  // 0x55e645: lea ecx, [esi+0xc]
const uint8_t eventRecordSizeBytes[] = {0x83, 0xc1, 0x10};  // 0x55e651: add ecx, 0x10
const uint8_t eventNodeBytes[] = {0x66, 0x8b, 0x00};  // 0x55e662: mov ax, [eax]
const uint8_t eventTitleBytes[] = {0x0f, 0xbf, 0x42, 0x08};  // 0x668846: movsx eax, word [edx+8]
const uint8_t eventTextsBytes[] = {0x8b, 0x4f, eventTextTableOffset};  // 0x668855: mov ecx, [edi+0x20]
const CodeBytes expectedCode[] = {
    {0x53fcf4, rowInEbx, sizeof(rowInEbx)}, {0x53fd3c, labelLeftBytes, sizeof(labelLeftBytes)},
    {0x53fd43, positionInEdi, sizeof(positionInEdi)}, {0x53fd51, rowKeptInEbx, sizeof(rowKeptInEbx)},
    {0x53fd6b, dateLeftBytes, sizeof(dateLeftBytes)}, {0x53fa7e, languageFlagBytes, sizeof(languageFlagBytes)},
    {0x55e635, eventRecordsBytes, sizeof(eventRecordsBytes)}, {0x55e63c, eventCountBytes, sizeof(eventCountBytes)},
    {0x55e645, eventIdBytes, sizeof(eventIdBytes)}, {0x55e651, eventRecordSizeBytes, sizeof(eventRecordSizeBytes)},
    {0x55e662, eventNodeBytes, sizeof(eventNodeBytes)}, {0x668846, eventTitleBytes, sizeof(eventTitleBytes)},
    {0x668855, eventTextsBytes, sizeof(eventTextsBytes)},
};
static_assert(labelLeft < 0x80 && eventTextTableOffset < 0x80);  // Both are 8-bit displacements above

// The game's own calls identify the functions the Mod calls, wherever other Mods hook them.
struct CallSite {
    uintptr_t call;
    uintptr_t target;
};
const CallSite expectedCalls[] = {
    {rowLabelDrawCall, drawAnsiText}, {0x53fb56, setFontColor}, {0x53fb66, drawFontText},
    {0x667eb9, measureUiTextWidth}, {0x53e6cd, findSaveDirectory},
};

// Instructions that read a global by its absolute address, which the loader relocates.
struct GlobalRead {
    uintptr_t instruction;
    uint8_t opcode[2];
    uint32_t opcodeLength;
    uintptr_t global;
};
const GlobalRead expectedGlobals[] = {
    {0x53fa65, {0x8b, 0x35}, 2, fontManager},  // mov esi, [cpFont]
    {0x53fa75, {0xa1}, 1, gameState},  // mov eax, [game state]
    {0x53fa89, {0xd9, 0x05}, 2, englishWidthScale},  // fld dword [0.8f]
    {0x55e62b, {0x8b, 0x0d}, 2, eventTable},  // mov ecx, [event table]
    {0x668ae9, {0x8b, 0x3d}, 2, textManager},  // mov edi, [text manager]
};

constexpr uint8_t callOpcode = 0xe8;
constexpr uint32_t callLength = 5;

bool sameBytes(uintptr_t address, const uint8_t* bytes, uint32_t length) {
    return !memcmp(reinterpret_cast<const void*>(relocated(address)), bytes, length);
}

bool callsTarget(const CallSite& site) {
    const auto* instruction = reinterpret_cast<const uint8_t*>(relocated(site.call));
    const auto displacement = *reinterpret_cast<const int32_t*>(instruction + 1);
    return instruction[0] == callOpcode && site.call + callLength + displacement == site.target;
}

bool readsGlobal(const GlobalRead& expected) {
    const auto* instruction = reinterpret_cast<const uint8_t*>(relocated(expected.instruction));
    return !memcmp(instruction, expected.opcode, expected.opcodeLength) &&
        *reinterpret_cast<const uintptr_t*>(instruction + expected.opcodeLength) == relocated(expected.global);
}

bool supportedImage() {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC || pe->FileHeader.TimeDateStamp != supportedTimestamp)
        return false;
    const DWORD imageSize = pe->OptionalHeader.SizeOfImage;
    if (imageSize != supportedImageSize && imageSize != steamImageSize) return false;
    for (const auto& code : expectedCode)
        if (!sameBytes(code.address, code.bytes, code.length)) return false;
    for (const auto& site : expectedCalls)
        if (!callsTarget(site)) return false;
    for (const auto& global : expectedGlobals)
        if (!readsGlobal(global)) return false;
    return true;
}

// ---- Installation -------------------------------------------------------------------------

bool redirectCall(uintptr_t call, const void* target) {
    auto* instruction = reinterpret_cast<uint8_t*>(relocated(call));
    const auto displacement = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) -
        (reinterpret_cast<uintptr_t>(instruction) + callLength));
    DWORD previous;
    if (!VirtualProtect(instruction, callLength, PAGE_EXECUTE_READWRITE, &previous)) return false;
    memcpy(instruction + 1, &displacement, sizeof(displacement));
    DWORD unused;
    VirtualProtect(instruction, callLength, previous, &unused);
    FlushInstructionCache(GetCurrentProcess(), instruction, callLength);
    return true;
}

void useHostTexts(const wots4::HostApi& host) {
    constexpr size_t requiredSize = offsetof(wots4::HostApi, findGameText) + sizeof(host.findGameText);
    findUiText = host.size >= requiredSize ? host.findGameText : nullptr;
}
}

bool installGameHooks(const wots4::ModContext& context, LogFunction logger) {
    base = reinterpret_cast<uintptr_t>(context.game);
    log = logger;
    if (!supportedImage()) {
        logMessage("[ERROR] Could not initialize Better Save Name: game version or code does not match; initialization stopped.");
        return false;
    }
    if (!context.host->claim(context.module, relocated(rowLabelDrawCall), callLength)) {
        logMessage("[ERROR] Could not reserve the save list's label drawing at 0x53fd4c: another Mod owns it; "
            "initialization stopped.");
        return false;
    }
    useHostTexts(*context.host);
    nativeDrawAnsiText = relocated(drawAnsiText);
    if (redirectCall(rowLabelDrawCall, reinterpret_cast<const void*>(rowLabelDrawHook))) return true;
    context.host->releaseClaims(context.module);
    logMessage("[ERROR] Could not redirect the save list's label drawing: memory protection failed; initialization stopped.");
    return false;
}
}
