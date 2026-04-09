#pragma once

#include "sh/game.hpp"

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

inline void require(bool condition, string_view message) {
    if (!condition) {
        cerr << "Test failure: " << message << '\n';
        exit(1);
    }
}

inline sh::Game make_started_game(int player_count, uint32_t seed = 12345) {
    sh::Game game("ABCDE");
    game.test_seed_rng(seed);
    for (int i = 1; i <= player_count; ++i) {
        require(game.add_player(to_string(i), "Player " + to_string(i)), "adding player should succeed");
    }
    require(game.start(), "game should start");
    return game;
}

inline vector<sh::Policy> stacked_draw(initializer_list<sh::Policy> next_cards) {
    vector<sh::Policy> pile;
    pile.reserve(next_cards.size());
    for (auto it = next_cards.end(); it != next_cards.begin();) {
        --it;
        pile.push_back(*it);
    }
    return pile;
}

inline void set_roles(sh::Game& game, initializer_list<sh::Role> roles) {
    game.test_set_roles(vector<sh::Role>(roles));
}

inline int count_role(const sh::Game& game, sh::Role role) {
    int count = 0;
    for (const auto& player : game.players()) {
        if (player.identity == role) {
            ++count;
        }
    }
    return count;
}

inline string first_player_with_role(const sh::Game& game, sh::Role role, int occurrence = 0) {
    for (const auto& player : game.players()) {
        if (player.identity == role) {
            if (occurrence == 0) {
                return player.id;
            }
            --occurrence;
        }
    }
    return "";
}

inline vector<string> players_with_role(const sh::Game& game, sh::Role role) {
    vector<string> ids;
    for (const auto& player : game.players()) {
        if (player.identity == role) {
            ids.push_back(player.id);
        }
    }
    return ids;
}

inline void cast_votes_for_all_alive(sh::Game& game, sh::Vote vote) {
    for (const auto& player : game.players()) {
        if (player.alive) {
            require(game.cast_vote(player.id, vote), "vote should be accepted");
        }
    }
}

inline void elect_government(sh::Game& game, const string& nominee, sh::Vote vote = sh::Vote::Ja) {
    require(game.nominate_chancellor(nominee), "nomination should succeed");
    cast_votes_for_all_alive(game, vote);
    require(game.resolve_election(), "election should resolve");
}

inline void fail_government(sh::Game& game, const string& nominee) {
    require(game.nominate_chancellor(nominee), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Nein);
    require(game.resolve_election(), "failed election should resolve");
}

inline void run_legislative_session(
    sh::Game& game,
    const string& nominee,
    initializer_list<sh::Policy> next_cards,
    size_t president_discard_index,
    size_t chancellor_enact_index
) {
    game.test_set_draw_pile(stacked_draw(next_cards));
    elect_government(game, nominee);
    require(game.phase() == sh::GamePhase::LegislativeSession, "game should enter legislative session");
    require(game.president_discards_policy(president_discard_index), "president should discard one policy");
    require(game.chancellor_enacts_policy(chancellor_enact_index), "chancellor should enact one policy");
}

inline void resolve_pending_power(
    sh::Game& game,
    const string& investigate_target,
    const string& special_election_target,
    const string& execution_target
) {
    if (game.phase() != sh::GamePhase::ExecutiveAction || !game.pending_executive_power().has_value()) {
        return;
    }

    switch (*game.pending_executive_power()) {
        case sh::ExecutivePower::InvestigateLoyalty:
            require(game.investigate_player(investigate_target), "investigation should succeed");
            break;
        case sh::ExecutivePower::SpecialElection:
            require(game.call_special_election(special_election_target), "special election should succeed");
            break;
        case sh::ExecutivePower::Execution:
            require(game.execute_player(execution_target), "execution should succeed");
            break;
        case sh::ExecutivePower::None:
        case sh::ExecutivePower::PolicyPeek:
            break;
    }
}

