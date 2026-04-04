# OpenRCT2 Desync Prevention and Tracing Guide

OpenRCT2 uses a **deterministic lockstep** architecture for multiplayer. This means every client starts with the exact same game state and must execute the exact same set of game actions at the exact same tick to maintain synchronisation. Any deviation, no matter how small, will eventually lead to a "desync".

## The "Golden Rule" of Determinism
**Every client must reach the same conclusion given the same inputs.**

---

## Dos and Don'ts

### ❌ Don't: Use Floating Point Math in Game Logic
Floating point results can vary between different CPU architectures (x86 vs ARM) and compiler optimisations.
*   **Correction:** Use fixed-point math or integers.
*   **Exception:** Rendering-only code or `consteval` blocks that resolve to constants at compile-time. If you find floating point in the core, please report it as a bug.

### ❌ Don't: Use Non-Deterministic Randomness
Functions like `rand()`, `std::mt19937`, or `UtilRand()` use system-specific entropy or local seeds.
*   **Correction:** Use `ScenarioRand()` or `ScenarioRandMax()`. These use a seed that is part of the synchronised game state.

### ❌ Don't: Leak Local Configuration into Game State
Using `Config::Get()` to decide game logic (e.g., "should this guest be happy?") is dangerous because different players have different config settings.
*   **Correction:** Only use config for UI/Rendering or synchronize the required settings from the server to all clients during connection.

### ❌ Don't: Use Pointers as IDs
Memory addresses are unique to each process and will never be the same across different machines.
*   **Correction:** Use `EntityId`, `RideId`, or other index-based identifiers.

### ❌ Don't: Read System Time for Logic
`Platform::GetTicks()` or `std::chrono` will differ between clients.
*   **Correction:** Use `gameState.currentTicks` for timing logic.

### ❌ Don't: Leave Memory Uninitialised
Uninitialised variables contain "garbage" from previous memory usage, which will differ between machines.
*   **Correction:** Always initialize members in constructors or using brace-initialization `{}`.

### ✅ Do: Sort Collections of Entities
If you iterate over a list of entities (e.g., to find the closest mechanic), ensure the list is in the same order on all clients.
*   **Correction:** The `EntityRegistry` in OpenRCT2 automatically maintains sorted lists by `EntityId`.

### ✅ Do: Use Game Actions for All State Changes
Any change to the game state initiated by a user must go through a `GameAction`.
*   **Correction:** `GameActionRunner` ensures actions are timestamped with a tick and sent to all clients.

---

## Suggested Tools for Tracing Desyncs

### 1. High-Frequency Rolling Hashing (Proposed)
Instead of a full sprite checksum every 100 ticks, implement a rolling hash (e.g., xxhash) of critical game state components (Money, Guest Count, Ride Status) every single tick.
*   **Benefit:** Pinpoints the *exact* tick where divergence begins.

### 2. Deterministic Action Logger
A tool that records every `GameAction` and the `srand` state at each tick into a replay-like format.
*   **Benefit:** Allows developers to "replay" a desynced session locally and step through the logic with a debugger.

### 3. Binary Search State Comparator
Enhance the existing snapshot comparison to perform a binary search across the game state memory.
*   **Benefit:** Quickly isolates which specific byte in a 500-byte entity structure changed.

### 4. Headless Desync Tester
A CI tool that runs two instances of the game engine with different simulated latencies and verifies their state remains identical after a battery of random `GameActions`.

---

## Immediate Fixes Identified
1.  **UtilRand in Actions:** `RideCreateAction` and `RideSetVehicleAction` currently use `UtilRand()` for picking "unused" colors. This must be changed to `ScenarioRand()`.
2.  **Weather Randomness:** Lightning/Thunder timing uses `UtilRand()`. While visual, if this ever triggers guest "scared" logic, it will desync.
3.  **Config Leaks:** `AllowEarlyCompletion` and `GameSpeed` logic leak local config settings into synced logic.
