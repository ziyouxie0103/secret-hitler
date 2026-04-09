#include "test_support.hpp"

namespace {

void setup_and_visibility_for_eight_players() {
    auto game = make_started_game(8, 8001);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(game.config().has_value(), "eight-player config should exist");
    require(game.config()->player_count == 8, "config should record eight players");
    require(game.config()->liberal_count == 5, "eight-player game should have five liberals");
    require(game.config()->non_hitler_fascist_count == 2, "eight-player game should have two fascists");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in eight-player game");
}

void third_failed_government_can_trigger_executive_action_in_eight_player_game() {
    auto game = make_started_game(8, 8002);
    game.test_set_policy_counts(0, 1);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Fascist}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().fascist_policies == 2, "top-deck fascist should advance fascist track");
    require(game.phase() == sh::GamePhase::ExecutiveAction, "top-deck fascist may trigger executive action");
    require(game.pending_executive_power() == sh::ExecutivePower::InvestigateLoyalty, "second fascist should trigger investigation");
    require(game.investigate_player("5"), "investigation should resolve after top-deck executive action");
    require(game.phase() == sh::GamePhase::Election, "game should return to election after investigation");
}

}  // namespace

void run_players_8_tests() {
    setup_and_visibility_for_eight_players();
    third_failed_government_can_trigger_executive_action_in_eight_player_game();
}
