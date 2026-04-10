#include "test_support.hpp"

namespace {

void setup_and_visibility_for_five_players() {
    auto game = make_started_game(5, 5001);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(game.config().has_value(), "five-player config should exist");
    require(game.config()->player_count == 5, "config should record five players");
    require(game.config()->liberal_count == 3, "five-player game should have three liberals");
    require(game.config()->non_hitler_fascist_count == 1, "five-player game should have one fascist");
    require(!game.config()->hitler_knows_fascists, "Hitler should not know fascists in five-player game");
    require(count_role(game, sh::Role::Hitler) == 1, "five-player game should have one Hitler");
    require(count_role(game, sh::Role::Fascist) == 1, "five-player game should have one fascist");
    require(count_role(game, sh::Role::Liberal) == 3, "five-player game should have three liberals");

    const auto hitler_view = game.player_view("1");
    const auto fascist_view = game.player_view("2");
    const auto liberal_view = game.player_view("3");
    require(hitler_view.has_value(), "Hitler view should exist");
    require(fascist_view.has_value(), "fascist view should exist");
    require(liberal_view.has_value(), "liberal view should exist");
    require(hitler_view->known_fascists.empty(), "Hitler should not know the fascist identity in a five-player game");
    require(fascist_view->known_fascists.size() == 1U, "fascist should see Hitler");
    require(fascist_view->known_fascists.front() == "1", "fascist should see player 1 as Hitler");
    require(liberal_view->known_fascists.empty(), "liberal should not see secret identities");
}

void policy_peek_is_private_in_five_player_game() {
    auto game = make_started_game(5, 5002);
    set_roles(game, {
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

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

    require(game.phase() == sh::GamePhase::Election, "policy peek should return game to election");
    require(!game.pending_executive_power().has_value(), "policy peek should not leave pending executive action");

    const auto president_view = game.player_view("1");
    require(president_view.has_value(), "president view should exist");
    require(president_view->policy_peek.size() == 3U, "president should see three peeked policies");
    require(game.public_state().policy_peek.empty(), "public state should not leak policy peek");
}

void hitler_elected_after_three_fascists_wins_for_fascists() {
    auto game = make_started_game(5, 5003);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    game.test_set_policy_counts(0, 3);
    elect_government(game, "2");

    require(game.phase() == sh::GamePhase::Complete, "Hitler election should end the game");
    require(game.winner().has_value(), "winner should be recorded");
    require(*game.winner() == "fascists", "fascists should win when Hitler is elected after three fascist policies");
}

void execution_paths_work_in_five_player_game() {
    auto game = make_started_game(5, 5004);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
    });

    game.test_set_policy_counts(0, 3);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Fascist, sh::Policy::Liberal},
        2,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "fourth fascist policy should trigger execution");
    require(game.pending_executive_power() == sh::ExecutivePower::Execution, "execution should be pending");
    require(game.execute_player("3"), "president should be able to execute a non-Hitler player");
    require(game.phase() == sh::GamePhase::Election, "non-Hitler execution should continue the game");
    require(!game.players()[2].alive, "executed player should be dead");

    auto second_game = make_started_game(5, 5005);
    set_roles(second_game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
    });
    second_game.test_set_policy_counts(0, 3);
    run_legislative_session(
        second_game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Fascist, sh::Policy::Liberal},
        2,
        0
    );

    require(second_game.execute_player("4"), "president should be able to execute Hitler");
    require(second_game.phase() == sh::GamePhase::Complete, "executing Hitler should end the game");
    require(second_game.winner().has_value(), "winner should be set after Hitler execution");
    require(*second_game.winner() == "liberals", "liberals should win when Hitler is executed");
}

void failed_government_top_deck_path_in_five_player_game() {
    auto game = make_started_game(5, 5006);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Liberal}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().election_tracker == 0, "tracker should reset after top-deck");
    require(game.public_state().liberal_policies == 1, "top-deck liberal should advance liberal board");
    require(game.phase() == sh::GamePhase::Election, "game should return to election after top-deck liberal");
}

void invalid_nomination_and_vote_paths_in_five_player_game() {
    auto game = make_started_game(5, 5007);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
    });

    require(!game.nominate_chancellor("1"), "president should not nominate themselves");
    require(!game.nominate_chancellor("9"), "unknown player should not be nominatable");
    require(game.nominate_chancellor("2"), "valid nomination should succeed");
    require(!game.nominate_chancellor("3"), "cannot replace chancellor nomination mid-election");
    require(game.cast_vote("3", sh::Vote::Ja), "alive player should be able to vote");
    require(game.cast_vote("3", sh::Vote::Nein), "player should be able to change vote before resolution");
    require(!game.cast_vote("9", sh::Vote::Ja), "unknown player should not be able to vote");
    require(!game.resolve_election(), "cannot resolve election until all votes are cast");
}

void dead_players_are_skipped_in_rotation_and_cannot_vote_in_five_player_game() {
    auto game = make_started_game(5, 5008);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
    });

    game.test_set_policy_counts(0, 3);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Fascist, sh::Policy::Liberal},
        2,
        0
    );
    require(game.execute_player("2"), "executing a non-Hitler player should succeed");
    require(!game.players()[1].alive, "player 2 should now be dead");
    require(game.president_id().has_value(), "next president should be assigned");
    require(*game.president_id() == "3", "presidency should skip the dead player and move to player 3");
    require(game.nominate_chancellor("4"), "next president should be able to nominate");
    require(!game.cast_vote("2", sh::Vote::Ja), "dead player should not be able to vote");
}

void top_deck_fascist_can_trigger_execution_in_five_player_game() {
    auto game = make_started_game(5, 5009);
    game.test_set_policy_counts(0, 3);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Fascist}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().election_tracker == 0, "tracker should reset after top-deck fascist");
    require(game.public_state().fascist_policies == 4, "top-deck fascist should advance fascist board");
    require(game.phase() == sh::GamePhase::ExecutiveAction, "top-deck fourth fascist should trigger execution");
    require(game.pending_executive_power() == sh::ExecutivePower::Execution, "execution should be pending");
}

void liberal_and_fascist_policy_wins_work_in_five_player_game() {
    auto liberal_game = make_started_game(5, 5010);
    liberal_game.test_set_policy_counts(4, 0);
    run_legislative_session(
        liberal_game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Fascist, sh::Policy::Fascist},
        1,
        0
    );
    require(liberal_game.phase() == sh::GamePhase::Complete, "fifth liberal policy should end the game");
    require(liberal_game.winner().has_value(), "winner should be set after liberal win");
    require(*liberal_game.winner() == "liberals", "liberals should win at five liberal policies");

    auto fascist_game = make_started_game(5, 5011);
    fascist_game.test_set_policy_counts(0, 5);
    run_legislative_session(
        fascist_game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Liberal, sh::Policy::Liberal},
        1,
        0
    );
    require(fascist_game.phase() == sh::GamePhase::Complete, "sixth fascist policy should end the game");
    require(fascist_game.winner().has_value(), "winner should be set after fascist win");
    require(*fascist_game.winner() == "fascists", "fascists should win at six fascist policies");
}

void player_view_shows_only_current_private_hand_in_five_player_game() {
    auto game = make_started_game(5, 5012);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Ja);
    require(game.resolve_election(), "election should resolve");

    const auto president_view = game.player_view("1");
    const auto chancellor_view_before = game.player_view("2");
    const auto spectator_view = game.player_view("3");
    require(president_view.has_value(), "president view should exist");
    require(chancellor_view_before.has_value(), "chancellor view should exist");
    require(spectator_view.has_value(), "spectator view should exist");
    require(president_view->legislative_hand.size() == 3U, "president should see three cards");
    require(chancellor_view_before->legislative_hand.empty(), "chancellor should not see cards before president discard");
    require(spectator_view->legislative_hand.empty(), "other players should not see legislative cards");

    require(game.president_discards_policy(0), "president should be able to discard");
    const auto president_view_after = game.player_view("1");
    const auto chancellor_view_after = game.player_view("2");
    require(president_view_after->legislative_hand.empty(), "president hand should clear after discard");
    require(chancellor_view_after->legislative_hand.size() == 2U, "chancellor should see two cards after president discard");
}

void election_requires_strict_majority_in_five_player_game() {
    auto game = make_started_game(5, 5013);
    require(game.nominate_chancellor("2"), "nomination should succeed");
    require(game.cast_vote("1", sh::Vote::Ja), "vote should be accepted");
    require(game.cast_vote("2", sh::Vote::Ja), "vote should be accepted");
    require(game.cast_vote("3", sh::Vote::Nein), "vote should be accepted");
    require(game.cast_vote("4", sh::Vote::Nein), "vote should be accepted");
    require(game.cast_vote("5", sh::Vote::Nein), "vote should be accepted");
    require(game.resolve_election(), "election should resolve");
    require(game.phase() == sh::GamePhase::Election, "2-3 vote should fail the election");
    require(game.public_state().election_tracker == 1, "failed election should increment tracker");
}

void invalid_executive_actions_are_rejected_in_five_player_game() {
    auto game = make_started_game(5, 5014);
    game.test_set_policy_counts(0, 3);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Fascist, sh::Policy::Liberal},
        2,
        0
    );

    require(game.phase() == sh::GamePhase::ExecutiveAction, "execution should be pending");
    require(!game.execute_player("1"), "president should not be able to execute themselves");
    require(!game.execute_player("9"), "unknown player should not be executable");
    require(!game.investigate_player("3"), "wrong executive action should be rejected");
    require(!game.call_special_election("3"), "wrong executive action should be rejected");
}

void player_view_hides_policy_peek_from_other_players_in_five_player_game() {
    auto game = make_started_game(5, 5015);
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
    require(president_view->policy_peek.size() == 3U, "president should keep policy peek info");
    require(chancellor_view->policy_peek.empty(), "chancellor should not see policy peek");
    require(spectator_view->policy_peek.empty(), "other players should not see policy peek");
}

void previous_chancellor_can_become_president_in_five_player_game() {
    auto game = make_started_game(5, 5016);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Liberal, sh::Policy::Fascist},
        2,
        0
    );

    require(game.president_id().has_value(), "next president should be assigned");
    require(*game.president_id() == "2", "previous chancellor should become next president in turn order");
    require(game.is_term_limited("2"), "previous chancellor should still be term-limited for chancellor");
    require(!game.nominate_chancellor("2"), "current president cannot nominate themselves");
}

void investigate_result_is_empty_without_investigation_in_five_player_game() {
    auto game = make_started_game(5, 5017);
    const auto president_view = game.player_view("1");
    const auto other_view = game.player_view("2");
    require(president_view.has_value(), "president view should exist");
    require(other_view.has_value(), "other player view should exist");
    require(!president_view->investigation_result.has_value(), "no investigation result should exist before any investigation");
    require(!other_view->investigation_result.has_value(), "other players should not have investigation results");
}

void actions_are_rejected_in_complete_phase_for_five_players() {
    auto game = make_started_game(5, 5018);
    game.test_set_policy_counts(4, 0);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Fascist, sh::Policy::Fascist},
        1,
        0
    );

    require(game.phase() == sh::GamePhase::Complete, "game should be complete after fifth liberal policy");
    require(!game.nominate_chancellor("3"), "cannot nominate after game is complete");
    require(!game.cast_vote("1", sh::Vote::Ja), "cannot vote after game is complete");
    require(!game.resolve_election(), "cannot resolve election after game is complete");
    require(!game.president_discards_policy(0), "cannot discard after game is complete");
    require(!game.chancellor_enacts_policy(0), "cannot enact after game is complete");
}

void top_deck_fascist_then_execute_hitler_wins_for_liberals_in_five_players() {
    auto game = make_started_game(5, 5019);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Liberal,
        sh::Role::Hitler,
    });
    game.test_set_policy_counts(0, 3);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Fascist}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.phase() == sh::GamePhase::ExecutiveAction, "top-deck fourth fascist should trigger execution");
    require(game.execute_player("5"), "executing Hitler after top-deck should succeed");
    require(game.phase() == sh::GamePhase::Complete, "executing Hitler should end the game");
    require(game.winner().has_value(), "winner should be recorded");
    require(*game.winner() == "liberals", "liberals should win by executing Hitler");
}

void failed_election_clears_previous_nomination_state_in_five_players() {
    auto game = make_started_game(5, 5020);

    require(game.nominate_chancellor("2"), "initial nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Nein);
    require(game.resolve_election(), "failed election should resolve");
    require(!game.chancellor_id().has_value(), "failed election should clear chancellor nomination");
    require(game.nominate_chancellor("3"), "next election should allow a fresh nomination");
}

void unknown_player_view_is_rejected_in_five_players() {
    auto game = make_started_game(5, 5021);
    require(!game.player_view("999").has_value(), "unknown player view should not exist");
}

void policy_peek_owner_changes_on_next_peek_in_five_players() {
    auto game = make_started_game(5, 5022);
    game.test_set_policy_counts(0, 2);
    run_legislative_session(
        game,
        "2",
        {
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
        },
        1,
        0
    );

    require(game.player_view("1")->policy_peek.size() == 3U, "first president should own first peek");

    game.test_set_policy_counts(0, 2);
    run_legislative_session(
        game,
        "3",
        {
            sh::Policy::Fascist,
            sh::Policy::Liberal,
            sh::Policy::Liberal,
            sh::Policy::Fascist,
            sh::Policy::Fascist,
            sh::Policy::Liberal,
        },
        1,
        0
    );

    require(game.player_view("2")->policy_peek.size() == 3U, "new president should own latest peek");
    require(game.player_view("1")->policy_peek.empty(), "old peek owner should not keep stale peek after next session starts");
}

void executive_actions_are_rejected_outside_executive_phase_in_five_players() {
    auto game = make_started_game(5, 5023);

    require(!game.execute_player("2"), "cannot execute outside executive phase");
    require(!game.investigate_player("2"), "cannot investigate outside executive phase");
    require(!game.call_special_election("2"), "cannot call special election outside executive phase");
}

void failed_election_clears_pending_votes_in_five_players() {
    auto game = make_started_game(5, 5024);

    require(game.nominate_chancellor("2"), "nomination should succeed");
    cast_votes_for_all_alive(game, sh::Vote::Nein);
    require(game.public_state().pending_votes == 5, "pending votes should be visible before resolution");
    require(game.resolve_election(), "failed election should resolve");
    require(game.public_state().pending_votes == 0, "pending votes should clear after failed election");
}

void top_deck_liberal_does_not_create_executive_action_in_five_players() {
    auto game = make_started_game(5, 5025);
    game.test_set_policy_counts(0, 3);
    game.test_set_draw_pile(stacked_draw({sh::Policy::Liberal}));

    fail_government(game, "2");
    fail_government(game, "3");
    fail_government(game, "4");

    require(game.public_state().liberal_policies == 1, "top-deck liberal should advance liberal board");
    require(game.phase() == sh::GamePhase::Election, "top-deck liberal should return directly to election");
    require(!game.pending_executive_power().has_value(), "top-deck liberal should not create executive action");
}

void previous_president_is_not_term_limited_in_five_players() {
    auto game = make_started_game(5, 5026);
    run_legislative_session(
        game,
        "2",
        {sh::Policy::Liberal, sh::Policy::Liberal, sh::Policy::Fascist},
        2,
        0
    );

    require(!game.is_term_limited("1"), "previous president should not be term-limited in five-player game");
    require(game.is_term_limited("2"), "previous chancellor should still be term-limited");
    require(game.nominate_chancellor("1"), "previous president should still be a valid nominee in five-player game");
}

void top_deck_third_fascist_triggers_policy_peek_in_five_players() {
    auto game = make_started_game(5, 5027);
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

    require(game.public_state().fascist_policies == 3, "top-deck fascist should advance to third fascist policy");
    require(game.phase() == sh::GamePhase::Election, "policy peek should resolve back to election");
    const auto peek_owner_view = game.player_view("3");
    require(peek_owner_view.has_value(), "peek owner view should exist");
    require(peek_owner_view->policy_peek.size() == 3U, "third failed government president should see peeked policies");
}

void executed_player_view_reflects_death_in_five_players() {
    auto game = make_started_game(5, 5028);
    game.test_set_policy_counts(0, 3);
    set_roles(game, {
        sh::Role::Liberal,
        sh::Role::Fascist,
        sh::Role::Liberal,
        sh::Role::Hitler,
        sh::Role::Liberal,
    });

    run_legislative_session(
        game,
        "2",
        {sh::Policy::Fascist, sh::Policy::Fascist, sh::Policy::Liberal},
        2,
        0
    );
    require(game.execute_player("3"), "execution should succeed");
    const auto executed_view = game.player_view("3");
    require(executed_view.has_value(), "executed player view should still exist");
    require(!executed_view->alive, "executed player view should show dead status");
    require(!game.public_state().players[2].alive, "public state should show executed player as dead");
}

}  // namespace

void run_players_5_tests() {
    setup_and_visibility_for_five_players();
    policy_peek_is_private_in_five_player_game();
    hitler_elected_after_three_fascists_wins_for_fascists();
    execution_paths_work_in_five_player_game();
    failed_government_top_deck_path_in_five_player_game();
    invalid_nomination_and_vote_paths_in_five_player_game();
    dead_players_are_skipped_in_rotation_and_cannot_vote_in_five_player_game();
    top_deck_fascist_can_trigger_execution_in_five_player_game();
    liberal_and_fascist_policy_wins_work_in_five_player_game();
    player_view_shows_only_current_private_hand_in_five_player_game();
    election_requires_strict_majority_in_five_player_game();
    invalid_executive_actions_are_rejected_in_five_player_game();
    player_view_hides_policy_peek_from_other_players_in_five_player_game();
    previous_chancellor_can_become_president_in_five_player_game();
    investigate_result_is_empty_without_investigation_in_five_player_game();
    actions_are_rejected_in_complete_phase_for_five_players();
    top_deck_fascist_then_execute_hitler_wins_for_liberals_in_five_players();
    failed_election_clears_previous_nomination_state_in_five_players();
    unknown_player_view_is_rejected_in_five_players();
    policy_peek_owner_changes_on_next_peek_in_five_players();
    executive_actions_are_rejected_outside_executive_phase_in_five_players();
    failed_election_clears_pending_votes_in_five_players();
    top_deck_liberal_does_not_create_executive_action_in_five_players();
    previous_president_is_not_term_limited_in_five_players();
    top_deck_third_fascist_triggers_policy_peek_in_five_players();
    executed_player_view_reflects_death_in_five_players();
}
