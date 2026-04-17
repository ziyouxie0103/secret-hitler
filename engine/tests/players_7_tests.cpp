#include "test_support.hpp"

namespace {

void setup_and_visibility_for_seven_players() {
    auto game = make_started_game(7, 7001);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(game.config().has_value(), "seven-player config should exist");
    require(game.config()->player_count == 7, "config should record seven players");
    require(game.config()->liberal_count == 4, "seven-player game should have four liberals");
    require(game.config()->non_hitler_fascist_count == 2, "seven-player game should have two fascists");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in seven-player game");

    const auto fascist_view = game.player_view("2");
    const auto hitler_view = game.player_view("1");
    require(fascist_view.has_value(), "fascist view should exist");
    require(hitler_view.has_value(), "Hitler view should exist");
    require(fascist_view->known_hitler.has_value(), "fascist should know Hitler");
    require(*fascist_view->known_hitler == "1", "fascist should see player 1 as Hitler");
    require(fascist_view->known_fascists.size() == 1U, "fascist should see the other fascist");
    require(fascist_view->known_fascists.front() == "3", "fascist should see player 3 as the other fascist");
    require(hitler_view->known_fascists.empty(), "Hitler should not know fascists");
}

void investigate_power_path_in_seven_player_game() {
    auto game = make_started_game(7, 7002);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });
    game.test_set_policy_counts(0, 1);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Liberal, sh::Policy::Liberal},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "second fascist policy should trigger investigation");
    require(game.pending_executive_power() == sh::ExecutivePower::InvestigateLoyalty, "investigation should be pending");
    require(game.investigate_player("4"), "president should be able to investigate player 4");
    require(game.phase() == sh::GamePhase::Election, "investigation should return game to election");
    const auto president_view = game.player_view("1");
    const auto target_view = game.player_view("4");
    require(president_view->investigation_result.has_value(), "president should receive investigation result");
    require(!target_view->investigation_result.has_value(), "investigated player should not see investigation result");
}

void special_election_path_in_seven_player_game() {
    auto game = make_started_game(7, 7003);
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
    require(game.call_special_election("5"), "president should be able to choose a special-election president");
    require(game.phase() == sh::GamePhase::Election, "special election should return to election phase");
    require(game.president_id().has_value(), "temporary president should be set");
    require(*game.president_id() == "5", "chosen player should become temporary president");

    run_legislative_session(
        game,
        "6",
        {sh::Policy::Liberal, sh::Policy::Liberal, sh::Policy::Fascist},
        2,
        0
    );

    require(game.phase() == sh::GamePhase::Election, "after temporary presidency, game should return to election");
    require(game.president_id().has_value(), "next normal president should be set");
    require(*game.president_id() == "2", "presidency should resume with the player after the original president");
}

}  // namespace

void run_players_7_tests() {
    setup_and_visibility_for_seven_players();
    investigate_power_path_in_seven_player_game();
    special_election_path_in_seven_player_game();
}
