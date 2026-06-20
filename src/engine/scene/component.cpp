#include "scene/component.h"

namespace scene {

void Component::startIfNeeded()
{
    if (!started_) {
        started_ = true;
        onStart();
    }
}

} // namespace scene
