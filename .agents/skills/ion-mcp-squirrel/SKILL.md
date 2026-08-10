---
name: ion-mcp-squirrel
description: Discover and execute Titanfall 2 Squirrel functions safely through the Ion MCP tools.
---

# Ion MCP Squirrel workflow

Read `ion-mcp-session` first and retain one session. Use this skill for function discovery, VM probes, console commands, and Squirrel execution.

## Choose the VM deliberately

| Context | Use for | Availability |
| --- | --- | --- |
| `ui` | Menus, dialogs, RUI handles, `uiGlobal` | Front-end/UI VM must exist. |
| `client` | Local gameplay, client entities and effects | Client VM usually requires a loaded map. |
| `server` | Authoritative gameplay, server entities and rules | Server VM requires a local/server map; absent on a front-end-only client. |

A dedicated server has no `client` or `ui` VM. Call `get_game_state` first, then use a harmless context-specific probe if VM availability is uncertain.

## Available tools

- `get_game_state`: structured connection, map, sign-on, and dedicated state.
- `search_script_functions`: native registration documentation plus live GameFS `.nut`/`.gnut` definitions.
- `execute_squirrel`: compile and call code in one selected VM.
- `execute_console_command`: queue a real engine console command.
- `get_console_log`: latest 50 lines from Northstar's ring buffer.

Do not route Squirrel through a console command when `execute_squirrel` expresses the operation directly.

## Discover before executing

1. Search the selected context by function name or description.
2. Inspect `structuredContent.results`, not only the rendered text.
3. Follow `page.nextCursor` while `page.hasMore` is true.
4. Set `include_source_matches=true` only when function-name search is insufficient; source scanning can return up to `source_match_limit` snippets.
5. Execute the smallest idempotent probe that establishes the needed state.

`search_script_functions` combines three sources:

- live `SQFuncRegistration` records observed while a VM registers globals;
- static client/server registration catalogs recovered from the loaded DLLs;
- function definitions parsed from live GameFS scripts.

A result with `native: true` uses the registration's `helpText` as `description`. For example, UI search for `Hud_SetColor` should return:

```text
Sets the color of an element with components (red, green, blue, alpha); given as 0-255 values.
```

A result with `native: false` and a description such as `<script function defined at ... line ...>` has no matched native documentation string. Preserve its `definition.path` and `definition.line`; do not present that placeholder as semantic documentation or invent a description.

## Execute and read the result correctly

`execute_squirrel` arguments:

```json
{
  "script": "printt(\"ION_MCP_PROBE\")",
  "context": "ui",
  "capture_output": true
}
```

- `isError` and `structuredContent.success` are the execution result.
- `structuredContent.compile_result` and optional `call_result` distinguish compilation from runtime failure.
- Captured text is diagnostic output, not the success oracle.
- Output capture is capped at 500 lines and waits up to 10 seconds for its end marker. A capture timeout does not by itself prove the Squirrel call failed.
- Set `capture_output=false` when no console evidence is needed.

The server serializes requests and owns one shared capture stream. Keep calls sequential; do not overlap output-capturing operations.

## Mutation discipline

For state-changing calls:

1. Read the precondition through `get_game_state` or a Squirrel probe.
2. Execute one mutation.
3. Read the changed state independently.
4. For UI behavior, perform the real user interaction and visual check described by `ion-mcp-live-ui`; a successful script call is not proof that click, focus, modal, or rendering behavior works.

Use `get_console_log` and the newest profile `nslog*.txt` to diagnose failures. Prefer a unique marker in probe output so unrelated game logs cannot be mistaken for the result.
