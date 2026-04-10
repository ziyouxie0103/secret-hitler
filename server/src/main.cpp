#include "sh/room_registry.hpp"

#include <algorithm>
#include <boost/asio.hpp>
#include <crow.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

std::string to_string(sh::GamePhase phase) {
    switch (phase) {
        case sh::GamePhase::Lobby:
            return "lobby";
        case sh::GamePhase::Election:
            return "election";
        case sh::GamePhase::LegislativeSession:
            return "legislative_session";
        case sh::GamePhase::ExecutiveAction:
            return "executive_action";
        case sh::GamePhase::Complete:
            return "complete";
    }

    return "unknown";
}

std::string to_string(sh::Role role) {
    switch (role) {
        case sh::Role::Liberal:
            return "liberal";
        case sh::Role::Fascist:
            return "fascist";
        case sh::Role::Hitler:
            return "hitler";
    }

    return "unknown";
}

std::string to_string(sh::Party party) {
    switch (party) {
        case sh::Party::Liberal:
            return "liberal";
        case sh::Party::Fascist:
            return "fascist";
    }

    return "unknown";
}

std::string to_string(sh::ExecutivePower power) {
    switch (power) {
        case sh::ExecutivePower::None:
            return "none";
        case sh::ExecutivePower::InvestigateLoyalty:
            return "investigate_loyalty";
        case sh::ExecutivePower::SpecialElection:
            return "special_election";
        case sh::ExecutivePower::PolicyPeek:
            return "policy_peek";
        case sh::ExecutivePower::Execution:
            return "execution";
    }

    return "unknown";
}

std::string to_string(sh::Policy policy) {
    switch (policy) {
        case sh::Policy::Liberal:
            return "liberal";
        case sh::Policy::Fascist:
            return "fascist";
    }

    return "unknown";
}

std::optional<sh::Vote> parse_vote(const std::string& value) {
    if (value == "ja") {
        return sh::Vote::Ja;
    }
    if (value == "nein") {
        return sh::Vote::Nein;
    }
    return std::nullopt;
}

json policies_to_json(const std::vector<sh::Policy>& policies) {
    json result = json::array();
    for (const auto policy : policies) {
        result.push_back(to_string(policy));
    }
    return result;
}

json to_json(const sh::PublicGameState& state) {
    json players = json::array();
    for (const auto& player : state.players) {
        players.push_back({
            {"id", player.id},
            {"name", player.name},
            {"connected", player.connected},
            {"alive", player.alive},
        });
    }

    return {
        {"roomCode", state.room_code},
        {"phase", to_string(state.phase)},
        {"players", players},
        {"liberalPolicies", state.liberal_policies},
        {"fascistPolicies", state.fascist_policies},
        {"electionTracker", state.election_tracker},
        {"presidentId", state.president_id.value_or("")},
        {"chancellorId", state.chancellor_id.value_or("")},
        {"lastPresidentId", state.last_president_id.value_or("")},
        {"lastChancellorId", state.last_chancellor_id.value_or("")},
        {"winner", state.winner.value_or("")},
        {"drawPileSize", state.draw_pile_size},
        {"discardPileSize", state.discard_pile_size},
        {"pendingVotes", state.pending_votes},
        {"presidentHandSize", state.president_hand_size},
        {"chancellorHandSize", state.chancellor_hand_size},
        {"pendingExecutivePower", state.pending_executive_power.has_value()
            ? to_string(*state.pending_executive_power)
            : ""},
    };
}

json to_json(const sh::Room& room) {
    json payload = to_json(room.game->public_state());
    payload["hostPlayerId"] = room.host_player_id.value_or("");
    return payload;
}

json to_json(const sh::PlayerView& view) {
    return {
        {"playerId", view.player_id},
        {"playerName", view.player_name},
        {"role", to_string(view.role)},
        {"party", to_string(view.party)},
        {"alive", view.alive},
        {"knownFascists", view.known_fascists},
        {"legislativeHand", policies_to_json(view.legislative_hand)},
        {"policyPeek", policies_to_json(view.policy_peek)},
        {"investigationResult", view.investigation_result.has_value()
            ? to_string(*view.investigation_result)
            : ""},
    };
}

void send_error(crow::websocket::connection& conn, const std::string& message) {
    conn.send_text(json{
        {"type", "error"},
        {"message", message},
    }.dump());
}

crow::response json_response(const json& payload, int code = 200) {
    crow::response response{code, payload.dump()};
    response.set_header("Content-Type", "application/json");
    response.set_header("Access-Control-Allow-Origin", "*");
    return response;
}

void send_public_state(const std::shared_ptr<sh::Room>& room) {
    const json payload = {
        {"type", "room_state"},
        {"payload", to_json(*room)},
    };

    for (const auto& [player_id, connection] : room->connections_by_player) {
        if (connection != nullptr) {
            connection->send_text(payload.dump());
        }
    }
}

void send_private_view(const std::shared_ptr<sh::Room>& room, const std::string& player_id) {
    const auto connection_it = room->connections_by_player.find(player_id);
    if (connection_it == room->connections_by_player.end() || connection_it->second == nullptr) {
        return;
    }

    const auto view = room->game->player_view(player_id);
    if (!view.has_value()) {
        return;
    }

    connection_it->second->send_text(json{
        {"type", "player_view"},
        {"payload", to_json(*view)},
    }.dump());
}

void sync_room_state(const std::shared_ptr<sh::Room>& room) {
    send_public_state(room);
    for (const auto& [player_id, connection] : room->connections_by_player) {
        if (connection != nullptr) {
            send_private_view(room, player_id);
        }
    }
}

std::shared_ptr<sh::Room> require_room_for_session(
    sh::RoomRegistry& rooms,
    const std::unordered_map<crow::websocket::connection*, sh::Session>& sessions,
    crow::websocket::connection& conn
) {
    const auto session_it = sessions.find(&conn);
    if (session_it == sessions.end() || session_it->second.room_code.empty()) {
        return nullptr;
    }

    auto room = rooms.find(session_it->second.room_code);
    if (!room) {
        return nullptr;
    }

    const auto connection_it = room->connections_by_player.find(session_it->second.player_id);
    if (connection_it == room->connections_by_player.end() || connection_it->second != &conn) {
        return nullptr;
    }

    return room;
}

bool is_current_president(const std::shared_ptr<sh::Room>& room, const std::string& player_id) {
    return room->game->president_id().has_value() && *room->game->president_id() == player_id;
}

bool is_current_chancellor(const std::shared_ptr<sh::Room>& room, const std::string& player_id) {
    return room->game->chancellor_id().has_value() && *room->game->chancellor_id() == player_id;
}

}  // namespace

int main() {
    crow::SimpleApp app;
    sh::RoomRegistry rooms;
    std::mutex rooms_mutex;
    std::unordered_map<crow::websocket::connection*, sh::Session> sessions;

    CROW_ROUTE(app, "/api/health")([] {
        return json_response(json{{"ok", true}});
    });

    CROW_ROUTE(app, "/api/room/<string>")
    ([&rooms, &rooms_mutex](const std::string& room_code) {
        std::scoped_lock lock(rooms_mutex);
        auto room = rooms.get_or_create(room_code);
        return json_response(to_json(*room));
    });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&rooms_mutex, &sessions](crow::websocket::connection& conn) {
            std::scoped_lock lock(rooms_mutex);
            sessions[&conn] = sh::Session{};
        })
        .onclose([&rooms, &rooms_mutex, &sessions](crow::websocket::connection& conn, const std::string&, std::uint16_t) {
            std::scoped_lock lock(rooms_mutex);
            const auto session_it = sessions.find(&conn);
            if (session_it == sessions.end()) {
                return;
            }

            if (!session_it->second.room_code.empty() && !session_it->second.player_id.empty()) {
                if (auto room = rooms.find(session_it->second.room_code)) {
                    room->remove_connection(&conn);
                    room->game->set_player_connected(session_it->second.player_id, false);
                    room->reassign_host_if_needed();
                    sync_room_state(room);
                }
            }

            sessions.erase(session_it);
        })
        .onmessage([&rooms, &rooms_mutex, &sessions](crow::websocket::connection& conn,
                                                      const std::string& data,
                                                      bool /*is_binary*/) {
            const auto message = json::parse(data, nullptr, false);
            if (message.is_discarded()) {
                send_error(conn, "invalid_json");
                return;
            }

            const auto type = message.value("type", "");
            std::scoped_lock lock(rooms_mutex);

            if (type == "join_room") {
                const auto room_code = message.value("roomCode", "DEMO1");
                const auto player_id = message.value("playerId", "");
                const auto name = message.value("name", "Anonymous");
                if (player_id.empty()) {
                    send_error(conn, "missing_player_id");
                    return;
                }

                auto room = rooms.get_or_create(room_code);
                const bool player_exists = std::any_of(
                    room->game->players().begin(),
                    room->game->players().end(),
                    [&player_id](const sh::Player& player) {
                        return player.id == player_id;
                    }
                );

                if (!player_exists && !room->game->add_player(player_id, name)) {
                    if (room->game->phase() != sh::GamePhase::Lobby) {
                        send_error(conn, "room_already_started");
                    } else if (room->game->player_count() >= 10U) {
                        send_error(conn, "room_full");
                    } else {
                        send_error(conn, "unable_to_join_room");
                    }
                    return;
                }

                if (player_exists) {
                    room->game->set_player_name(player_id, name);
                }

                room->game->set_player_connected(player_id, true);
                if (const auto existing_connection = room->connections_by_player.find(player_id);
                    existing_connection != room->connections_by_player.end()
                    && existing_connection->second != nullptr
                    && existing_connection->second != &conn) {
                    room->players_by_connection.erase(existing_connection->second);
                }
                room->connections_by_player[player_id] = &conn;
                room->players_by_connection[&conn] = player_id;
                if (!room->host_player_id.has_value()) {
                    room->host_player_id = player_id;
                }

                sessions[&conn] = sh::Session{
                    .room_code = room_code,
                    .player_id = player_id,
                    .player_name = name,
                };

                conn.send_text(json{
                    {"type", "joined_room"},
                    {"payload", {
                        {"roomCode", room_code},
                        {"playerId", player_id},
                        {"hostPlayerId", room->host_player_id.value_or("")},
                    }},
                }.dump());
                sync_room_state(room);
                return;
            }

            auto room = require_room_for_session(rooms, sessions, conn);
            if (!room) {
                send_error(conn, "not_in_room");
                return;
            }

            const auto& session = sessions.at(&conn);

            if (type == "sync_state") {
                conn.send_text(json{
                    {"type", "sync_ack"},
                    {"message", "state_synced"},
                }.dump());
                sync_room_state(room);
                return;
            }

            if (type == "start_game") {
                if (!room->host_player_id.has_value() || *room->host_player_id != session.player_id) {
                    send_error(conn, "only_host_can_start");
                    return;
                }
                if (!room->game->start()) {
                    send_error(conn, "unable_to_start");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "nominate_chancellor") {
                if (!is_current_president(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto target_player_id = message.value("targetPlayerId", "");
                if (!room->game->nominate_chancellor(target_player_id)) {
                    send_error(conn, "invalid_nomination");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "cast_vote") {
                const auto vote_value = parse_vote(message.value("vote", ""));
                if (!vote_value.has_value()) {
                    send_error(conn, "invalid_vote");
                    return;
                }
                if (!room->game->cast_vote(session.player_id, *vote_value)) {
                    send_error(conn, "unable_to_cast_vote");
                    return;
                }
                if (room->game->all_votes_cast()) {
                    if (!room->game->resolve_election()) {
                        send_error(conn, "unable_to_resolve_election");
                        return;
                    }
                }
                sync_room_state(room);
                return;
            }

            if (type == "president_discard_policy") {
                if (!is_current_president(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto index = message.value("index", -1);
                if (index < 0 || !room->game->president_discards_policy(static_cast<size_t>(index))) {
                    send_error(conn, "invalid_president_discard");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "chancellor_enact_policy") {
                if (!is_current_chancellor(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto index = message.value("index", -1);
                if (index < 0 || !room->game->chancellor_enacts_policy(static_cast<size_t>(index))) {
                    send_error(conn, "invalid_chancellor_action");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "investigate_player") {
                if (!is_current_president(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto target_player_id = message.value("targetPlayerId", "");
                if (!room->game->investigate_player(target_player_id)) {
                    send_error(conn, "invalid_investigation");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "call_special_election") {
                if (!is_current_president(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto target_player_id = message.value("targetPlayerId", "");
                if (!room->game->call_special_election(target_player_id)) {
                    send_error(conn, "invalid_special_election");
                    return;
                }
                sync_room_state(room);
                return;
            }

            if (type == "execute_player") {
                if (!is_current_president(room, session.player_id)) {
                    send_error(conn, "not_your_turn");
                    return;
                }
                const auto target_player_id = message.value("targetPlayerId", "");
                if (!room->game->execute_player(target_player_id)) {
                    send_error(conn, "invalid_execution");
                    return;
                }
                sync_room_state(room);
                return;
            }

            send_error(conn, "unknown_message");
        });

    std::cout << "Secret Hitler server listening on http://localhost:18080\n";
    app.port(18080).multithreaded().run();
    return 0;
}
