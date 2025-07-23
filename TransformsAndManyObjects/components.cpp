#include "pch.h"
#include "components.h"
#include <entt/entt.hpp>

size_t GetNumberOfRenderables(const entt::registry& gRegistry) {
    auto renderables = gRegistry.view<transforms::components::Renderable>();
    size_t sz = renderables.size();
    return sz;
}

namespace transforms::components
{
    void UpdateTransformRecursive(
        entt::registry& registry,
        entt::entity entity,
        const DirectX::XMMATRIX& parentMatrix
    ) {
        using namespace transforms::components;
        auto& transform = registry.get<Transform>(entity);

        transform.parentMatrix = parentMatrix;
        transform.GetWorldMatrix(); // updates worldMatrix internally

        // Recurse through children
        if (auto* hierarchy = registry.try_get<Hierarchy>(entity)) {
            for (auto child : hierarchy->children) {
                if (registry.valid(child) && registry.all_of<Transform>(child)) {
                    UpdateTransformRecursive(registry, child, transform.worldMatrix);
                }
            }
        }
    }

    void UpdateAllTransforms(entt::registry& registry) {
        using namespace transforms::components;

        // Step 1: Iterate over all entities that have a Transform and possibly a Hierarchy
        auto view = registry.view<Transform, Hierarchy>();

        // Step 2: Find all roots (no parent or invalid parent)
        std::vector<entt::entity> roots;
        for (auto entity : view) {
            const auto& hierarchy = view.get<Hierarchy>(entity);
            if (hierarchy.parent == entt::null || !registry.valid(hierarchy.parent)) {
                roots.push_back(entity);
            }
        }

        // Step 3: Update each root and its hierarchy
        for (auto root : roots) {
            auto& rootTransform = registry.get<Transform>(root);
            rootTransform.parentMatrix = DirectX::XMMatrixIdentity();
            UpdateTransformRecursive(registry, root, rootTransform.parentMatrix);
        }

        // Step 4: Optionally handle entities without hierarchy at all (no parent, no children)
        auto orphanView = registry.view<Transform>(entt::exclude<Hierarchy>);
        for (auto orphan : orphanView) {
            auto& transform = registry.get<Transform>(orphan);
            transform.parentMatrix = DirectX::XMMatrixIdentity();
            transform.GetWorldMatrix(); // computes local * parent (identity)
        }
    }

}
