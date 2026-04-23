# Security Audit Findings - OpenRCT2 Network Code

## High Level Findings

### 1. Out-of-Bounds Read in `Packet::ReadString()`
The `Packet::ReadString()` method in `src/openrct2/network/NetworkPacket.cpp` does not properly handle cases where a string is not null-terminated within the packet data. It iterates through the `Data` buffer until it finds a `\0` byte. If the packet is malformed and lacks a null terminator, the loop will continue reading past the end of the `Data` buffer, potentially leading to information leakage or a crash.

### 2. Unchecked Return Values from `Packet::Read()`
Multiple packet handlers (e.g., `ServerHandleMapRequest`, `ServerHandleAuth`) call `Packet::Read()` and immediately use the returned pointer without checking if it is `nullptr`. `Packet::Read()` returns `nullptr` if the requested number of bytes exceeds the remaining data in the packet. Dereferencing this `nullptr` will cause the application (including the server) to crash.

### 3. Denial of Service (DoS) via Memory Exhaustion
The `Connection::receiveData()` method in `src/openrct2/network/NetworkConnection.cpp` reads data from the socket and appends it to `_inboundBuffer` without any upper bound on the buffer size. A malicious client can send a continuous stream of data without ever completing a packet, causing the server to consume all available memory and eventually crash or become unresponsive.

### 4. Arbitrary Tile Element Injection via `TileModifyAction`
The `TileModifyAction` allows authenticated players with `Permission::modifyTile` to "paste" an arbitrary `TileElement` onto the map. The `TileElement` structure contains internal state, including `BannerIndex`. Since the server does not validate the contents of the `TileElement` being pasted, a malicious player could inject a `TileElement` with a crafted `BannerIndex` or other invalid internal data. This can lead to:
- **Out-of-Bounds Reads/Writes**: When the game attempts to render or process the corrupted tile element, it may use invalid indices to access other game structures (like the banner list).
- **Remote Code Execution (RCE)**: While more difficult to achieve, if the corrupted state is used in a way that affects control flow (e.g., via a virtual function call or an indirect jump), it could potentially be leveraged for RCE.

### 5. Insecure Integer Handling and Lack of Validation
- **Packet Size Overflows**: In `Connection::readPacket()`, the `totalPacketLength` is calculated as `sizeof(header) + header.size`. If `header.size` is very large, this can overflow, leading to incorrect buffer processing.
- **Large Allocation Requests**: Handlers that read a "count" of items (e.g., `objectCount` in `Client_Handle_OBJECTS_LIST`) do not validate that the count is reasonable before entering loops or potentially allocating memory.

## Exploit Chains

### Server Crash (DoS) via Malformed Map Request
1. A client connects and authenticates (authentication is required for `mapRequest`).
2. The client sends a `Command::mapRequest` packet.
3. The client specifies a large `size` for the number of objects.
4. For one of the objects, the client specifies it as a `DAT` object (generation 0) but does not provide enough data for a `RCTObjectEntry`.
5. `NetworkBase::ServerHandleMapRequest` calls `packet.Read(sizeof(RCTObjectEntry))`, which returns `nullptr`.
6. The server immediately attempts to use this pointer (e.g., in `repo.FindObject(entry)`), resulting in a null pointer dereference and a crash.

### Remote Memory Corruption via `TileModifyAction`
1. A player with `Permission::modifyTile` sends a `Command::gameAction` for `TileModifyAction` with `TileModifyType::AnyPaste`.
2. The `_pasteElement` contains a `BannerIndex` that is intentionally set to a very large value.
3. The server pastes this element onto the map.
4. Any subsequent operation that iterates over tile elements and accesses the banner (e.g., rendering or a subsequent `AnyRemove` on a large scenery part) will use the out-of-bounds `BannerIndex` to index into the global banner array, leading to memory corruption.
