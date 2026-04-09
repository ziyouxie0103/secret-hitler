#include "test_support.hpp"

namespace {

void setup_and_visibility_for_ten_players() {
    auto game = make_started_game(10, 10001);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(game.config().has_value(), "ten-player config should exist");
    require(game.config()->player_count == 10, "config should record ten players");
    require(game.config()->liberal_count == 6, "ten-player game should have six liberals");
    require(game.config()->non_hitler_fascist_count == 3, "ten-player game should have three fascists");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in ten-player game");
}

void fascist_win_path_in_ten_player_game() {
    auto game = make_started_game(10, 10002);
    game.test_set_policy_counts(0, 5);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Liberal, sh::Policy::Liberal},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::Complete, "sixth fascist policy should end the game");
    require(game.winner().has_value(), "winner should be recorded");
    require(*game.winner() == "fascists", "fascists should win at six fascist policies");
}

void special_election_track_exists_in_ten_player_game() {
    auto game = make_started_game(10, 10003);
    game.test_set_policy_counts(0, 2);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Liberal, sh::Policy::Liberal},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "third fascist policy should trigger special election");
    require(game.pending_executive_power() == sh::ExecutivePower::SpecialElection, "special election should be pending");
    require(game.call_special_election("7"), "special election should allow choosing a temporary president");
    require(game.phase() == sh::GamePhase::Election, "temporary president should lead an election");
    require(game.president_id().has_value(), "temporary president should be assigned");
    require(*game.president_id() == "7", "chosen player should become temporary president");
}

}  // namespace

void run_players_10_tests() {
    setup_and_visibility_for_ten_players();
    fascist_win_path_in_ten_player_game();
    special_election_track_exists_in_ten_player_game();
}
