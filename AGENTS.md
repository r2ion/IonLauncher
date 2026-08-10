# Repository agent guidance

These instructions apply to the entire repository. Follow `STANDARDS.md` for project code style and the more specific instructions below for agent-driven work.

## Repository skills

Read the matching repository skill before live Titanfall work:

- `.agents/skills/ion-mcp-session/SKILL.md` — start, connect, retain, and recover an Ion MCP session.
- `.agents/skills/ion-mcp-squirrel/SKILL.md` — discover and execute Squirrel functions.
- `.agents/skills/ion-mcp-live-ui/SKILL.md` — verify UI behavior with a real game window and physical input.

Resolve the deployed runtime from `NS_BINARY_DIR` in `config.cmake`; do not hard-code a developer's Titanfall path.

## IDA and IDASQL

`IDASQL_URLS.md` is the root registry for IDASQL endpoints. When a user supplies an IDASQL URL, or an agent discovers one, record it there immediately without credentials. Verify every endpoint with:

```sql
SELECT key, value
FROM binary
WHERE key IN ('filename', 'idb_path', 'sha256');
```

Reuse a verified live endpoint before starting another server. Preserve unreachable entries and mark them `stale` or `unverified`; do not silently repoint an entry at a different database.

Before analyzing a raw executable or DLL, look for the corresponding `.i64` or `.idb` in the repository and configured game runtime. If a matching database exists:

- use the recorded IDASQL URL, or run `idasql -s <database>` when a new local server is genuinely needed;
- do **not** run `idasql -s <raw .exe/.dll>` and create a second analysis;
- do not regenerate, overwrite, or fork the existing database unless the user explicitly asks.

Fresh raw-binary analysis is the fallback only when no matching database exists. Confirm binary identity from `binary.filename`, `binary.idb_path`, and `binary.sha256` before relying on results.

## C++ namespaces and linkage

Do not introduce anonymous namespaces. Put helpers and implementation-only types in a descriptive named namespace associated with the subsystem. Use `static` for translation-unit-local functions or objects when internal linkage is required.

Do not churn untouched legacy code solely to remove an existing anonymous namespace. When changing declarations already inside one, migrate the declarations touched by the change to an appropriate named namespace rather than adding more anonymous-namespace content.
