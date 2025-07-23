#pragma once
#include <entt/entt.hpp>
namespace transforms {
	class Context;
	class Window;
}

namespace transforms::components {
    void CreateEscHandler(entt::registry& gRegistry,
		transforms::Context* ctx,
		transforms::Window* gWindow);
}