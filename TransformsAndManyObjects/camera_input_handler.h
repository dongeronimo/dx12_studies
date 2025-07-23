#pragma once
#include <entt/entt.hpp>
namespace transforms {
    class Window;
}
namespace transforms::components {
    void CreateCameraInputHandler(transforms::Window* gWindow, entt::registry& gRegistry,
        entt::entity mainCamera);
}