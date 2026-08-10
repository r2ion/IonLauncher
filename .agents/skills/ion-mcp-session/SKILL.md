---
name: ion-mcp-session
description: Start, connect to, retain, and recover a Titanfall 2 Ion MCP HTTP session for this repository.
---

# Ion MCP session workflow

Use this skill before any live `execute_squirrel`, console, game-state, script-search, or UI-smoke work.

## Source of truth

- The server implementation is `src/engine/mcp_server.cpp` and `src/engine/mcp_server.h`.
- The runtime root is `NS_BINARY_DIR` in `config.cmake`; do not assume the game path.
- The default endpoint is `http://127.0.0.1:8765/` and the protocol version is `2025-06-18`.
- The listener is loopback-only. `GET /health` proves the listener is alive, not that a session or a requested Squirrel VM is ready.

## Start a disposable session

1. Check the intended port before launching. Do not take over a live endpoint owned by another agent or user.
2. If native code changed, stop the game before building so `Northstar.dll` is not locked. Build the configured target:
   ```text
   cmake --build build --config Debug --target NorthstarDLL
   ```
   Adapt the configuration to the active build; `NorthstarDLL` writes directly to `NS_BINARY_DIR`.
3. Start the game with the harness process manager, never as an untracked shell process. Use the runtime root as `cwd` and preserve the user's profile/launch arguments. A typical isolated launch is:
   ```text
   NorthstarLauncher.exe --profile=R2Northstar --windowed --noborder -multiple +mcp_start_http 8766
   ```
4. Wait for both `GET http://127.0.0.1:<port>/health` and the log line `Northstar MCP server listening on ...`.

Use a separate unused port for an isolated smoke test. Port `8765` is commonly owned by an interactive session.

## Initialize exactly once

The server supports one current session ID. Every successful `initialize` call replaces it, immediately invalidating another client's session. Reuse a supplied session ID; do not initialize merely to probe a shared endpoint.

For a session you own, send:

```http
POST /
Content-Type: application/json
MCP-Protocol-Version: 2025-06-18

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"ion-agent","version":"1"}}}
```

Retain the `Mcp-Session-Id` response header and send it, plus `MCP-Protocol-Version`, on every later request. Then send the normal initialized notification:

```json
{"jsonrpc":"2.0","method":"notifications/initialized","params":{}}
```

HTTP `202` with an empty body is the expected notification response.

## Call tools

Use JSON-RPC `tools/call`:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/call",
  "params": {
    "name": "get_game_state",
    "arguments": {}
  }
}
```

Prefer a configured MCP client when one is available. The raw HTTP flow is the recovery path and protocol oracle.

## Diagnose session failures

- `400 Missing session ID`: add the retained `Mcp-Session-Id` header.
- `404 Session not found`: the server restarted or another client initialized. Reinitialize only if you own the endpoint; otherwise stop and coordinate.
- `400 Unsupported protocol version`: send `2025-06-18`.
- Health succeeds but a UI/client/server call fails: the requested VM is not created yet; this is not a transport failure.
- A launcher process can exit while the loaded game process remains. Treat port and runtime log state as authoritative, and stop the process tree through the harness when finished.

## Teardown

For an owned session, `DELETE /` clears the MCP session. Stop the owned game process before a final build. Never stop or relaunch a game process that predates the task or is serving another client.
