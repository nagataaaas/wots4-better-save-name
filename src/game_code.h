#pragma once
#include <windows.h>
#include <cstdint>

// Addresses of the supported Steam executable at its preferred base 0x400000.
namespace better_save_name::game {
constexpr uintptr_t preferredImageBase = 0x400000;
constexpr DWORD supportedTimestamp = 0x683d42eb;
constexpr DWORD supportedImageSize = 0x65b000;
// The Steam executable keeps its DRM wrapper section, so the running image is larger than the decrypted copy.
constexpr DWORD steamImageSize = 0x689000;

// The save list draws each row with 0x53fce0 (ecx = list, edx = row position, stack = index, row; ebx = row).
// Its first line starts with the row's label, drawn by this call to the ANSI text drawer.
constexpr uintptr_t rowLabelDrawCall = 0x53fd4c;  // call 0x53fa60
constexpr uintptr_t drawAnsiText = 0x53fa60;  // ebx = ANSI text, edi = int32 {x, y}, stack = rgba, scale; ret 8
// Game functions the Mod calls, found through the game's own calls to them.
constexpr uintptr_t setFontColor = 0x4c8740;  // eax = font manager, ecx = 0xRRGGBBAA
constexpr uintptr_t drawFontText = 0x4c7dc0;  // stdcall (font manager, x, y, UTF-16 text)
constexpr uintptr_t measureUiTextWidth = 0x6833f0;  // cdecl (UTF-16 text, scale) -> width in st0
constexpr uintptr_t findSaveDirectory = 0x4bddc0;  // Returns the folder of the saves, ending with a separator

// Globals.
constexpr uintptr_t fontManager = 0x9ecf1c;
constexpr uintptr_t gameState = 0x9e967c;  // Everything a save holds (cpGParamMng); null before a game is made
constexpr uint32_t gameLanguageOffset = 0x493ce;  // 0 when the game is in Japanese (read by 0x53fa7e)
constexpr uintptr_t englishWidthScale = 0x989804;  // 0.8f: the game narrows its English text by this
constexpr uint32_t fontResourceOffset = 4;  // The font manager's resource starts with the x and y scale
constexpr uintptr_t eventTable = 0x9eb5e8;  // EventTable* (read by 0x55e62b)
constexpr uintptr_t textManager = 0x9e9680;
constexpr uint32_t eventTextTableOffset = 0x20;  // GameTextTable* of the event titles (0x668855)

// Row layout of 0x53fce0: the label at x + 0x46 (0x53fd3c), the date at x + 0x1ae (0x53fd6b).
constexpr int32_t labelLeft = 0x46;
constexpr int32_t dateLeft = 0x1ae;
// 0x53fb00 draws its shadow one pixel lower right in this color before the text.
constexpr uint32_t shadowRgba = 0x1f;
constexpr int32_t shadowOffset = 1;

constexpr uint32_t noDataText = 0x336;  // GameTextUI shown for an empty slot; its language is the game's

inline uintptr_t base;
inline uintptr_t relocated(uintptr_t address) { return base + address - preferredImageBase; }
template <typename T> T read(uintptr_t address) { return *reinterpret_cast<const T*>(relocated(address)); }
}
