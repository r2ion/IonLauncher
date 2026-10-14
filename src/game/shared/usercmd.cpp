#include "game/shared/usercmd.h"
#include "engine/server/server_limits.h"

DECLARE_MODULE(UserCmdHooks)
// clang-format off
DECLARE_HOOK(ReadUserCmd, server.dll + 0x2603F0,
([](auto& hook, void* buf, void* pCmd_move, void* pCmd_from) // 4C 89 44 24 ? 53 55 56 57
// clang-format on
{
	// Let normal usercmd read happen first, it's safe
	hook.Original(buf, pCmd_move, pCmd_from);

	// Now let's make sure the CMD we read isnt messed up to prevent numerous exploits (including server crashing)

	auto cmd = (SV_CUserCmd*)pCmd_move;
	auto fromCmd = (SV_CUserCmd*)pCmd_from;

	std::string BLOCK_PREFIX =
		"ReadUserCmd (command_number delta: " + std::to_string(cmd->command_number - fromCmd->command_number) + "): ";

	// fix invalid player angles
	if (!cmd->worldViewAngles.IsValid())
		cmd->worldViewAngles.Init();

	if (!cmd->attackangles.IsValid())
		cmd->attackangles.Init();

	if (!cmd->localViewAngles.IsValid())
		cmd->localViewAngles.Init();

	// Fix invalid camera angles
	if (!cmd->cameraPos.IsValid())
		cmd->cameraPos.Init();
	if (!cmd->cameraAngles.IsValid())
		cmd->cameraAngles.Init();

	// Fix invaid movement vector
	if (!cmd->move.IsValid())
		cmd->move.Init();

	if (cmd->frameTime <= 0 || cmd->tick_count == 0 || cmd->command_time <= 0)
	{
		BLOCKED_INFO(
			"Bogus cmd timing (tick_count: " << cmd->tick_count << ", frameTime: " << cmd->frameTime
											 << ", commandTime : " << cmd->command_time << ")");
		goto INVALID_CMD; // No simulation of bogus-timed cmds
	}

	return;

INVALID_CMD:

	// Fix any gameplay-affecting cmd properties
	// NOTE: Currently tickcount/frametime is set to 0, this ~shouldn't~ cause any problems
	cmd->worldViewAngles = cmd->localViewAngles = cmd->attackangles = cmd->cameraAngles = {0, 0, 0};
	cmd->tick_count = cmd->frameTime = 0;
	cmd->move = cmd->cameraPos = {0, 0, 0};
	cmd->buttons = 0;
	cmd->meleetarget = 0;
}))

ON_DLL_LOAD_RELIESON("server.dll", ServerUserCmd, ServerLimits, [](CModule)
{
    DISPATCH_MODULE(UserCmdHooks)
})
