#pragma once
#include <chrono>

namespace common {
    /// <summary>
    /// Utility class to calculate delta time.
    /// </summary>
    class DeltaTimer {
    public:
        DeltaTimer() {
            lastTime = std::chrono::high_resolution_clock::now();
        }
        /// <summary>
        /// Should be called once per frame because every time it's called it'll
        /// give a different dt since it is recalculated every time.
        /// </summary>
        /// <returns></returns>
        float GetDelta() {
            auto nowTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> delta = nowTime - lastTime;
            float deltaTime = delta.count(); // Now it's in seconds
            lastTime = nowTime;
            return deltaTime;
        }
    private:
        std::chrono::high_resolution_clock::time_point lastTime;
    };
}