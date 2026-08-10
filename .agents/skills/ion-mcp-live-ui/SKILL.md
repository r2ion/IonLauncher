---
name: ion-mcp-live-ui
description: Smoke-test Northstar and Ion UI changes end to end in a real Titanfall 2 session through MCP and physical input.
---

# Ion MCP live UI smoke workflow

Use after changing Northstar UI scripts, RUI resources, dynamic textures, ModWorkshop APIs, or menu-driven install/remove behavior. Read `ion-mcp-session` and `ion-mcp-squirrel` first.

## Prepare

1. Resolve the runtime from `NS_BINARY_DIR` in `config.cmake`.
2. Stop only the game process owned by the task, then build `NorthstarDLL`; the configured output directory receives `Northstar.dll` directly.
3. Launch a windowed game on an unused MCP port through the harness process manager.
4. Wait for `/health`, initialize one owned MCP session, and confirm the `ui` VM with a harmless `execute_squirrel` probe.

## Exercise the user path

- Navigate to the target menu through normal UI where practical. Squirrel may establish setup state, but it must not replace the interaction being tested.
- For nested `RuiButton` controls, use real Win32 cursor input:
  1. bring the Titanfall window to the foreground;
  2. convert client coordinates with `ClientToScreen`;
  3. move the cursor with `SetCursorPos`;
  4. send left-down/left-up with `mouse_event`.
- Posted `WM_LBUTTON*` messages can update hover while missing the nested action. Do not use them as proof of a click.
- Reacquire control coordinates after menu transitions or list rerenders.
- Capture the game window with `PrintWindow` and inspect the actual confirmation, progress, error, and stable post-operation UI.

A button's physical left click reaches its `UIE_CLICK` handler before the global input callback. Handle LMB through one path; wiring both `UIE_CLICK` and `IE_ButtonPressed` for the same action creates races. When a modal must open from an input callback, defer by a frame and keep one shared pending/modal latch until the dialog has genuinely left the active-menu state.

## Assert behavior, not setup

- Verify the final menu state and a second interaction where repeatability matters.
- Verify MCP state with an independent call; do not stop at `execute_squirrel` returning success.
- For ModWorkshop operations, poll `NSMWSGetOperationState()`. State `9` is `Done`; check the expected action and a fresh operation generation.
- For removal, use only a disposable package and verify both `NSGetModInformation` and the package root on disk no longer contain it.
- For internal reload, inspect the newest `<profile>/logs/nslog*.txt` for a fresh mod scan and `RPAK_FSYS Reloading RPaks on next map load`.
- For dynamic RUI images or atlases, render and scroll the real UI. Successful resource creation or upload alone is not a rendering smoke test.

## Finish

Return the UI to a stable state, close any test-owned dialog, clear the owned MCP session, and stop the owned game process. Stop the game before the final native build so the deployed DLL cannot remain locked.
