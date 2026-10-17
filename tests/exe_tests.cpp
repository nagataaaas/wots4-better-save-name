// Initializes mod.dll against a mapped copy of the decrypted executable, then calls the redirected
// label drawing the way 0x53fce0 does, with the game's font, text and save folder functions replaced
// by local stubs. No game startup, renderer or game save folder is used.
#include "mapped_exe.h"
#include "save_slot_record.h"
#include <cwchar>
#include <filesystem>

namespace {
using namespace mapped_exe;
using better_save_name::SaveSlotRecord;

bool refuseClaims = false;
BOOL WINAPI claim(HMODULE owner, uintptr_t address, uint32_t length) {
    if (refuseClaims) return FALSE;
    return hostClaim(owner, address, length);
}
BOOL WINAPI addPage(HMODULE, const wots4::LocalizedText*, const wots4::OptionItem*, uint32_t,
    const wots4::OptionsPageCallbacks*) { return FALSE; }
BOOL WINAPI onFrame(HMODULE, wots4::GameEventCallback, void*) { return FALSE; }
BOOL WINAPI onSave(HMODULE, wots4::GameEventCallback, void*) { return FALSE; }
const wchar_t* emptySlotText = L"< 空のスロット >";
const wchar_t* WINAPI findGameText(uint32_t number) { return number == 0x336 ? emptySlotText : nullptr; }

bool insideModule(HMODULE module, unsigned address) {
    MEMORY_BASIC_INFORMATION info;
    return VirtualQuery(reinterpret_cast<void*>(address), &info, sizeof(info)) && info.AllocationBase == module;
}
unsigned callTarget(unsigned preferredCall) {
    return va(preferredCall) + 5 + *reinterpret_cast<int*>(va(preferredCall) + 1);
}

// ---- Stubs of the game functions the hook calls ---------------------------------------------

struct DrawnText {
    int x, y;
    std::wstring text;
    uint32_t color;
    float xScale, yScale;
};
std::vector<DrawnText> drawn;
uint32_t currentColor = 0;
unsigned nativeDraws = 0;
float fontResource[16] = {};
uintptr_t fontManager[2] = {0, reinterpret_cast<uintptr_t>(fontResource)};

__declspec(naked) void setColorStub() {
    __asm {
        mov currentColor, ecx
        ret
    }
}
void __stdcall drawTextStub(uintptr_t manager, int x, int y, const wchar_t* text) {
    if (manager == reinterpret_cast<uintptr_t>(fontManager))
        drawn.push_back({x, y, text, currentColor, fontResource[0], fontResource[1]});
}
float characterWidth = 8.0f;
float __cdecl measureStub(const wchar_t* text, float scale) { return characterWidth * scale * static_cast<float>(wcslen(text)); }
__declspec(naked) void nativeDrawStub() {
    __asm {
        inc nativeDraws
        mov al, 1
        ret 8
    }
}

void writeJump(unsigned preferred, const void* target) {
    auto* code = reinterpret_cast<unsigned char*>(va(preferred));
    code[0] = 0xe9;
    *reinterpret_cast<int*>(code + 1) = static_cast<int>(reinterpret_cast<uintptr_t>(target) - (va(preferred) + 5));
}

// Calls the redirected call at 0x53fd4c with its inputs: ebx = row, edi = position, stack = rgba, scale.
bool callLabelDraw(const SaveSlotRecord* row, const int* position, uint32_t rgba, float scale) {
    const unsigned target = callTarget(0x53fd4c);
    uint32_t scaleBits;
    memcpy(&scaleBits, &scale, sizeof(scaleBits));
    unsigned ebxAfter, ediAfter, stackBefore, stackAfter;
    __asm {
        push ebx
        push edi
        mov ebx, row
        mov edi, position
        mov stackBefore, esp
        push scaleBits
        push rgba
        call target
        mov stackAfter, esp
        mov ebxAfter, ebx
        mov ediAfter, edi
        pop edi
        pop ebx
    }
    return ebxAfter == reinterpret_cast<unsigned>(row) && ediAfter == reinterpret_cast<unsigned>(position) &&
        stackAfter == stackBefore;
}

template <typename T> void writeAt(std::vector<char>& file, unsigned offset, T value) { memcpy(file.data() + offset, &value, sizeof(value)); }
void writeSave(const std::filesystem::path& path, unsigned short stage, unsigned char time, unsigned short days, short node,
    int succeededEvent = -1) {
    std::vector<char> file(0x48bb0, 0);
    writeAt(file, 0x10, stage);
    writeAt(file, 0x14, time);
    writeAt(file, 0xbd8, days);
    writeAt(file, 0x3900, node);
    if (succeededEvent >= 0) file[0x36a8 + succeededEvent] = 4;
    std::ofstream(path, std::ios::binary).write(file.data(), file.size());
}
SaveSlotRecord makeRow(const char* label) {
    SaveSlotRecord row{};
    strcpy_s(row.label, label);
    row.hasDetails = 1;
    return row;
}

#pragma pack(push, 1)
struct EventRecord { short node, unknown, timeOfDay, chartRow, titleText, conditionText, eventId, stage; };
struct EventTable { short count, padding; const EventRecord* records; };
struct TextTable { unsigned count; const wchar_t* const* strings; };
#pragma pack(pop)

void testLabelDraw(const std::filesystem::path& fixture) {
    const std::string folder = fixture.string() + "\\";
    writeSave(fixture / "S4-DATA_01", 120, 2, 0, 7);
    writeSave(fixture / "S4-AUTOSAVE", 210, 1, 1, 7);
    // Like a save after sleeping: the game cleared the hint node, event 8 succeeded earlier.
    writeSave(fixture / "S4-DATA_03", 400, 0, 3, -1, 8);
    unsigned char directoryStub[] = {0xb8, 0, 0, 0, 0, 0xc3};  // mov eax, folder; ret
    *reinterpret_cast<uintptr_t*>(directoryStub + 1) = reinterpret_cast<uintptr_t>(folder.c_str());
    memcpy(reinterpret_cast<void*>(va(0x4bddc0)), directoryStub, sizeof(directoryStub));
    writeJump(0x4c8740, setColorStub);
    writeJump(0x4c7dc0, drawTextStub);
    writeJump(0x6833f0, measureStub);
    writeJump(0x53fa60, nativeDrawStub);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(imageBase), 0x65b000);

    // The master data: node 7 is a foreigners' event whose title is text 2.
    const EventRecord records[] = {{5, 0, 1, 3, 1, 0, 9, 110}, {7, 0, 1, 5, 2, 0, 8, 110}};
    const EventTable events{2, 0, records};
    const wchar_t* const titles[] = {L"first", L"other", L"電信あり"};
    const TextTable titleTable{3, titles};
    uintptr_t textManager[9] = {};
    textManager[8] = reinterpret_cast<uintptr_t>(&titleTable);  // +0x20
    *reinterpret_cast<const void**>(va(0x9eb5e8)) = &events;
    *reinterpret_cast<void**>(va(0x9e9680)) = textManager;
    *reinterpret_cast<void**>(va(0x9ecf1c)) = fontManager;

    const int position[2] = {0x1ea + 0x46, 0x7c + 0xa};
    SaveSlotRecord manual = makeRow("S4-DATA_01");
    check("The redirected call keeps ebx, edi and the stack as the native drawer does",
        callLabelDraw(&manual, position, 0xff, 1.0f));
    check("A manual save's label is drawn twice: shadow, then text", drawn.size() == 2 && nativeDraws == 0);
    if (drawn.size() == 2) {
        check("The label comes from the save and the event master data", drawn[1].text == L"01: 1日目夜 [外]電信あり");
        check("The shadow is one pixel lower right in the shadow color", drawn[0].x == position[0] + 1 &&
            drawn[0].y == position[1] + 1 && drawn[0].color == 0x1f && drawn[0].text == drawn[1].text);
        check("The text is at the row's position in the row's color", drawn[1].x == position[0] &&
            drawn[1].y == position[1] && drawn[1].color == 0xff);
        check("A label that fits is drawn at the native scale", drawn[1].xScale == 1.0f && drawn[1].yScale == 1.0f);
    }
    drawn.clear();
    callLabelDraw(&manual, position, 0x3f, 1.0f);
    check("The next frame draws the same label from the cache", drawn.size() == 2 &&
        drawn[1].text == L"01: 1日目夜 [外]電信あり" && drawn[1].color == 0x3f);
    drawn.clear();
    SaveSlotRecord slept = makeRow("S4-DATA_03");
    callLabelDraw(&slept, position, 0xff, 1.0f);
    check("Without a hint node the last reached event comes from the event states", drawn.size() == 2 &&
        drawn[1].text == L"03: 4日目昼 [外]電信あり");

    // English: the empty slot text and the game's language flag both say English.
    emptySlotText = L"< Empty Slot >";
    std::vector<unsigned char> gameState(0x493d0, 0);
    gameState[0x493ce] = 1;
    *reinterpret_cast<void**>(va(0x9e967c)) = gameState.data();
    drawn.clear();
    SaveSlotRecord autosave = makeRow("S4-AUTOSAVE");
    callLabelDraw(&autosave, position, 0xff, 1.0f);
    const float englishWidth = *reinterpret_cast<float*>(va(0x989804));
    check("An auto-save named by its file is labeled in English", drawn.size() == 2 &&
        drawn[1].text == L"AUTOSAVE: Day 2 Evening [Frn] 電信あり");
    check("English text is narrowed like the game's (0.8)", drawn.size() == 2 && englishWidth == 0.8f &&
        drawn[1].xScale == englishWidth && drawn[1].yScale == 1.0f);

    // 26 characters at 12 units are 312, past the 296.4 units left of the label's underline.
    characterWidth = 12.0f;
    drawn.clear();
    SaveSlotRecord wide = makeRow("S4-DATA_01");
    callLabelDraw(&wide, position, 0xff, 1.0f);
    const float labelWidth = 12.0f * 26.0f;
    check("A label wider than the underline is narrowed to end before it", drawn.size() == 2 &&
        drawn[1].text == L"01: Day 1 Night [Frn] 電信あり" && drawn[1].yScale == 1.0f &&
        drawn[1].xScale < englishWidth && labelWidth * drawn[1].xScale / englishWidth <= 312.0f * 0.95f + 0.01f);
    characterWidth = 8.0f;

    drawn.clear();
    SaveSlotRecord named = makeRow("Autosave");
    callLabelDraw(&named, position, 0xff, 1.0f);
    SaveSlotRecord empty = makeRow("< Empty Slot >");
    empty.hasDetails = 0;
    check("A row that is not a save keeps the game's drawing", callLabelDraw(&empty, position, 0xff, 1.0f) &&
        nativeDraws == 2 && drawn.empty());

    *reinterpret_cast<void**>(va(0x9e967c)) = nullptr;
    *reinterpret_cast<void**>(va(0x9eb5e8)) = nullptr;
    *reinterpret_cast<void**>(va(0x9e9680)) = nullptr;
    *reinterpret_cast<void**>(va(0x9ecf1c)) = nullptr;
}
}

int main(int argc, char** argv) {
    if (argc < 3 || !mapImage(argv[1])) return 2;
    const std::string scenario = argc > 3 ? argv[3] : "valid";
    refuseClaims = scenario == "conflict";
    const unsigned nativeTarget = callTarget(0x53fd4c);

    const auto module = LoadLibraryA(argv[2]);
    const auto initialize = module ? reinterpret_cast<wots4::Initialize>(GetProcAddress(module, "Wots4ModInitialize")) : nullptr;
    check("The x86 DLL exports Wots4ModInitialize", initialize != nullptr);
    if (!initialize) return 3;
    wots4::HostApi host{sizeof(host), wots4::modApiVersion, hostLog, claim, hostRelease, addPage, onFrame, onSave, findGameText};
    wots4::ModContext context{sizeof(context), wots4::modApiVersion, nullptr, module,
        reinterpret_cast<HMODULE>(imageBase), L"", &host};
    check("A null context is refused", initialize(nullptr) == FALSE);
    const BOOL accepted = initialize(&context);
    if (scenario == "conflict") {
        check("Initialization is refused when the call is reserved by another Mod", accepted == FALSE);
        check("The label call still goes to the game's drawer", callTarget(0x53fd4c) == nativeTarget &&
            nativeTarget == va(0x53fa60));
        check("The refusal is logged", loggedText("another Mod owns it"));
    } else {
        check("Initialization succeeds", accepted == TRUE);
        check("Only the label call is reserved", claims.size() == 1 && claimed(0x53fd4c, 5));
        check("The label call goes into mod.dll", insideModule(module, callTarget(0x53fd4c)));
        const auto fixture = std::filesystem::absolute(argv[0]).parent_path() / "better_save_name_exe_fixture";
        std::filesystem::remove_all(fixture);
        std::filesystem::create_directories(fixture);
        testLabelDraw(fixture);
        std::filesystem::remove_all(fixture);
    }
    printf("RESULT: %u checks passed, %u failed\n", tests - failures, failures);
    puts("Only a mapped executable copy, a fixture folder next to the test and local stubs were used; no game was started.");
    puts("No writes to the installation, saves or game process.");
    return failures ? 1 : 0;
}
