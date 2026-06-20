#pragma once

class Time
{
public:
    // Called once per frame with the real delta time in seconds.
    void update(float deltaSeconds);

    float deltaTime() const { return deltaTime_; }
    float elapsed() const { return elapsed_; }

private:
    float deltaTime_ = 0.0f;
    float elapsed_ = 0.0f;
};
