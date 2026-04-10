#include "test_support.hpp"

namespace {

void setup_and_visibility_for_six_players() {
    auto game = make_started_game(6, 6001);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(game.config().has_value(), "six-player config should exist");
    require(game.config()->player_count == 6, "config should record six players");
    require(game.config()->liberal_count == 4, "six-player game should have four liberals");
    require(game.config()->non_hitler_fascist_count == 1, "six-player game should have one fascist");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in six-player game");

    const auto hitler_view = game.player_view("1");
    const auto fascist_view = game.player_view("2");
    require(hitler_view.has_value(), "Hitler view should exist");
    require(fascist_view.has_value(), "fascist view should exist");
    require(hitler_view->known_fascists.empty(), "Hitler should not know the fascist in a six-player game");
    require(fascist_view->known_fascists == vector<string>{"1"}, "fascist should see Hitler");
}

void term_limits_apply_to_previous_president_in_six_player_game() {
    auto game = make_started_game(6, 6002);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Hitler,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Liberal, sh::Policy::Fascist},
        2,
        0
    );

    require(game.is_term_limited("1"), "previous president should be term-limited in six-player game");
    require(game.is_term_limited("2"), "previous chancellor should be term-limited");
    require(!game.can_nominate_chancellor("1"), "previous president should not be a valid nominee");
    require(!game.can_nominate_chancellor("2"), "previous chancellor should not be a valid nominee");
}

void liberal_win_path_in_six_player_game() {
    auto game = make_started_game(6, 6003);
    game.test_set_policy_counts(4, 0);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Fascist, sh::Policy::Fascist},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::Complete, "fifth liberal policy should end the game");
    require(game.winner().has_value(), "winner should be set");
    require(*game.winner() == "liberals", "liberals should win at five liberal policies");
}

void failed_government_rotation_and_tracker_in_six_player_game() {
    auto game = make_started_game(6, 6004);

    fail_government(game, "2");
    require(game.public_state().election_tracker == 1, "first failed government should increment tracker");
    require(game.president_id().has_value(), "next president should exist");
    require(*game.president_id() == "2", "presidency should rotate after failed government");

    fail_government(game, "3");
    require(game.public_state().election_tracker == 2, "second failed government should increment tracker");
    require(game.president_id().has_value(), "next president should still exist");
    require(*game.president_id() == "3", "presidency should continue rotating");
}

void top_deck_policy_can_end_game_in_six_player_game() {
    auto game = make_started_game(6, 6005);
    game.test_set_policy_counts(4, 0);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Liberal}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.phase() == sh::GamePhase::Complete, "top-deck fifth liberal policy should end the game");
    require(game.winner().has_value(), "winner should be set");
    require(*game.winner() == "liberals", "liberals should win after fifth top-deck liberal policy");
}

void invalid_legislative_actions_are_rejected_in_six_player_game() {
    auto game = make_started_game(6, 6006);

    require(!game.president_discards_policy(0), "cannot discard before legislative session");
    require(!game.chancellor_enacts_policy(0), "cannot enact before legislative session");
    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Ja);
    require(game.resolve_election(), "election should resolve");
    require(!game.chancellor_enacts_policy(0), "chancellor cannot act before president discards");
    require(!game.president_discards_policy(3), "president cannot discard out-of-range index");
    require(game.president_discards_policy(0), "president should be able to discard valid card");
    require(!game.president_discards_policy(0), "president cannot discard twice");
    require(!game.chancellor_enacts_policy(2), "chancellor cannot enact out-of-range index");
    require(game.chancellor_enacts_policy(0), "chancellor should enact valid card");
}

void dead_candidate_cannot_be_nominated_in_six_player_game() {
    auto game = make_started_game(6, 6007);
    game.test_set_policy_counts(0, 3);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    run_legislative_session(
        game,
        "2",
        {
            sh::Policy::Fascist,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
        },
        2,
        0
    );
    require(game.execute_player("5"), "execution should succeed");
    require(!game.players()[4].alive, "player 5 should be dead");
    require(!game.can_nominate_chancellor("5"), "dead player should not be a valid nominee");
    require(!game.nominate_chancellor("5"), "dead player should not be nominatable");
}

void public_state_reports_expected_sizes_in_six_player_game() {
    auto game = make_started_game(6, 6008);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Ja);
    const auto voting_state = game.public_state();
    require(voting_state.pending_votes == 6, "public state should report all cast votes");
    require(game.resolve_election(), "election should resolve");
    const auto legislative_state = game.public_state();
    require(legislative_state.president_hand_size == 3, "public state should report president hand size");
    require(legislative_state.chancellor_hand_size == 0, "chancellor should not yet have cards");
    require(game.president_discards_policy(0), "president should discard one card");
    const auto post_discard_state = game.public_state();
    require(post_discard_state.president_hand_size == 0, "president hand size should clear");
    require(post_discard_state.chancellor_hand_size == 2, "chancellor hand size should report two cards");
}

void hitler_election_loss_path_in_six_player_game() {
    auto game = make_started_game(6, 6009);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });
    game.test_set_policy_counts(0, 3);

    elect_government(game, "2");
    require(game.phase() == sh::GamePhase::Complete, "Hitler election should end the game");
    require(game.winner().has_value(), "winner should be recorded");
    require(*game.winner() == "fascists", "fascists should win when Hitler is elected after three fascist policies");
}

void dead_players_are_not_counted_for_majority_in_six_player_game() {
    auto game = make_started_game(6, 6010);
    game.test_set_policy_counts(0, 3);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    run_legislative_session(
        game,
        "2",
        {
            sh::Policy::Fascist,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
        },
        2,
        0
    );
    require(game.execute_player("6"), "execution should succeed");
    require(!game.players()[5].alive, "player 6 should be dead");

    require(game.president_id().has_value(), "next president should be assigned");
    require(*game.president_id() == "2", "presidency should rotate to player 2");
    require(game.nominate_chancellor("3"), "nomination should succeed");
    require(game.cast_vote("1", sh::Vote::Ja), "vote should be accepted");
    require(game.cast_vote("2", sh::Vote::Ja), "president vote should be accepted");
    require(game.cast_vote("3", sh::Vote::Ja), "vote should be accepted");
    require(game.cast_vote("4", sh::Vote::Nein), "vote should be accepted");
    require(game.cast_vote("5", sh::Vote::Nein), "vote should be accepted");
    require(game.resolve_election(), "election should resolve with only living players counted");
    require(game.phase() == sh::GamePhase::LegislativeSession, "3-2 among living players should pass");
}

void top_deck_fascist_can_trigger_policy_peek_in_six_player_game() {
    auto game = make_started_game(6, 6011);
    game.test_set_policy_counts(0, 2);
    game.test_set_draw_pile(stacked_draw({
        sh::Policy::Fascist,
        sh::Policy::Liberal,
        sh::Policy::Fascist,
        sh::Policy::Liberal,
    }));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().fascist_policies == 3, "top-deck fascist should advance fascist board");
    require(game.phase() == sh::GamePhase::Election, "policy peek should resolve immediately back to election");
    const auto peek_owner_view = game.player_view("3");
    require(peek_owner_view.has_value(), "peek owner view should exist");
    require(peek_owner_view->policy_peek.size() == 3U, "president at top-deck time should see peeked policies");
}

void previous_officeholders_are_tracked_after_successful_government_in_six_players() {
    auto game = make_started_game(6, 6012);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Fascist, sh::Policy::Fascist},
        1,
        0
    );

    require(game.last_president_id().has_value(), "last president should be tracked");
    require(game.last_chancellor_id().has_value(), "last chancellor should be tracked");
    require(*game.last_president_id() == "1", "player 1 should be previous president");
    require(*game.last_chancellor_id() == "2", "player 2 should be previous chancellor");
}

void vote_updates_do_not_duplicate_pending_votes_in_six_players() {
    auto game = make_started_game(6, 6013);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    require(game.cast_vote("3", sh::Vote::Ja), "first vote should be accepted");
    require(game.cast_vote("3", sh::Vote::Nein), "updated vote should be accepted");
    require(game.public_state().pending_votes == 1, "updating a vote should not add a duplicate pending vote");
}

void unknown_and_dead_players_cannot_be_executive_targets_in_six_players() {
    auto game = make_started_game(6, 6014);
    game.test_set_policy_counts(0, 3);
    run_legislative_session(
        game,
        "2",
        {
            sh::Policy::Fascist,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
        },
        2,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "execution should be pending");
    require(!game.execute_player("99"), "unknown player cannot be executed");
    require(game.execute_player("6"), "valid execution should succeed");
    require(!game.execute_player("6"), "dead player cannot be executed twice");
}

void public_state_clears_pending_votes_after_resolution_in_six_players() {
    auto game = make_started_game(6, 6015);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Nein);
    require(game.public_state().pending_votes == 6, "pending vote count should reflect cast votes");
    require(game.resolve_election(), "failed election should resolve");
    require(game.public_state().pending_votes == 0, "pending votes should clear after election resolves");
}

void cannot_nominate_or_vote_in_legislative_session_in_six_players() {
    auto game = make_started_game(6, 6016);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Ja);
    require(game.resolve_election(), "election should resolve");
    require(game.phase() == sh::GamePhase::LegislativeSession, "game should be in legislative session");
    require(!game.nominate_chancellor("3"), "cannot nominate during legislative session");
    require(!game.cast_vote("1", sh::Vote::Ja), "cannot vote during legislative session");
}

void previous_officeholder_limits_update_after_new_successful_government_in_six_players() {
    auto game = make_started_game(6, 6017);

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Liberal, sh::Policy::Fascist},
        2,
        0
    );

    require(game.is_term_limited("1"), "first president should be term-limited after first government");
    require(game.is_term_limited("2"), "first chancellor should be term-limited after first government");

    run_legislative_session(
        game,
        "4",
        {sh::Policy::Liberal, sh::Policy::Fascist, sh::Policy::Fascist},
        1,
        0
    );

    require(!game.is_term_limited("1"), "older president should no longer be term-limited after a new government");
    require(game.is_term_limited("2"), "player 2 should remain term-limited as the new previous president");
    require(!game.is_term_limited("3"), "player 3 should not be term-limited in this sequence");
    require(game.is_term_limited("4"), "new previous chancellor should now be term-limited");
}

void unknown_player_operations_are_rejected_in_six_players() {
    auto game = make_started_game(6, 6018);

    require(!game.nominate_chancellor("99"), "unknown player cannot be nominated");
    require(!game.cast_vote("99", sh::Vote::Ja), "unknown player cannot vote");
    require(!game.player_view("99").has_value(), "unknown player view should be absent");
}

void successful_election_resets_tracker_in_six_players() {
    auto game = make_started_game(6, 6019);

    fail_government(game, "2");
    fail_government(game, "3");
    require(game.public_state().election_tracker == 2, "two failed governments should set tracker to two");

    elect_government(game, "4");
    require(game.public_state().election_tracker == 0, "successful government should reset election tracker");
}

void policy_peek_is_visible_only_to_owner_in_six_players() {
    auto game = make_started_game(6, 6020);
    game.test_set_policy_counts(0, 2);
    run_legislative_session(
        game,
        "2",
        {
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
        },
        1,
        0
    );

    const auto president_view = game.player_view("1");
    const auto chancellor_view = game.player_view("2");
    const auto spectator_view = game.player_view("3");
    require(president_view.has_value(), "president view should exist");
    require(chancellor_view.has_value(), "chancellor view should exist");
    require(spectator_view.has_value(), "spectator view should exist");
    require(president_view->policy_peek.size() == 3U, "president should own policy peek");
    require(chancellor_view->policy_peek.empty(), "chancellor should not see policy peek");
    require(spectator_view->policy_peek.empty(), "spectator should not see policy peek");
}

void top_deck_fourth_fascist_triggers_execution_in_six_players() {
    auto game = make_started_game(6, 6021);
    game.test_set_policy_counts(0, 3);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Fascist}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().fascist_policies == 4, "top-deck fascist should advance to fourth fascist policy");
    require(game.phase() == sh::GamePhase::ExecutiveAction, "fourth fascist should trigger execution");
    require(game.pending_executive_power() == sh::ExecutivePower::Execution, "execution should be pending");
}

void failed_election_clears_nomination_in_six_players() {
    auto game = make_started_game(6, 6022);

    require(game.nominate_chancellor("2"), "initial nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Nein);
    require(game.resolve_election(), "failed election should resolve");
    require(!game.chancellor_id().has_value(), "failed election should clear chancellor nomination");
    require(game.nominate_chancellor("3"), "next round should allow a new nomination");
}

}  // namespace

void run_players_6_tests() {
    setup_and_visibility_for_six_players();
    term_limits_apply_to_previous_president_in_six_player_game();
    liberal_win_path_in_six_player_game();
    failed_government_rotation_and_tracker_in_six_player_game();
    top_deck_policy_can_end_game_in_six_player_game();
    invalid_legislative_actions_are_rejected_in_six_player_game();
    dead_candidate_cannot_be_nominated_in_six_player_game();
    public_state_reports_expected_sizes_in_six_player_game();
    hitler_election_loss_path_in_six_player_game();
    dead_players_are_not_counted_for_majority_in_six_player_game();
    top_deck_fascist_can_trigger_policy_peek_in_six_player_game();
    previous_officeholders_are_tracked_after_successful_government_in_six_players();
    vote_updates_do_not_duplicate_pending_votes_in_six_players();
    unknown_and_dead_players_cannot_be_executive_targets_in_six_players();
    public_state_clears_pending_votes_after_resolution_in_six_players();
    cannot_nominate_or_vote_in_legislative_session_in_six_players();
    previous_officeholder_limits_update_after_new_successful_government_in_six_players();
    unknown_player_operations_are_rejected_in_six_players();
    successful_election_resets_tracker_in_six_players();
    policy_peek_is_visible_only_to_owner_in_six_players();
    top_deck_fourth_fascist_triggers_execution_in_six_players();
    failed_election_clears_nomination_in_six_players();
}
