#include "widgets.h"

bool pointInRect(float px, float py, float x, float y, float w, float h) {
    return px >= x && py >= y && px < x + w && py < y + h;
}
