#pragma once

#include "sh/game.hpp"

#include <crow/websocket.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace sh {

struct Session {
    std::string room_code;
    std::string player_id;
    std::string player_name;
};

struct Room {
    explicit Room(std::string code)
        : room_code(std::move(code)),
          game(std::make_shared<Game>(room_code)) {}

    std::string room_code;
    std::shared_ptr<Game> game;
    std::optional<std::string> host_player_id;
    std::unordered_map<std::string, crow::websocket::connection*> connections_by_player;
    std::unordered_map<crow::websocket::connection*, std::string> players_by_connection;

    void remove_connection(crow::websocket::connection* connection) {
        const auto player_it = players_by_connection.find(connection);
        if (player_it == players_by_connection.end()) {
            return;
        }

        const std::string player_id = player_it->second;
        players_by_connection.erase(player_it);

        const auto connection_it = connections_by_player.find(player_id);
        if (connection_it != connections_by_player.end() && connection_it->second == connection) {
            connections_by_player.erase(connection_it);
        }
    }

    void reassign_host_if_needed() {
        if (host_player_id.has_value()) {
            const auto existing_host = connections_by_player.find(*host_player_id);
            if (existing_host != connections_by_player.end() && existing_host->second != nullptr) {
                return;
            }
        }

        host_player_id.reset();
        for (const auto& player : game->players()) {
            if (!player.connected) {
                continue;
            }

            const auto connection_it = connections_by_player.find(player.id);
            if (connection_it == connections_by_player.end() || connection_it->second == nullptr) {
                continue;
            }

            host_player_id = player.id;
            return;
        }
    }
};

class RoomRegistry {
public:
    std::shared_ptr<Room> get_or_create(const std::string& room_code) {
        if (const auto it = rooms_.find(room_code); it != rooms_.end()) {
            return it->second;
        }

        auto room = std::make_shared<Room>(room_code);
        rooms_.emplace(room_code, room);
        return room;
    }

    std::shared_ptr<Room> find(const std::string& room_code) const {
        if (const auto it = rooms_.find(room_code); it != rooms_.end()) {
            return it->second;
        }
        return nullptr;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Room>> rooms_;
};

}  // namespace sh
