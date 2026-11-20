# Second Life / OpenSimulator Compatibility Matrix

Status values: Not started / In progress / Implemented / Verified / Blocked. No feature is considered Verified without repeatable tests against a real compatible Grid and comparison against a reference Viewer.

| Area | Required behavior | Current status | Acceptance evidence |
|---|---|---|---|
| Login | SL and OpenSim login/session lifecycle | Not started | Login test logs with secrets redacted |
| LLUDP | Message templates, sequencing, ACK/reliability, circuit lifecycle | Not started | Protocol conformance tests |
| Capabilities | Capability discovery and HTTP requests | Not started | Captured request/response fixtures |
| LLSD | XML/binary LLSD parsing and serialization as required | Not started | Round-trip and fixture tests |
| Region | Region handshake, parcel/terrain/environment state | Not started | Region load comparison |
| Objects | Prim parameters, linksets, transforms, flags, updates | Not started | Reference-scene comparison |
| Mesh | Mesh asset decode, LODs, skinning where applicable | Not started | Known mesh fixture set |
| Materials | Legacy texture/material behavior and newer material capabilities | Not started | Material test gallery |
| Textures | Image formats, UUID fetch, cache, alpha and face mapping | Not started | Texture fixture gallery |
| Avatar | Shape/body parameters, skeleton, appearance updates | Not started | Avatar appearance matrix |
| Clothing | Wearables, layers, alpha masks, attachments | Not started | Outfit regression set |
| Bake/appearance | Baked texture and appearance pipeline | Not started | Appearance comparison |
| Animation | Legacy animations, priorities, overrides and playback | Not started | Animation regression set |
| AO/Gesture | AO workflows, gestures and animation state interactions | Not started | AO/gesture test plan |
| Inventory | Folders, items, fetch, update, move, copy and permissions | Not started | Inventory operation tests |
| Attachments | Attachment points, attach/detach and state updates | Not started | Attachment test scenes |
| HUD | HUD attachment rendering, input and script-driven updates | Not started | HUD compatibility gallery |
| LSL-visible behavior | Touch, collision, sit, rez, sound, particles, texture and other viewer-visible effects | Not started | Script behavior suite on server |
| Chat/IM | Local chat, IM, group chat and typing state | Not started | Multi-account tests |
| Friends/groups | Presence, friend permissions, group data and notices | Not started | Two-account tests |
| Teleport | Local/remote teleport and failure recovery | Not started | Region crossing tests |
| Voice | Voice login, spatial position/orientation, channel lifecycle | Not started | Voice test grid/account |
| Media | Parcel media and media-on-a-prim behavior | Not started | Media test fixtures |
| Marketplace | Supported purchase/delivery flows subject to service/API access | Not started | Service-specific acceptance |
| Physics extension | New server-authoritative collision/grab behavior | Not started | Multi-client determinism tests |

## Test policy

- Use controlled test accounts and content owned or authorized for testing.
- Keep credentials and session tokens out of logs and fixtures.
- Run the same scenario in a pinned Firestorm reference build and the prototype.
- Record Grid type, server version where known, Viewer commit, Godot version, OS, GPU, and test asset identifiers.
- Separate protocol conformance from visual parity; passing one does not imply the other.
