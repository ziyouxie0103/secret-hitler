import React, { FormEvent, useEffect, useMemo, useRef, useState } from "react";

type Player = {
  id: string;
  name: string;
  connected: boolean;
  alive: boolean;
};

type RoomState = {
  roomCode: string;
  phase: "lobby" | "election" | "legislative_session" | "executive_action" | "complete";
  players: Player[];
  liberalPolicies: number;
  fascistPolicies: number;
  electionTracker: number;
  presidentId: string;
  chancellorId: string;
  lastPresidentId: string;
  lastChancellorId: string;
  winner: string;
  drawPileSize: number;
  discardPileSize: number;
  pendingVotes: number;
  presidentHandSize: number;
  chancellorHandSize: number;
  pendingExecutivePower: string;
  hostPlayerId: string;
};

type PlayerView = {
  playerId: string;
  playerName: string;
  role: string;
  party: string;
  alive: boolean;
  knownFascists: string[];
  legislativeHand: string[];
  policyPeek: string[];
  investigationResult: string;
};

type ServerMessage =
  | { type: "joined_room"; payload?: { roomCode?: string; playerId?: string; hostPlayerId?: string } }
  | { type: "room_state"; payload?: RoomState }
  | { type: "player_view"; payload?: PlayerView }
  | { type: "sync_ack"; message?: string }
  | { type: "error"; message?: string };

type RoundLogEntry = {
  round: number;
  president: string;
  chancellor: string;
  enactedPolicy: string;
  specialEvent?: string;
};

type EventKind = "policy" | "execution" | "winner" | "";

const defaultRoomState: RoomState = {
  roomCode: "DEMO1",
  phase: "lobby",
  players: [],
  liberalPolicies: 0,
  fascistPolicies: 0,
  electionTracker: 0,
  presidentId: "",
  chancellorId: "",
  lastPresidentId: "",
  lastChancellorId: "",
  winner: "",
  drawPileSize: 0,
  discardPileSize: 0,
  pendingVotes: 0,
  presidentHandSize: 0,
  chancellorHandSize: 0,
  pendingExecutivePower: "",
  hostPlayerId: "",
};

function getStoredPlayerId() {
  const existing = localStorage.getItem("secret-hitler-player-id");
  if (existing) {
    return existing;
  }

  const nextId = crypto.randomUUID();
  localStorage.setItem("secret-hitler-player-id", nextId);
  return nextId;
}

function formatLabel(value: string) {
  if (!value) {
    return "-";
  }

  return value
    .split("_")
    .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
    .join(" ");
}

function formatPhaseForPeople(phase: RoomState["phase"]) {
  switch (phase) {
    case "lobby":
      return "Waiting in lobby";
    case "election":
      return "Election underway";
    case "legislative_session":
      return "Legislative session";
    case "executive_action":
      return "Executive action";
    case "complete":
      return "Game finished";
  }
}

// New PolicyLabel component to consistently style Liberal and Fascist policies
function PolicyLabel({ policy }: { policy: string }) {
  const isLiberal = policy === "Liberal";
  const className = `policy-label ${isLiberal ? "policy-label-liberal" : "policy-label-fascist"}`;
  return (
    <span className={className}>
      {policy}
    </span>
  );
}

// New helper function to render text that might be a policy, applying PolicyLabel if appropriate
function renderPolicyText(text: string): React.ReactNode {
  if (text === "Liberal" || text === "Fascist") {
    return <PolicyLabel policy={text} />;
  }
  return text;
}

export default function App() {
  const socketRef = useRef<WebSocket | null>(null);
  const [roomCode, setRoomCode] = useState("DEMO1");
  const [name, setName] = useState(() => localStorage.getItem("secret-hitler-player-name") ?? "");
  const [nomineeId, setNomineeId] = useState("");
  const [executiveTargetId, setExecutiveTargetId] = useState("");
  const [playerId, setPlayerId] = useState(() => getStoredPlayerId());
  const [joined, setJoined] = useState(false);
  const [status, setStatus] = useState("Disconnected");
  const [roomState, setRoomState] = useState<RoomState>(defaultRoomState); // No change needed here
  const [playerView, setPlayerView] = useState<PlayerView | null>(null);
  const [eventNotice, setEventNotice] = useState<string | React.ReactNode>("");
  const [eventKind, setEventKind] = useState<EventKind>("");
  const [noticeKey, setNoticeKey] = useState(0);
  const previousPolicyCountsRef = useRef({ liberal: 0, fascist: 0 });
  const previousPlayersRef = useRef<Record<string, boolean>>({});
  const previousWinnerRef = useRef("");
  const [roundLog, setRoundLog] = useState<RoundLogEntry[]>([]);
  const [privateOpen, setPrivateOpen] = useState(false);
  const [selectedVote, setSelectedVote] = useState<"ja" | "nein" | "">("");
  const [voteLocked, setVoteLocked] = useState(false);
  const [selectedPolicyIndex, setSelectedPolicyIndex] = useState<number | null>(null);
  const [policyLocked, setPolicyLocked] = useState(false);
  const [syncPending, setSyncPending] = useState(false);
  const aliveSignature = useMemo(
    () => roomState.players.map((player) => `${player.id}:${player.alive ? 1 : 0}`).join("|"),
    [roomState.players],
  );
  
  useEffect(() => {
    let wsUrl = import.meta.env.VITE_WS_URL;

    // If no environment variable, fallback to localhost
    if (!wsUrl) {
      wsUrl = "ws://localhost:18080/ws";
    }

    // Security: If the page is HTTPS, the socket MUST be WSS
    if (window.location.protocol === "https:" && wsUrl.startsWith("ws:")) {
      wsUrl = wsUrl.replace("ws:", "wss:");
    }

    const socket = new WebSocket(wsUrl);
    socketRef.current = socket;

    socket.onopen = () => setStatus("Connected to server");
    socket.onclose = () => {
      setJoined(false);
      setStatus("Disconnected");
    };
    socket.onerror = () => setStatus("Connection error");
    socket.onmessage = (event) => {
      const message = JSON.parse(event.data) as ServerMessage;

      if (message.type === "joined_room") {
        setJoined(true);
        if (message.payload?.playerId) {
          setPlayerId(message.payload.playerId);
          localStorage.setItem("secret-hitler-player-id", message.payload.playerId);
        }
        setStatus(`In room ${message.payload?.roomCode ?? roomCode}`);
        return;
      }

      if (message.type === "room_state" && message.payload) {
        setRoomState(message.payload);
        if (syncPending) {
          setStatus(`State synced for room ${message.payload.roomCode}`);
          setSyncPending(false);
        } else {
          setStatus(`In room ${message.payload.roomCode}`);
        }
        return;
      }

      if (message.type === "player_view" && message.payload) {
        setPlayerView(message.payload);
        return;
      }

      if (message.type === "sync_ack") {
        setStatus("Refreshing room state...");
        return;
      }

      if (message.type === "error") {
        setStatus(`Server error: ${message.message ?? "unknown"}`);
      }
    };

    return () => socket.close();
  }, []);

  useEffect(() => {
    if (!joined) {
      previousPolicyCountsRef.current = {
        liberal: roomState.liberalPolicies,
        fascist: roomState.fascistPolicies,
      };
      previousPlayersRef.current = Object.fromEntries(
        roomState.players.map((player) => [player.id, player.alive]),
      );
      previousWinnerRef.current = roomState.winner;
      setEventKind("");
      setEventNotice("");
      setRoundLog([]);
      return;
    }

    const previous = previousPolicyCountsRef.current;
    const liberalDelta = Math.max(0, roomState.liberalPolicies - previous.liberal);
    const fascistDelta = Math.max(0, roomState.fascistPolicies - previous.fascist);
    const nextEntries = [
      ...Array.from({ length: liberalDelta }, () => "Liberal"),
      ...Array.from({ length: fascistDelta }, () => "Fascist"),
    ];

    previousPolicyCountsRef.current = {
      liberal: roomState.liberalPolicies,
      fascist: roomState.fascistPolicies,
    };

    if (nextEntries.length > 0) {
      const latestPolicy = nextEntries[nextEntries.length - 1];
      setEventKind("policy");
      setEventNotice(<>A <PolicyLabel policy={latestPolicy} /> policy was enacted.</>);
      setNoticeKey((value) => value + 1);
      setRoundLog((previousLog) => [
        ...previousLog,
        ...nextEntries.map((enactedPolicy, index) => ({
          round: previousLog.length + index + 1,
          president: displayPlayer(roomState.lastPresidentId || roomState.presidentId || ""),
          chancellor: displayPlayer(roomState.lastChancellorId || roomState.chancellorId || ""),
          enactedPolicy,
        })),
      ]);
    }

    const previousPlayers = previousPlayersRef.current;
    const executedPlayers = roomState.players.filter((player) => previousPlayers[player.id] && !player.alive);
    previousPlayersRef.current = Object.fromEntries(
      roomState.players.map((player) => [player.id, player.alive]),
    );

    if (executedPlayers.length > 0) {
      const executedNames = executedPlayers.map((player) => `${player.name} (${player.id})`).join(", ");
      setEventKind("execution");
      setEventNotice(`Player ${executedNames} was executed.`);
      setNoticeKey((value) => value + 1);
      setRoundLog((previousLog) => {
        if (previousLog.length === 0) {
          return previousLog;
        }

        const nextLog = [...previousLog];
        const lastEntry = nextLog[nextLog.length - 1];
        nextLog[nextLog.length - 1] = {
          ...lastEntry,
          specialEvent: `Execution: ${executedNames}`,
        };
        return nextLog;
      });
    }

    if (roomState.winner && previousWinnerRef.current !== roomState.winner) {
      setEventKind("winner");
      setEventNotice(<>{roomState.winner === "liberals" ? <PolicyLabel policy="Liberal" /> : <PolicyLabel policy="Fascist" />} win the game.</>);
      setNoticeKey((value) => value + 1);
    }

    previousWinnerRef.current = roomState.winner;
  }, [aliveSignature, joined, roomState.fascistPolicies, roomState.liberalPolicies, roomState.winner]);

  const currentPlayer = useMemo(
    () => roomState.players.find((player) => player.id === playerId) ?? null,
    [playerId, roomState.players],
  );

  const playerNameById = useMemo(() => {
    const entries = new Map<string, string>();
    for (const player of roomState.players) {
      entries.set(player.id, player.name);
    }
    return entries;
  }, [roomState.players]);

  const socketReady = socketRef.current?.readyState === WebSocket.OPEN;

  const isHost = roomState.hostPlayerId === playerId;
  const isPresident = roomState.presidentId === playerId;
  const isChancellor = roomState.chancellorId === playerId;
  const canStart = joined && isHost && roomState.phase === "lobby" && roomState.players.length >= 5;
  const canNominate =
    joined &&
    currentPlayer?.alive &&
    roomState.phase === "election" &&
    isPresident &&
    roomState.chancellorId === "";
  const canVote =
    joined &&
    currentPlayer?.alive &&
    roomState.phase === "election" &&
    roomState.chancellorId !== "";
  const canPresidentDiscard =
    joined &&
    isPresident &&
    roomState.phase === "legislative_session" &&
    (playerView?.legislativeHand.length ?? 0) === 3;
  const canChancellorEnact =
    joined &&
    isChancellor &&
    roomState.phase === "legislative_session" &&
    (playerView?.legislativeHand.length ?? 0) === 2;
  const canResolveExecutive = joined && isPresident && roomState.phase === "executive_action";
  const showPrivateRole = roomState.phase !== "lobby" && playerView?.role;
  const showPrivateParty = roomState.phase !== "lobby" && playerView?.party;
  const isDead = currentPlayer?.alive === false;

  useEffect(() => {
    if (!canVote) {
      setSelectedVote("");
      setVoteLocked(false);
    }
  }, [canVote, roomState.chancellorId, roomState.phase]);

  useEffect(() => {
    if (!canPresidentDiscard && !canChancellorEnact) {
      setSelectedPolicyIndex(null);
      setPolicyLocked(false);
    }
  }, [canPresidentDiscard, canChancellorEnact, roomState.phase]);

  function displayPlayer(playerIdToDisplay: string) {
    if (!playerIdToDisplay) {
      return "nobody";
    }
    return `${playerNameById.get(playerIdToDisplay) ?? "Unknown"} (${playerIdToDisplay})`;
  }

  function titledPlayer(role: "president" | "chancellor", playerIdToDisplay: string) {
    const title = role === "president" ? "the president" : "the chancellor";
    if (!playerIdToDisplay) {
      return title;
    }

    return `${title} ${displayPlayer(playerIdToDisplay)}`;
  }

  const nextStep = useMemo(() => {
    if (!joined) {
      return socketReady
        ? "Choose your room details and click Join Room."
        : "Waiting for the socket connection to open.";
    }

    if (roomState.phase === "lobby") {
      return isHost
        ? "Wait for 5 to 10 players, then start the game."
        : "Wait for the host to start the game.";
    }

    if (isDead) {
      return "You have been executed. You cannot take any more actions, but you can still watch the game.";
    }

    if (roomState.phase === "election" && roomState.chancellorId !== "") {
      return currentPlayer?.alive
        ? `Chancellor nominee is ${displayPlayer(roomState.chancellorId)}. ${isPresident ? "You nominated them." : ""} Vote Ja or Nein, then confirm.`
        : `Waiting for living players to vote on ${displayPlayer(roomState.chancellorId)}.`;
    }

    if (roomState.phase === "election") {
      return isPresident
        ? `You're the president. Choose a chancellor nominee.`
        : `Waiting for ${titledPlayer("president", roomState.presidentId)} to nominate a chancellor.`;
    }

    if (roomState.phase === "legislative_session" && roomState.presidentHandSize === 3) {
      return isPresident
        ? "You're the president. Select one of your three policies, then confirm the discard."
        : `Waiting for ${titledPlayer("president", roomState.presidentId)} to discard one policy.`;
    }

    if (roomState.phase === "legislative_session" && roomState.chancellorHandSize === 2) {
      return isChancellor
        ? "You're the chancellor. Select one of your two policies, then confirm the enactment."
        : `Waiting for ${titledPlayer("chancellor", roomState.chancellorId)} to enact one policy.`;
    }

    if (roomState.phase === "executive_action") {
      return isPresident
        ? `You're the president. Resolve ${formatLabel(roomState.pendingExecutivePower)}.`
        : `Waiting for ${titledPlayer("president", roomState.presidentId)} to resolve ${formatLabel(roomState.pendingExecutivePower)}.`;
    }

    if (roomState.phase === "complete") {
      const winnerLabel = roomState.winner === "liberals" ? <PolicyLabel policy="Liberal" /> : <PolicyLabel policy="Fascist" />;
      return <>Game over. Winner: {winnerLabel}.</>;

    }

    return "Waiting for the game to continue.";
  }, [currentPlayer?.alive, isChancellor, isDead, isHost, isPresident, joined, roomState, playerNameById]);

  function sendMessage(payload: Record<string, unknown>) {
    if (!socketRef.current || socketRef.current.readyState !== WebSocket.OPEN) {
      setStatus("Socket not ready");
      return;
    }

    socketRef.current.send(JSON.stringify(payload));
  }

  function joinRoom(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    localStorage.setItem("secret-hitler-player-name", name || "Anonymous");
    sendMessage({
      type: "join_room",
      roomCode,
      name: name || "Anonymous",
      playerId,
    });
  }

  function startGame() {
    sendMessage({ type: "start_game" });
  }

  function syncState() {
    setSyncPending(true);
    setStatus("Refreshing room state...");
    sendMessage({ type: "sync_state" });
  }

  function nominateChancellor() {
    if (!nomineeId.trim()) {
      setStatus("Enter a player id to nominate");
      return;
    }

    sendMessage({
      type: "nominate_chancellor",
      targetPlayerId: nomineeId.trim(),
    });
  }

  function castVote(vote: "ja" | "nein") {
    sendMessage({
      type: "cast_vote",
      vote,
    });
  }

  function confirmVote() {
    if (!selectedVote) {
      setStatus("Choose Ja or Nein before confirming");
      return;
    }

    castVote(selectedVote);
    setVoteLocked(true);
  }

  function presidentDiscard(index: number) {
    sendMessage({
      type: "president_discard_policy",
      index,
    });
  }

  function chancellorEnact(index: number) {
    sendMessage({
      type: "chancellor_enact_policy",
      index,
    });
  }

  function confirmPolicyAction() {
    if (selectedPolicyIndex === null) {
      setStatus("Choose a policy first");
      return;
    }

    if (canPresidentDiscard) {
      presidentDiscard(selectedPolicyIndex);
      setPolicyLocked(true);
      return;
    }

    if (canChancellorEnact) {
      chancellorEnact(selectedPolicyIndex);
      setPolicyLocked(true);
    }
  }

  function resolveExecutive(type: "investigate_player" | "call_special_election" | "execute_player") {
    if (!executiveTargetId.trim()) {
      setStatus("Enter a player id for the executive target");
      return;
    }

    sendMessage({
      type,
      targetPlayerId: executiveTargetId.trim(),
    });
  }

  return (
    <main className="shell">
      <section className="hero">
        <h1>Secret Hitler</h1>
      </section>

      {joined && roomState.phase !== "lobby" && (
        <section className="panel banner">
          <div>
            <strong>{nextStep}</strong>
          </div>
        </section>
      )}

      {joined && eventNotice && (
        <section key={noticeKey} className="notice-overlay">
          <div className="panel notice-modal">
            <p className="notice-kicker">
              {eventKind === "winner"
                ? "Game Over"
                : eventKind === "execution"
                  ? "Execution"
                  : "Policy Enacted"}
            </p>
            <strong>{eventNotice}</strong>
            <button
              type="button"
              className="secondary"
              onClick={() => {
                setEventNotice("");
                setEventKind("");
              }}
            >
              Continue
            </button>
          </div>
        </section>
      )}

      {!joined && (
        <section className="panel">
          <form className="join-form" onSubmit={joinRoom}>
            <label>
              Room Code
              <input
                value={roomCode}
                onChange={(event) => setRoomCode(event.target.value.toUpperCase())}
                maxLength={5}
              />
            </label>
            <label>
              Name
              <input
                value={name}
                onChange={(event) => setName(event.target.value)}
                placeholder="Player name"
              />
            </label>
            <label>
              Player ID
              <input value={playerId} onChange={(event) => setPlayerId(event.target.value)} />
            </label>
            <div className="button-stack">
              <button type="submit" disabled={!socketReady}>
                Join Room
              </button>
            </div>
          </form>
        </section>
      )}

      {joined && roomState.phase === "lobby" && (
        <section className="panel lobby-actions">
          <div>
            <strong>Room {roomState.roomCode}</strong>
            <p>{isHost ? "Start when 5 to 10 players are here." : "Waiting for the host to start the game."}</p>
          </div>
          <div className="button-row">
            <button type="button" className="secondary" onClick={syncState} disabled={!socketReady}>
              Sync State
            </button>
            <button type="button" onClick={startGame} disabled={!canStart}>
              Start Game
            </button>
          </div>
        </section>
      )}

      {joined && (
      <section className="stack-layout">
        <section className="top-row">
          <article className="panel action-panel">
            {canNominate && (
              <div className="action-group">
                <h3>Nominate Chancellor</h3>
                <label>
                  Chancellor target player id
                  <input
                    value={nomineeId}
                    onChange={(event) => setNomineeId(event.target.value)}
                    placeholder="p2"
                  />
                </label>
                <div className="button-row compact-row">
                  <button type="button" onClick={nominateChancellor}>
                    Nominate Chancellor
                  </button>
                </div>
              </div>
            )}

            {canVote && (
              <div className="action-group">
                <h3>Cast Vote</h3>
                <div className="button-row compact-row">
                  <button
                    type="button"
                    className={selectedVote === "ja" ? "selected" : "secondary"}
                    onClick={() => setSelectedVote("ja")}
                    disabled={voteLocked}
                  >
                    Vote Ja
                  </button>
                  <button
                    type="button"
                    className={selectedVote === "nein" ? "selected danger" : "secondary"}
                    onClick={() => setSelectedVote("nein")}
                    disabled={voteLocked}
                  >
                    Vote Nein
                  </button>
                  <button type="button" onClick={confirmVote} disabled={voteLocked || !selectedVote}>
                    {voteLocked ? "Vote Confirmed" : "Confirm Vote"}
                  </button>
                </div>
                {voteLocked && <p className="helper">Your vote is locked in for this election.</p>}
              </div>
            )}

            {(canPresidentDiscard || canChancellorEnact) && (
              <div className="action-group">
                <h3>{canPresidentDiscard ? "President Discard" : "Chancellor Enact"}</h3>
                <div className="card-row">
                  {(playerView?.legislativeHand ?? []).map((policy, index) => (
                    <button
                      key={`${policy}-${index}`}
                      type="button"
                      className={`policy-card ${policy} ${selectedPolicyIndex === index ? "active" : ""}`}
                      onClick={() => setSelectedPolicyIndex(index)}
                      disabled={policyLocked}
                  >
                    <PolicyLabel policy={policy} />
                  </button>
                ))}
              </div>
                <div className="button-row compact-row">
                  <button type="button" onClick={confirmPolicyAction} disabled={policyLocked || selectedPolicyIndex === null}>
                    {policyLocked
                      ? canPresidentDiscard
                        ? "Discard Confirmed"
                        : "Enactment Confirmed"
                      : canPresidentDiscard
                        ? "Confirm Discard"
                        : "Confirm Enactment"}
                  </button>
                </div>
              </div>
            )}

            {canResolveExecutive && (
              <div className="action-group">
                <h3>{formatLabel(roomState.pendingExecutivePower)}</h3>
                <label>
                  Executive target player id
                  <input
                    value={executiveTargetId}
                    onChange={(event) => setExecutiveTargetId(event.target.value)}
                    placeholder="p3"
                  />
                </label>
                <div className="button-row compact-row">
                  {roomState.pendingExecutivePower === "investigate_loyalty" && (
                    <button type="button" onClick={() => resolveExecutive("investigate_player")}>
                      Investigate Loyalty
                    </button>
                  )}
                  {roomState.pendingExecutivePower === "special_election" && (
                    <button type="button" className="secondary" onClick={() => resolveExecutive("call_special_election")}>
                      Call Special Election
                    </button>
                  )}
                  {roomState.pendingExecutivePower === "execution" && (
                    <button type="button" className="danger" onClick={() => resolveExecutive("execute_player")}>
                      Execute Player
                    </button>
                  )}
                </div>
              </div>
            )}

            {!canNominate && !canVote && !canPresidentDiscard && !canChancellorEnact && !canResolveExecutive && (
              <p className="helper">No actions available for you right now.</p>
            )}
          </article>

          <article className={`panel compact-panel private-panel ${privateOpen ? "is-open" : ""}`}>
            <button
              type="button"
              className="private-toggle"
              onClick={() => setPrivateOpen((open) => !open)}
            >
              <span>Your Private View</span>
              <span>{privateOpen ? "Hide" : "Show"}</span>
            </button>
            {privateOpen && (
              <dl className="facts private-facts">
                {isDead && (
                  <div className="notice-row">
                    <dt>Status</dt>
                    <dd className="dead-notice">You are dead and cannot take actions now.</dd>
                  </div>
                )}
                {showPrivateRole && (
                  <div>
                    <dt>Role</dt>
                    <dd>{renderPolicyText(formatLabel(playerView?.role ?? ""))}</dd>
                  </div>
                )}
                {showPrivateParty && (
                  <div>
                    <dt>Party</dt>
                    <dd>{renderPolicyText(formatLabel(playerView?.party ?? ""))}</dd>
                  </div>
                )}
                {Boolean(playerView?.knownFascists.length) && (
                  <div>
                    <dt>Known fascists</dt>
                    <dd>{playerView?.knownFascists.join(", ")}</dd>
                  </div>
                )}
                {Boolean(playerView?.policyPeek.length) && (
                  <div className="facts-block">
                    <dt>Last policy peek</dt>
                    <dd>
                      {playerView?.policyPeek.map((policy, index) => (
                        <PolicyLabel key={index} policy={policy} />
                      ))}
                    </dd>
                  </div>
                )}
                {Boolean(playerView?.investigationResult) && (
                  <div>
                    <dt>Investigation</dt>
                    <dd>{renderPolicyText(formatLabel(playerView?.investigationResult ?? ""))}</dd>
                  </div>
                )}
                {!showPrivateRole &&
                  !showPrivateParty &&
                  !playerView?.knownFascists.length &&
                  !playerView?.legislativeHand.length &&
                  !playerView?.policyPeek.length &&
                  !playerView?.investigationResult && (
                    <p className="helper">No private information to show right now.</p>
                  )}
              </dl>
            )}
          </article>
        </section>

        <article className="panel compact-panel players-panel">
          <h2>Players</h2>
          <ul className="player-grid">
            {roomState.players.map((player) => {
              const tags = [
                player.id === playerId ? "you" : "",
                player.id === roomState.hostPlayerId ? "host" : "",
                player.id === roomState.presidentId ? "president" : "",
                player.id === roomState.chancellorId ? "chancellor" : "",
                !player.connected ? "offline" : "",
                !player.alive ? "dead" : "",
              ].filter(Boolean);

              return (
                <li
                  key={player.id}
                  className={[
                    "player-tile",
                    player.id === playerId ? "is-self" : "",
                    !player.alive ? "is-dead" : "",
                  ]
                    .filter(Boolean)
                    .join(" ")}
                >
                  <div className="player-heading">
                    <strong>{player.name}</strong>
                    <span className="player-id">{player.id}</span>
                  </div>
                  <div className="tag-row">
                    {tags.map((tag) => (
                      <span key={tag} className={`tag ${tag}`}>
                        {tag}
                      </span>
                    ))}
                  </div>
                </li>
              );
            })}
          </ul>
        </article>

        <article className="panel compact-panel board-panel">
          <h2>Public Board</h2>
          <div className="board-grid">
            <div className="board-tile">
              <span><PolicyLabel policy="Liberal" /> policies</span>
              <strong>{roomState.liberalPolicies}</strong>
            </div>
            <div className="board-tile">
              <span><PolicyLabel policy="Fascist" /> policies</span>
              <strong>{roomState.fascistPolicies}</strong>
            </div>
            <div className="board-tile">
              <span><PolicyLabel policy="Liberal" /> left</span>
              <strong>{6 - roomState.liberalPolicies}</strong>
            </div>
            <div className="board-tile">
              <span><PolicyLabel policy="Fascist" /> left</span>
              <strong>{11 - roomState.fascistPolicies}</strong>
            </div>
            <div className="board-tile">
              <span>Draw pile</span>
              <strong>{roomState.drawPileSize}</strong>
            </div>
            <div className="board-tile">
              <span>Discard pile</span>
              <strong>{roomState.discardPileSize}</strong>
            </div>
            <div className="board-tile">
              <span>Election tracker</span>
              <strong>{roomState.electionTracker}</strong>
            </div>
            <div className="board-tile">
              <span>Pending votes</span>
              <strong>{roomState.pendingVotes}</strong>
            </div>
          </div>
        </article>

        <article className="panel compact-panel log-panel">
          <h2>Round Log</h2>
          {roundLog.length > 0 ? (
            <div className="log-scroll">
              <table className="round-log-table">
                <thead>
                  <tr>
                    <th>Round</th>
                    <th>President</th>
                    <th>Chancellor</th>
                    <th style={{ textAlign: 'right' }}>Enacted</th>
                  </tr>
                </thead>
                <tbody>
                  {roundLog.map((entry) => (
                    <tr key={entry.round}>
                      <td className="log-round">#{entry.round}</td>
                      <td className="log-name">{entry.president}</td>
                      <td className="log-name">{entry.chancellor}</td>
                      <td className="log-policy">
                        <PolicyLabel policy={entry.enactedPolicy} />
                        {entry.specialEvent && (
                          <div className="log-special">{entry.specialEvent}</div>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          ) : (
            <p className="helper">No enacted policies yet.</p>
          )}
        </article>

      </section>
      )}
    </main>
  );
}
