#include "pch.h"
#include "script_runner_system.h"
#include <entt/entt.hpp>
#include "components.h"
#include "../Common/delta_timer.h"

namespace transforms::systems {
    void RunScripts(entt::registry& gRegistry,
        const float deltaTime) {
        auto scriptViews = gRegistry.view<transforms::components::Script>();
        scriptViews.each([deltaTime, &gRegistry]
        (entt::entity entity, transforms::components::Script script) {
            script.execute(deltaTime, entity, gRegistry);
        });
    }
}