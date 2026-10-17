#pragma once
#include "save_summary.h"
#include <iterator>

namespace better_save_name {
// Width of text drawn at the native scale, in the list's 2D coordinates.
using MeasureText = float (*)(const wchar_t* text, void* context);

struct FittedLabel {
    wchar_t text[std::size(LabelParts{}.head) + std::size(LabelParts{}.eventTitle) + 4];
    float widthScale;  // Horizontal scale on top of the native one, in [minimumWidthScale, 1]
};

// Fits head + eventTitle into availableWidth: first by narrowing the text down to minimumWidthScale,
// then by cutting the end of the event title and adding ellipsis. The head is never cut; a head
// too wide alone is narrowed as much as allowed and may overflow.
FittedLabel fitLabel(const LabelParts& parts, const wchar_t* ellipsis, float availableWidth, float minimumWidthScale,
    MeasureText measure, void* context);
}
