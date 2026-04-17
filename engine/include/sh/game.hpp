#pragma once

#include <array>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace sh {

enum class GamePhase {
    Lobby,
    Election,
    LegislativeSession,
    ExecutiveAction,
    Complete
};

enum class Role {
    Liberal,
    Fascist,
    Hitler
};

enum class Policy {
    Liberal,
    Fascist
};

enum class Party {
    Liberal,
    Fascist
};

enum class ExecutivePower {
    None,
    InvestigateLoyalty,
    SpecialElection,
    PolicyPeek,
    Execution
};

enum class Vote {
    Ja,
    Nein
};

struct GameConfig {
    int player_count{0};
    int liberal_count{0};
    int non_hitler_fascist_count{0};
    bool hitler_knows_fascists{false};
    array<ExecutivePower, 6> fascist_track{
        ExecutivePower::None,
        ExecutivePower::None,
        ExecutivePower::None,
        ExecutivePower::None,
        ExecutivePower::None,
        ExecutivePower::None,
    };
};

struct Player {
    string id;
    string name;
    bool connected{true};
    bool alive{true};
    Party party{Party::Liberal};
    Role identity{Role::Liberal};
};

struct PublicPlayerState {
    string id;
    string name;
    bool connected{true};
    bool alive{true};
};

struct PublicGameState {
    string room_code;
    GamePhase phase{GamePhase::Lobby};
    vector<PublicPlayerState> players;
    int liberal_policies{0};
    int fascist_policies{0};
    int election_tracker{0};
    optional<string> president_id;
    optional<string> chancellor_id;
    optional<string> last_president_id;
    optional<string> last_chancellor_id;
    optional<string> winner;
    int draw_pile_size{0};
    int discard_pile_size{0};
    int pending_votes{0};
    int president_hand_size{0};
    int chancellor_hand_size{0};
    optional<ExecutivePower> pending_executive_power;
    vector<Policy> policy_peek;
};

struct PlayerView {
    string player_id;
    string player_name;
    Role role{Role::Liberal};
    Party party{Party::Liberal};
    bool alive{true};
    size_t num_players{0};
    optional<string> known_hitler;
    vector<string> known_fascists;
    vector<Policy> legislative_hand;
    vector<Policy> policy_peek;
    optional<Party> investigation_result;
};

[[nodiscard]] optional<GameConfig> config_for_player_count(size_t player_count);

class Game {
public:
    explicit Game(string room_code);

    bool add_player(string player_id, string name);
    bool set_player_name(const string& player_id, const string& name);
    bool set_player_connected(const string& player_id, bool connected);
    bool start();

    [[nodiscard]] PublicGameState public_state() const;
    [[nodiscard]] GamePhase phase() const;
    [[nodiscard]] size_t player_count() const;
    [[nodiscard]] const optional<GameConfig>& config() const;
    [[nodiscard]] const vector<Player>& players() const;
    [[nodiscard]] const vector<Policy>& draw_pile() const;
    [[nodiscard]] const vector<Policy>& discard_pile() const;
    [[nodiscard]] const vector<Policy>& president_hand() const;
    [[nodiscard]] const vector<Policy>& chancellor_hand() const;
    [[nodiscard]] const optional<string>& president_id() const;
    [[nodiscard]] const optional<string>& chancellor_id() const;
    [[nodiscard]] const optional<string>& last_president_id() const;
    [[nodiscard]] const optional<string>& last_chancellor_id() const;
    [[nodiscard]] const optional<string>& winner() const;
    [[nodiscard]] optional<ExecutivePower> current_executive_power() const;
    [[nodiscard]] optional<ExecutivePower> pending_executive_power() const;
    [[nodiscard]] bool is_term_limited(const string& player_id) const;
    [[nodiscard]] bool can_nominate_chancellor(const string& player_id) const;
    [[nodiscard]] bool all_votes_cast() const;
    [[nodiscard]] optional<Party> investigation_result() const;
    [[nodiscard]] optional<PlayerView> player_view(const string& player_id) const;
    bool advance_presidency();
    bool nominate_chancellor(const string& player_id);
    bool cast_vote(const string& player_id, Vote vote);
    bool resolve_election();
    bool president_discards_policy(size_t index);
    bool chancellor_enacts_policy(size_t index);
    bool execute_player(const string& player_id);
    bool investigate_player(const string& player_id);
    bool call_special_election(const string& player_id);

#ifdef SH_ENABLE_TEST_HOOKS
    void test_seed_rng(uint32_t seed);
    void test_set_roles(const vector<Role>& roles);
    void test_set_draw_pile(const vector<Policy>& draw_pile);
    void test_set_discard_pile(const vector<Policy>& discard_pile);
    void test_set_policy_counts(int liberal_policies, int fascist_policies);
    void test_set_previous_officeholders(optional<string> president_id, optional<string> chancellor_id);
    void test_set_president(const string& player_id);
    void test_set_phase(GamePhase phase);
    void test_set_pending_executive_power(optional<ExecutivePower> power);
#endif

private:
    void assign_roles(const GameConfig& game_config);
    void initialize_policy_deck();
    void reset_round_state();
    void reshuffle_discard_into_draw_pile();
    void clear_pending_votes();
    void clear_legislative_state();
    [[nodiscard]] bool has_player(const string& player_id) const;
    [[nodiscard]] optional<size_t> player_index(const string& player_id) const;
    [[nodiscard]] int alive_player_count() const;
    [[nodiscard]] int count_vote(Vote vote) const;
    [[nodiscard]] bool draw_policies(vector<Policy>& destination, size_t count);
    [[nodiscard]] bool begin_legislative_session();
    [[nodiscard]] bool enact_policy(Policy policy, bool from_top_deck);
    [[nodiscard]] bool top_deck_policy();
    [[nodiscard]] vector<string> visible_fascists_for_player(size_t index) const;
    void finalize_round_after_policy();
    void check_policy_win_conditions();

    string room_code_;
    GamePhase phase_{GamePhase::Lobby};
    vector<Player> players_;
    vector<Policy> draw_pile_;
    vector<Policy> discard_pile_;
    vector<Policy> president_hand_;
    vector<Policy> chancellor_hand_;
    vector<Policy> policy_peek_;
    optional<string> policy_peek_owner_id_;
    optional<string> investigation_result_owner_id_;
    int liberal_policies_{0};
    int fascist_policies_{0};
    int election_tracker_{0};
    size_t president_index_{0};
    optional<string> president_id_;
    optional<string> chancellor_id_;
    optional<string> last_president_id_;
    optional<string> last_chancellor_id_;
    optional<string> winner_;
    optional<GameConfig> config_;
    optional<ExecutivePower> pending_executive_power_;
    optional<Party> investigation_result_;
    vector<pair<string, Vote>> pending_votes_;
    optional<size_t> special_election_return_index_;
    optional<size_t> pending_special_election_index_;
    mt19937 rng_{random_device{}()};
};

}  // namespace sh
