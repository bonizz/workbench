#include "core/time.h"

void Time::update(float deltaSeconds)
{
    deltaTime_ = deltaSeconds;
    elapsed_ += deltaSeconds;
}
