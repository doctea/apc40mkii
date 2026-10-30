#include "APCControl.h"

void APCControl::applyAPCEvent(float v) {
    value = v;
    if (onAPCValueChanged) {
        onAPCValueChanged(v);
    }
}

void APCControl::setAppValue(float v) {
    value = v;
    if (onAppValueChanged) {
        onAppValueChanged(v);
    }
}
