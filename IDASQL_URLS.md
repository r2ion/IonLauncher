# IDASQL URL registry

Use this file as the repository-wide registry for IDASQL endpoints supplied by a user or discovered during analysis. Verify an endpoint against the `binary` table before relying on it. Never store authentication tokens, credentials, or token-bearing query strings here.

| Target | Database | Transport | Base URL | Last verified (UTC) | Status / notes |
| --- | --- | --- | --- | --- | --- |
| `server.dll` | `C:\Program Files\EA Games\Titanfall2\server.dll.i64` | HTTP | `http://127.0.0.1:8139` | `2026-08-09T11:08:54Z` | Stale; previously verified as SHA-256 `a5ca3a25c8ae56952a26141b0f6cdcb6c19086c8c39013f7cb345d5d723661af`; IDA remains open without an HTTP listener. |
| `server.dll` | `C:\Program Files\EA Games\Titanfall2\server.dll.i64` | HTTP | `http://127.0.0.1:8191` | `2026-08-10T07:29:23Z` | Verified; SHA-256 `a5ca3a25c8ae56952a26141b0f6cdcb6c19086c8c39013f7cb345d5d723661af`. |
| `client.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\client.dll.i64` | HTTP | `http://127.0.0.1:8177` | `2026-08-09T11:08:54Z` | Stale; previously verified as SHA-256 `002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc`; endpoint is no longer listening. |
| `vstdlib.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\vstdlib.dll.i64` | HTTP | `http://127.0.0.1:8148` | `2026-08-09T06:39:34Z` | Verified; SHA-256 `b22952a07850d836babdd726b27bba87fd39228658ad44a170fd5e3b51b66caf`. |
| `engine.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\engine.dll.i64` | HTTP | `http://127.0.0.1:8100` | `2026-08-10T12:33:47Z` | Verified; SHA-256 `58eb1a1b44b30275bdd21368de264d856bc310d37d93a0f76f111f0026913487`. |
| `client.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\client.dll.i64` | HTTP | `http://127.0.0.1:8155` | `2026-08-09T10:35:46Z` | Stale; previously verified as SHA-256 `002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc`; IDA process exited. |
| `client.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\client.dll.i64` | HTTP | `http://127.0.0.1:8196` | `2026-08-10T07:25:20Z` | Verified; SHA-256 `002b36487fec7c98882929fbccdb506d0146bbf3270cbb964a4d21c5edf7eebc`. |
| `mileswin64.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\mileswin64.dll.i64` | HTTP | `http://127.0.0.1:8153` | `2026-08-09T09:58:54Z` | Verified; SHA-256 `3821976711ec29275518bd0fd914ba7d2d3eb5aff15700450fee035bd026f924`. |
| `Client.dll` | `C:\Users\Will\Documents\portal2_game_bins\Client.dll.i64` | HTTP | `http://127.0.0.1:8108` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `6f464720d91b3d38d856dd18cadaf0254dffb44f267c9949c52f0bcacfdad4ec`. |
| `engine.dll` | `C:\Users\Will\Downloads\Telegram Desktop\portal2_engine_bins\bin\engine.dll.i64` | HTTP | `http://127.0.0.1:8125` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `78a67d308d1e1a4d47df0664b538e6be78304476b7893de68ac7396d4afab7a0`. |
| `tier0.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\tier0.dll.i64` | HTTP | `http://127.0.0.1:8132` | `2026-08-10T12:15:31Z` | Verified; SHA-256 `2ff3a9bf91d67b8d8e2c3c08eba4c8b38e8d39cb876d910d9f187ff1af4f2df0`. |
| `engine.dll` | `S:\game\bin\x64_retail\engine.dll.i64` | HTTP | `http://127.0.0.1:8134` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `0214a40436eca77f160d12763ab8f392a8ce2268bfe0a973188e426ae4c0263c`. |
| `server.dll` | `E:\Games\EA\R1Delta\game\r1delta\bin\server.dll.i64` | HTTP | `http://127.0.0.1:8136` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `50faad928e0715a51ad624d0a3eb8f00f7ffad9e66258dc95c960f9d11ff41e4`. |
| `client.dll` | `S:\game\r1\bin\x64_retail\client.dll.i64` | HTTP | `http://127.0.0.1:8140` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `5b3f00805568eeaa6ee1cf889f12dc4570eb28a3fad2626ec0578eae3e7afd5e`. |
| `materialsystem_dx11.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\materialsystem_dx11.dll.i64` | HTTP | `http://127.0.0.1:8141` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `1afffb8dfe2a97a00ca3c5a68ca5277249176c2fd51ee7c5ad70f6eb699a6050`. |
| `datacache.dll` | `C:\Users\Will\Documents\portal2_engine_bins\bin\datacache.dll.i64` | HTTP | `http://127.0.0.1:8144` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `3dcd1924ae76c94d2bb92f503345be36b6dd6f9fecd3784e443469d25cb53bac`. |
| `materialsystem_dx11.dll` | `S:\game\bin\x64_retail\materialsystem_dx11.dll.i64` | HTTP | `http://127.0.0.1:8146` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `0d2337844afb0e6e3c2079638c798101bf25910d1360dd0f64bf4e1d9c6b78c8`. |
| `MaterialSystem.dll` | `C:\Users\Will\Documents\portal2_engine_bins\bin\MaterialSystem.dll.i64` | HTTP | `http://127.0.0.1:8163` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `847632544ad3b03724354619321bd4dbb4f2283744223ff09975904f14f8c4d8`. |
| `Titanfall2.exe` | `C:\Program Files\EA Games\Titanfall2\Titanfall2.exe.i64` | HTTP | `http://127.0.0.1:8166` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `1d557ff919aa2215b324292b24362ed780942e0c7d22310ff4ca1605982d0a62`. |
| `ui(11).dll` | `C:\Program Files\EA Games\Titanfall2\r2\paks\Win64\ui(11).dll.i64` | HTTP | `http://127.0.0.1:8168` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `ac211cf1b9452e998db88b6614feae7a1e13c6d37c84b247b00f5e4d1a74f135`. |
| `launcher.dll` | `C:\Users\Will\AppData\Local\R1Delta\app-2.9.30\r1delta\bin\launcher.dll.i64` | HTTP | `http://127.0.0.1:8169` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `c0320bbe75b59ce221e3fb1dbdcdf8857e9305845ff9b0f3f3bfb4897d4b7a38`. |
| `launcher.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\launcher.dll.i64` | HTTP | `http://127.0.0.1:8170` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `6791af0641e2346dff3dcbe0fb4baa63914590921ad46b6bfec2f0dc7ce0eff0`. |
| `vphysics.dll` | `S:\game\bin\x64_retail\vphysics.dll.i64` | HTTP | `http://127.0.0.1:8171` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `4cbf051a20e5b50973f5e9acb016dc006cd839895231be2f9ea63c568d94abd8`. |
| `datacache.dll` | `S:\game\bin\x64_retail\datacache.dll.i64` | HTTP | `http://127.0.0.1:8178` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `67f0f05a876897de54578e830f571e1b51a019ae1a2f0dfe1bb7f3dd46f8be3d`. |
| `studiorender.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\studiorender.dll.i64` | HTTP | `http://127.0.0.1:8179` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `1c96db33cbda1c52684ff26dcf798c71a3aa69cb16bc7c03ffc7a03fde1de4c5`. |
| `studiorender.dll` | `S:\game\bin\x64_retail\studiorender.dll.i64` | HTTP | `http://127.0.0.1:8180` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `d4a6667e10f2e221c9245f7ee631022ce62f820048d14ef9db65d975fea4ec40`. |
| `engine.dll` | `C:\Users\Will\Documents\portal2_engine_bins\bin\engine.dll.i64` | HTTP | `http://127.0.0.1:8182` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `78a67d308d1e1a4d47df0664b538e6be78304476b7893de68ac7396d4afab7a0`. |
| `filesystem_stdio.dll` | `E:\Games\EA\R1Delta\game\bin\x64_retail\filesystem_stdio.dll.i64` | HTTP | `http://127.0.0.1:8185` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `8200543c985b34b592d30c49a97e45f00db48538baccf29eeed671d0db8736a2`. |
| `mileswin64.dll` | `C:\Users\Will\Downloads\38833FF26BA1D.UnigramPreview_g9c9v27vpyspw!App\mileswin64.dll.i64` | HTTP | `http://127.0.0.1:8187` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `df72bdf262758e50ecbf5b6e5481f3372fbe966327de7409c066a8c0b8c4973b`. |
| `vphysics.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\vphysics.dll.i64` | HTTP | `http://127.0.0.1:8199` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `2452d36b0f4bc1fc091837db5dc6648aec2d785416f832a00c12217c316f25ba`. |
| `datacache.dll` | `C:\Program Files\EA Games\Titanfall2\bin\x64_retail\datacache.dll.i64` | HTTP | `http://127.0.0.1:8201` | `2026-08-09T11:08:54Z` | Verified; SHA-256 `0fe9657e21b72ed5e8936f86220474cb25ccaedfc452c8942af7e791881c8df5`. |

When adding or refreshing an entry:

1. Keep the exact base URL and identify its transport (`HTTP` or `MCP`).
2. Query `SELECT key, value FROM binary WHERE key IN ('filename', 'idb_path', 'sha256');` and update the target/database fields from the result.
3. Record the verification time in UTC. If the endpoint cannot be reached, retain it and mark it `stale` or `unverified` rather than guessing its target.
4. Reuse a verified endpoint instead of starting another server for the same database.
