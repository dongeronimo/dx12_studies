#include "pch.h"
#include "on_esc_handler.h"
#include "components.h"
#include "direct3d_context.h"
#include "game_window.h"
void transforms::components::CreateEscHandler(entt::registry& gRegistry,
	transforms::Context* ctx,
	transforms::Window* gWindow)
{
	entt::entity onEscHandler = gRegistry.create();
	{
		transforms::components::Script onEscScript;
		onEscScript.execute = [&ctx, &gWindow](float deltaTime, entt::entity self, entt::registry& registry) {
			if (gWindow->GetLastKey() == transforms::KeyCodes::Esc) {
				if (MessageBox(0, L"Are you sure you want to exit?",
					L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES) {
					ctx->debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);//TODO crash: why crashing here?
					DestroyWindow(gWindow->Hwnd());
				}
			}
			};
		gRegistry.emplace<transforms::components::Script>(onEscHandler, onEscScript);
	}
}
