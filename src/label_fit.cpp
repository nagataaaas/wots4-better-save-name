#include "label_fit.h"
#include <cwchar>
#include <iterator>

namespace better_save_name {
namespace {
void compose(FittedLabel& label, const LabelParts& parts, size_t titleLength, const wchar_t* ellipsis) {
    swprintf_s(label.text, L"%s%.*s%s", parts.head, static_cast<int>(titleLength), parts.eventTitle, ellipsis);
}

float widthScaleFor(float width, float availableWidth, float minimumWidthScale) {
    if (width <= availableWidth) return 1.0f;
    const float scale = availableWidth / width;
    return scale < minimumWidthScale ? minimumWidthScale : scale;
}
}

FittedLabel fitLabel(const LabelParts& parts, const wchar_t* ellipsis, float availableWidth, float minimumWidthScale,
    MeasureText measure, void* context) {
    FittedLabel label{};
    const size_t titleLength = wcslen(parts.eventTitle);
    compose(label, parts, titleLength, L"");
    float width = measure(label.text, context);
    if (width * minimumWidthScale <= availableWidth || !titleLength) {
        label.widthScale = widthScaleFor(width, availableWidth, minimumWidthScale);
        return label;
    }
    // Runs only when the label is made (the list opened or a save changed), never per frame.
    for (size_t kept = titleLength - 1; kept > 0; --kept) {
        compose(label, parts, kept, ellipsis);
        width = measure(label.text, context);
        if (width * minimumWidthScale <= availableWidth) {
            label.widthScale = widthScaleFor(width, availableWidth, minimumWidthScale);
            return label;
        }
    }
    compose(label, parts, 0, L"");
    label.widthScale = widthScaleFor(measure(label.text, context), availableWidth, minimumWidthScale);
    return label;
}
}
