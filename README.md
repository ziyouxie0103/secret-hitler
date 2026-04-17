# Secret Hitler

Starter scaffold for a web-based Secret Hitler implementation with:

- `engine/`: C++ game rules and state
- `server/`: C++ HTTP/WebSocket server
- `client/`: React + TypeScript browser client

## Architecture

The `engine` owns game rules and should not know about HTTP, WebSockets, or browsers.
The `server` translates client messages into engine calls and broadcasts visible state.
The `client` renders public and private game information for each player.

## Layout

```text
.
|-- CMakeLists.txt
|-- engine/
|   |-- include/sh/
|   |-- src/
|   `-- tests/
|-- server/
|   |-- include/sh/
|   `-- src/
`-- client/
    |-- src/
    `-- public/
```

## Next steps

1. Flesh out the game phases and role assignment in `engine/`.
2. Replace the server's in-memory demo room with real room/session management.
3. Expand the React client from the lobby shell into the full game table.

## Build

Required tools:

- `clang++` or another C++20 compiler
- `cmake` 3.20+
- `node` 20+
- `npm`
- `boost`

## Dependency notes

The C++ build can work in two modes:

- System packages: CMake will first try `find_package(...)` for `Crow` and `nlohmann_json`, and it requires `Boost`.
- FetchContent fallback: if `Crow` or `nlohmann_json` are missing and `SH_USE_FETCHCONTENT=ON`, CMake will download them from GitHub.

On macOS with Homebrew, install the local prerequisites with:

```bash
brew install cmake node boost nlohmann-json
```

Useful configure variants:

```bash
# Build everything and allow downloads for missing dependencies
cmake -S . -B build

# Build only the engine, skip the server and tests
cmake -S . -B build -DSH_BUILD_SERVER=OFF -DSH_BUILD_TESTS=OFF

# Use only system-installed packages, never download
cmake -S . -B build -DSH_USE_FETCHCONTENT=OFF
```

Backend:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Frontend:

```bash
cd client
npm install
npm run dev
```

## Manual server testing

You can also test the WebSocket server without the React app:

1. Start the server:

```bash
./build/server/secret_hitler_server
```

2. Open [server_test.html](/Users/zyxie/Desktop/secret-hitler/server_test.html) directly in your browser.
3. Open the file in multiple tabs to simulate multiple players.
4. Use the buttons to join a room, start a game, nominate, vote, and resolve legislative or executive actions.

The page shows:

- the latest `room_state`
- the latest private `player_view`
- a message log of everything sent and received

When a chancellor is nominated, the server does not send a special standalone
"nomination" event right now. Instead, clients learn it from the next
`room_state`, where:

- `phase` is still `election`
- `chancellorId` becomes non-empty
- `pendingVotes` starts at `0`

That is the signal that all living players should vote.

ngrok http 18080