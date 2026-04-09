import { FormEvent, useEffect, useRef, useState } from "react";

type Player = {
  id: string;
  name: string;
  connected: boolean;
};

type RoomState = {
  roomCode: string;
  phase: string;
  players: Player[];
  liberalPolicies: number;
  fascistPolicies: number;
  electionTracker: number;
  presidentId: string;
  chancellorId: string;
  winner: string;
};

const defaultRoomState: RoomState = {
  roomCode: "DEMO1",
  phase: "lobby",
  players: [],
  liberalPolicies: 0,
  fascistPolicies: 0,
  electionTracker: 0,
  presidentId: "",
  chancellorId: "",
  winner: "",
};

export default function App() {
  const [roomCode, setRoomCode] = useState("DEMO1");
  const [name, setName] = useState("");
  const [roomState, setRoomState] = useState<RoomState>(defaultRoomState);
  const [status, setStatus] = useState("Disconnected");
  const socketRef = useRef<WebSocket | null>(null);

  useEffect(() => {
    const socket = new WebSocket("ws://localhost:18080/ws");
    socketRef.current = socket;

    socket.onopen = () => setStatus("Connected");
    socket.onclose = () => setStatus("Disconnected");
    socket.onerror = () => setStatus("Connection error");
    socket.onmessage = (event) => {
      const message = JSON.parse(event.data) as {
        type: string;
        payload?: RoomState;
        message?: string;
      };

      if (message.type === "room_state" && message.payload) {
        setRoomState(message.payload);
      }

      if (message.type === "error") {
        setStatus(`Server error: ${message.message ?? "unknown"}`);
      }
    };

    return () => socket.close();
  }, []);

  function joinRoom(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    if (!socketRef.current || socketRef.current.readyState !== WebSocket.OPEN) {
      setStatus("Socket not ready");
      return;
    }

    socketRef.current.send(
      JSON.stringify({
        type: "join_room",
        roomCode,
        name: name || "Anonymous",
        playerId: crypto.randomUUID(),
      }),
    );
  }

  function startGame() {
    socketRef.current?.send(JSON.stringify({ type: "start_game" }));
  }

  return (
    <main className="shell">
      <section className="hero">
        <p className="eyebrow">C++ rules engine + realtime web client</p>
        <h1>Secret Hitler</h1>
        <p className="lede">
          This starter app gives us a lobby, a live socket connection, and a
          clean split between hidden game logic and browser presentation.
        </p>
      </section>

      <section className="panel">
        <form className="join-form" onSubmit={joinRoom}>
          <label>
            Room Code
            <input
              value={roomCode}
              onChange={(e) => setRoomCode(e.target.value.toUpperCase())}
              maxLength={5}
            />
          </label>
          <label>
            Name
            <input
              value={name}
              onChange={(e) => setName(e.target.value)}
              placeholder="Player name"
            />
          </label>
          <button type="submit">Join room</button>
        </form>

        <div className="status-row">
          <span>Status: {status}</span>
          <button type="button" onClick={startGame}>
            Start game
          </button>
        </div>
      </section>

      <section className="grid">
        <article className="panel">
          <h2>Room</h2>
          <dl className="facts">
            <div>
              <dt>Code</dt>
              <dd>{roomState.roomCode}</dd>
            </div>
            <div>
              <dt>Phase</dt>
              <dd>{roomState.phase}</dd>
            </div>
            <div>
              <dt>Election tracker</dt>
              <dd>{roomState.electionTracker}</dd>
            </div>
          </dl>
        </article>

        <article className="panel">
          <h2>Board</h2>
          <dl className="facts">
            <div>
              <dt>Liberal policies</dt>
              <dd>{roomState.liberalPolicies}</dd>
            </div>
            <div>
              <dt>Fascist policies</dt>
              <dd>{roomState.fascistPolicies}</dd>
            </div>
            <div>
              <dt>President</dt>
              <dd>{roomState.presidentId || "TBD"}</dd>
            </div>
          </dl>
        </article>
      </section>

      <section className="panel">
        <h2>Players</h2>
        <ul className="player-list">
          {roomState.players.map((player) => (
            <li key={player.id}>
              <span>{player.name}</span>
              <span>{player.connected ? "online" : "offline"}</span>
            </li>
          ))}
        </ul>
      </section>
    </main>
  );
}
