#include "test_support.hpp"

namespace {

void setup_and_visibility_for_nine_players() {
    auto game = make_started_game(9, 9001);
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
    });

    require(game.config().has_value(), "nine-player config should exist");
    require(game.config()->player_count == 9, "config should record nine players");
    require(game.config()->liberal_count == 5, "nine-player game should have five liberals");
    require(game.config()->non_hitler_fascist_count == 3, "nine-player game should have three fascists");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in nine-player game");

    const auto fascist_view = game.player_view("2");
    require(fascist_view.has_value(), "fascist view should exist");
    require(fascist_view->known_hitler.has_value(), "fascist should know Hitler");
    require(*fascist_view->known_hitler == "1", "fascist should see player 1 as Hitler");
    require(fascist_view->known_fascists.size() == 2U, "fascist should see the other two fascists");
}

void first_fascist_policy_triggers_investigation_in_nine_player_game() {
    auto game = make_started_game(9, 9002);
    game.test_set_policy_counts(0, 0);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Liberal, sh::Policy::Liberal},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "first fascist policy should trigger investigation in nine-player game");
    require(game.pending_executive_power() == sh::ExecutivePower::InvestigateLoyalty, "investigation should be pending");
    require(game.investigate_player("5"), "investigation should resolve");
    require(game.phase() == sh::GamePhase::Election, "investigation should return game to election");
}

void hitler_election_loss_path_in_nine_player_game() {
    auto game = make_started_game(9, 9003);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });
    game.test_set_policy_counts(0, 3);

    elect_government(game, "2");

    require(game.phase() == sh::GamePhase::Complete, "Hitler election should end the game");
    require(game.winner().has_value(), "winner should be recorded");
    require(*game.winner() == "fascists", "fascists should win when Hitler is elected as chancellor");
}

}  // namespace

void run_players_9_tests() {
    setup_and_visibility_for_nine_players();
    first_fascist_policy_triggers_investigation_in_nine_player_game();
    hitler_election_loss_path_in_nine_player_game();
}
