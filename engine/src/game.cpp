#include "sh/game.hpp"

#include <algorithm>
#include <iterator>

using namespace std;

namespace sh {

optional<GameConfig> config_for_player_count(size_t player_count) {
    switch (player_count) {
        case 5:
            return GameConfig{
                .player_count = 5,
                .liberal_count = 3,
                .non_hitler_fascist_count = 1,
                .hitler_knows_fascists = false,
                .fascist_track = {
                    ExecutivePower::None,
                    ExecutivePower::None,
                    ExecutivePower::PolicyPeek,
                    ExecutivePower::Execution,
                    ExecutivePower::Execution,
                    ExecutivePower::None,
                },
            };
        case 6:
            return GameConfig{
                .player_count = 6,
                .liberal_count = 4,
                .non_hitler_fascist_count = 1,
                .hitler_knows_fascists = false,
                .fascist_track = {
                    ExecutivePower::None,
                    ExecutivePower::None,
                    ExecutivePower::PolicyPeek,
                    ExecutivePower::Execution,
                    ExecutivePower::Execution,
                    ExecutivePower::None,
                },
            };
        case 7:
        case 8:
            return GameConfig{
                .player_count = static_cast<int>(player_count),
                .liberal_count = player_count == 7 ? 4 : 5,
                .non_hitler_fascist_count = 2,
                .hitler_knows_fascists = false,
                .fascist_track = {
                    ExecutivePower::None,
                    ExecutivePower::InvestigateLoyalty,
                    ExecutivePower::SpecialElection,
                    ExecutivePower::Execution,
                    ExecutivePower::Execution,
                    ExecutivePower::None,
                },
            };
        case 9:
        case 10:
            return GameConfig{
                .player_count = static_cast<int>(player_count),
                .liberal_count = player_count == 9 ? 5 : 6,
                .non_hitler_fascist_count = 3,
                .hitler_knows_fascists = false,
                .fascist_track = {
                    ExecutivePower::InvestigateLoyalty,
                    ExecutivePower::InvestigateLoyalty,
                    ExecutivePower::SpecialElection,
                    ExecutivePower::Execution,
                    ExecutivePower::Execution,
                    ExecutivePower::None,
                },
            };
        default:
            return nullopt;
    }
}

Game::Game(string room_code) : room_code_(move(room_code)) {}

void Game::assign_roles(const GameConfig& game_config) {
    vector<Role> roles;
    roles.reserve(players_.size());

    roles.push_back(Role::Hitler);
    for (int i = 0; i < game_config.non_hitler_fascist_count; ++i) {
        roles.push_back(Role::Fascist);
    }
    for (int i = 0; i < game_config.liberal_count; ++i) {
        roles.push_back(Role::Liberal);
    }

    shuffle(roles.begin(), roles.end(), rng_);

    for (size_t i = 0; i < players_.size(); ++i) {
        players_[i].identity = roles[i];
        players_[i].party = roles[i] == Role::Liberal ? Party::Liberal : Party::Fascist;
        players_[i].alive = true;
    }
}

void Game::initialize_policy_deck() {
    draw_pile_.clear();
    discard_pile_.clear();
    draw_pile_.reserve(17);

    for (int i = 0; i < 6; ++i) {
        draw_pile_.push_back(Policy::Liberal);
    }
    for (int i = 0; i < 11; ++i) {
        draw_pile_.push_back(Policy::Fascist);
    }

    shuffle(draw_pile_.begin(), draw_pile_.end(), rng_);
}

void Game::reset_round_state() {
    liberal_policies_ = 0;
    fascist_policies_ = 0;
    election_tracker_ = 0;
    president_index_ = 0;
    president_id_.reset();
    chancellor_id_.reset();
    last_president_id_.reset();
    last_chancellor_id_.reset();
    winner_.reset();
    pending_executive_power_.reset();
    investigation_result_.reset();
    investigation_result_owner_id_.reset();
    special_election_return_index_.reset();
    pending_special_election_index_.reset();
    policy_peek_.clear();
    policy_peek_owner_id_.reset();
    clear_pending_votes();
    clear_legislative_state();
}

void Game::reshuffle_discard_into_draw_pile() {
    if (discard_pile_.empty()) {
        return;
    }

    draw_pile_.insert(draw_pile_.end(), discard_pile_.begin(), discard_pile_.end());
    discard_pile_.clear();
    shuffle(draw_pile_.begin(), draw_pile_.end(), rng_);
}

void Game::clear_pending_votes() {
    pending_votes_.clear();
}

void Game::clear_legislative_state() {
    president_hand_.clear();
    chancellor_hand_.clear();
}

bool Game::has_player(const string& player_id) const {
    return player_index(player_id).has_value();
}

optional<size_t> Game::player_index(const string& player_id) const {
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].id == player_id) {
            return i;
        }
    }
    return nullopt;
}

int Game::alive_player_count() const {
    return static_cast<int>(count_if(
        players_.begin(),
        players_.end(),
        [](const Player& player) {
            return player.alive;
        }
    ));
}

int Game::count_vote(Vote vote) const {
    return static_cast<int>(count_if(
        pending_votes_.begin(),
        pending_votes_.end(),
        [vote](const pair<string, Vote>& entry) {
            return entry.second == vote;
        }
    ));
}

bool Game::draw_policies(vector<Policy>& destination, size_t count) {
    while (draw_pile_.size() < count) {
        if (discard_pile_.empty()) {
            break;
        }
        reshuffle_discard_into_draw_pile();
    }

    if (draw_pile_.size() < count) {
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        destination.push_back(draw_pile_.back());
        draw_pile_.pop_back();
    }
    return true;
}

bool Game::begin_legislative_session() {
    clear_legislative_state();
    policy_peek_.clear();
    policy_peek_owner_id_.reset();
    investigation_result_.reset();
    investigation_result_owner_id_.reset();
    if (!draw_policies(president_hand_, 3)) {
        return false;
    }
    phase_ = GamePhase::LegislativeSession;
    return true;
}

void Game::check_policy_win_conditions() {
    if (liberal_policies_ >= 5) {
        winner_ = "liberals";
        phase_ = GamePhase::Complete;
        pending_executive_power_.reset();
        return;
    }

    if (fascist_policies_ >= 6) {
        winner_ = "fascists";
        phase_ = GamePhase::Complete;
        pending_executive_power_.reset();
    }
}

void Game::finalize_round_after_policy() {
    chancellor_id_.reset();
    clear_pending_votes();
    clear_legislative_state();

    if (phase_ == GamePhase::Complete) {
        return;
    }

    if (pending_special_election_index_.has_value()) {
        president_index_ = *pending_special_election_index_;
        president_id_ = players_[president_index_].id;
        pending_special_election_index_.reset();
        phase_ = GamePhase::Election;
        return;
    }

    if (special_election_return_index_.has_value()) {
        president_index_ = *special_election_return_index_;
        special_election_return_index_.reset();
        phase_ = GamePhase::Election;
        advance_presidency();
        return;
    }

    phase_ = GamePhase::Election;
    advance_presidency();
}

bool Game::enact_policy(Policy policy, bool from_top_deck) {
    if (!from_top_deck && phase_ != GamePhase::LegislativeSession) {
        return false;
    }

    if (policy == Policy::Liberal) {
        ++liberal_policies_;
    } else {
        ++fascist_policies_;
    }

    check_policy_win_conditions();
    if (phase_ == GamePhase::Complete) {
        clear_legislative_state();
        return true;
    }

    if (policy == Policy::Fascist && config_.has_value()) {
        const ExecutivePower power = config_->fascist_track[static_cast<size_t>(fascist_policies_ - 1)];
        if (power == ExecutivePower::None) {
            pending_executive_power_.reset();
        } else if (power == ExecutivePower::PolicyPeek) {
            pending_executive_power_ = power;
            policy_peek_.clear();
            policy_peek_owner_id_ = president_id_;
            const size_t peek_count = min<size_t>(3, draw_pile_.size());
            for (size_t i = 0; i < peek_count; ++i) {
                policy_peek_.push_back(draw_pile_[draw_pile_.size() - 1 - i]);
            }
            pending_executive_power_.reset();
            finalize_round_after_policy();
            return true;
        } else {
            pending_executive_power_ = power;
        }
    } else {
        pending_executive_power_.reset();
    }

    if (!pending_executive_power_.has_value()) {
        finalize_round_after_policy();
    } else {
        clear_legislative_state();
        phase_ = GamePhase::ExecutiveAction;
    }

    return true;
}

bool Game::top_deck_policy() {
    vector<Policy> top_policy;
    if (!draw_policies(top_policy, 1)) {
        return false;
    }
    return enact_policy(top_policy.front(), true);
}

vector<string> Game::visible_fascists_for_player(size_t index) const {
    vector<string> visible_fascists;
    if (!config_.has_value() || index >= players_.size()) {
        return visible_fascists;
    }

    const Player& viewer = players_[index];
    if (viewer.identity == Role::Liberal) {
        return visible_fascists;
    }

    for (size_t i = 0; i < players_.size(); ++i) {
        if (i == index) {
            continue;
        }

        const Player& other = players_[i];
        if (viewer.identity == Role::Fascist) {
            if (other.identity == Role::Fascist || other.identity == Role::Hitler) {
                visible_fascists.push_back(other.id);
            }
            continue;
        }

        if (viewer.identity == Role::Hitler && config_->hitler_knows_fascists && other.identity == Role::Fascist) {
            visible_fascists.push_back(other.id);
        }
    }

    return visible_fascists;
}

bool Game::add_player(string player_id, string name) {
    if (phase_ != GamePhase::Lobby || players_.size() >= 10U) {
        return false;
    }

    if (find_if(
            players_.begin(),
            players_.end(),
            [&player_id](const Player& player) {
                return player.id == player_id;
            }
        ) != players_.end()) {
        return false;
    }

    players_.push_back(Player{
        .id = move(player_id),
        .name = move(name),
        .connected = true,
        .alive = true,
    });
    return true;
}

bool Game::set_player_connected(const string& player_id, bool connected) {
    const auto index = player_index(player_id);
    if (!index.has_value()) {
        return false;
    }

    players_[*index].connected = connected;
    return true;
}

bool Game::set_player_name(const string& player_id, const string& name) {
    const auto index = player_index(player_id);
    if (!index.has_value()) {
        return false;
    }

    players_[*index].name = name;
    return true;
}

bool Game::start() {
    if (phase_ != GamePhase::Lobby) {
        return false;
    }

    config_ = config_for_player_count(players_.size());
    if (!config_.has_value()) {
        return false;
    }

    reset_round_state();
    assign_roles(*config_);
    initialize_policy_deck();

    phase_ = GamePhase::Election;
    president_id_ = players_.front().id;
    return true;
}

PublicGameState Game::public_state() const {
    PublicGameState state;
    state.room_code = room_code_;
    state.phase = phase_;
    state.liberal_policies = liberal_policies_;
    state.fascist_policies = fascist_policies_;
    state.election_tracker = election_tracker_;
    state.president_id = president_id_;
    state.chancellor_id = chancellor_id_;
    state.last_president_id = last_president_id_;
    state.last_chancellor_id = last_chancellor_id_;
    state.winner = winner_;
    state.draw_pile_size = static_cast<int>(draw_pile_.size());
    state.discard_pile_size = static_cast<int>(discard_pile_.size());
    state.pending_votes = static_cast<int>(pending_votes_.size());
    state.president_hand_size = static_cast<int>(president_hand_.size());
    state.chancellor_hand_size = static_cast<int>(chancellor_hand_.size());
    state.pending_executive_power = pending_executive_power_;

    state.players.reserve(players_.size());
    for (const auto& player : players_) {
        state.players.push_back(PublicPlayerState{
            .id = player.id,
            .name = player.name,
            .connected = player.connected,
            .alive = player.alive,
        });
    }

    return state;
}

GamePhase Game::phase() const {
    return phase_;
}

size_t Game::player_count() const {
    return players_.size();
}

const optional<GameConfig>& Game::config() const {
    return config_;
}

const vector<Player>& Game::players() const {
    return players_;
}

const vector<Policy>& Game::draw_pile() const {
    return draw_pile_;
}

const vector<Policy>& Game::discard_pile() const {
    return discard_pile_;
}

const vector<Policy>& Game::president_hand() const {
    return president_hand_;
}

const vector<Policy>& Game::chancellor_hand() const {
    return chancellor_hand_;
}

const optional<string>& Game::president_id() const {
    return president_id_;
}

const optional<string>& Game::chancellor_id() const {
    return chancellor_id_;
}

const optional<string>& Game::last_president_id() const {
    return last_president_id_;
}

const optional<string>& Game::last_chancellor_id() const {
    return last_chancellor_id_;
}

const optional<string>& Game::winner() const {
    return winner_;
}

optional<ExecutivePower> Game::current_executive_power() const {
    if (!config_.has_value() || fascist_policies_ <= 0 || fascist_policies_ > 6) {
        return nullopt;
    }

    return config_->fascist_track[static_cast<size_t>(fascist_policies_ - 1)];
}

optional<ExecutivePower> Game::pending_executive_power() const {
    return pending_executive_power_;
}

bool Game::is_term_limited(const string& player_id) const {
    if (!config_.has_value()) {
        return false;
    }

    if (last_chancellor_id_.has_value() && *last_chancellor_id_ == player_id) {
        return true;
    }

    return config_->player_count > 5
        && last_president_id_.has_value()
        && *last_president_id_ == player_id;
}

bool Game::can_nominate_chancellor(const string& player_id) const {
    if (phase_ != GamePhase::Election || !president_id_.has_value() || pending_executive_power_.has_value()) {
        return false;
    }

    if (chancellor_id_.has_value()) {
        return false;
    }

    const auto index = player_index(player_id);
    if (!index.has_value()) {
        return false;
    }

    if (!players_[*index].alive || player_id == *president_id_) {
        return false;
    }

    return !is_term_limited(player_id);
}

bool Game::all_votes_cast() const {
    return !players_.empty() && static_cast<int>(pending_votes_.size()) == alive_player_count();
}

optional<Party> Game::investigation_result() const {
    return investigation_result_;
}

optional<PlayerView> Game::player_view(const string& player_id) const {
    const auto index = player_index(player_id);
    if (!index.has_value()) {
        return nullopt;
    }

    const Player& player = players_[*index];
    PlayerView view;
    view.player_id = player.id;
    view.player_name = player.name;
    view.role = player.identity;
    view.party = player.party;
    view.alive = player.alive;
    view.known_fascists = visible_fascists_for_player(*index);

    if (president_id_.has_value() && *president_id_ == player_id) {
        view.legislative_hand = president_hand_;
    } else if (chancellor_id_.has_value() && *chancellor_id_ == player_id) {
        view.legislative_hand = chancellor_hand_;
    }

    if (policy_peek_owner_id_.has_value() && *policy_peek_owner_id_ == player_id) {
        view.policy_peek = policy_peek_;
    }

    if (investigation_result_owner_id_.has_value() && *investigation_result_owner_id_ == player_id) {
        view.investigation_result = investigation_result_;
    }

    return view;
}

bool Game::advance_presidency() {
    if (players_.empty()) {
        return false;
    }

    const size_t start_index = president_index_;
    do {
        president_index_ = (president_index_ + 1) % players_.size();
        if (players_[president_index_].alive) {
            president_id_ = players_[president_index_].id;
            return true;
        }
    } while (president_index_ != start_index);

    return false;
}

bool Game::nominate_chancellor(const string& player_id) {
    if (!can_nominate_chancellor(player_id)) {
        return false;
    }

    chancellor_id_ = player_id;
    clear_pending_votes();
    return true;
}

bool Game::cast_vote(const string& player_id, Vote vote) {
    if (phase_ != GamePhase::Election || !chancellor_id_.has_value()) {
        return false;
    }

    const auto index = player_index(player_id);
    if (!index.has_value() || !players_[*index].alive) {
        return false;
    }

    const auto existing_vote = find_if(
        pending_votes_.begin(),
        pending_votes_.end(),
        [&player_id](const pair<string, Vote>& entry) {
            return entry.first == player_id;
        }
    );

    if (existing_vote != pending_votes_.end()) {
        existing_vote->second = vote;
        return true;
    }

    pending_votes_.push_back({player_id, vote});
    return true;
}

bool Game::resolve_election() {
    if (phase_ != GamePhase::Election || !chancellor_id_.has_value() || !all_votes_cast()) {
        return false;
    }

    const bool passes = count_vote(Vote::Ja) > alive_player_count() / 2;
    clear_pending_votes();

    if (passes) {
        election_tracker_ = 0;
        last_president_id_ = president_id_;
        last_chancellor_id_ = chancellor_id_;

        const auto nominee_index = player_index(*chancellor_id_);
        if (nominee_index.has_value()
            && players_[*nominee_index].identity == Role::Hitler
            && fascist_policies_ >= 3) {
            winner_ = "fascists";
            phase_ = GamePhase::Complete;
            return true;
        }

        return begin_legislative_session();
    }

    chancellor_id_.reset();
    ++election_tracker_;
    if (election_tracker_ >= 3) {
        election_tracker_ = 0;
        if (!top_deck_policy()) {
            return false;
        }
        return true;
    }

    if (phase_ != GamePhase::Election) {
        return true;
    }

    return advance_presidency();
}

bool Game::president_discards_policy(size_t index) {
    if (phase_ != GamePhase::LegislativeSession || president_hand_.size() != 3U || !chancellor_hand_.empty()) {
        return false;
    }

    if (index >= president_hand_.size()) {
        return false;
    }

    discard_pile_.push_back(president_hand_[index]);
    president_hand_.erase(president_hand_.begin() + static_cast<ptrdiff_t>(index));
    chancellor_hand_ = president_hand_;
    president_hand_.clear();
    return true;
}

bool Game::chancellor_enacts_policy(size_t index) {
    if (phase_ != GamePhase::LegislativeSession || chancellor_hand_.size() != 2U) {
        return false;
    }

    if (index >= chancellor_hand_.size()) {
        return false;
    }

    const Policy enacted_policy = chancellor_hand_[index];
    const size_t discard_index = index == 0 ? 1U : 0U;
    discard_pile_.push_back(chancellor_hand_[discard_index]);
    chancellor_hand_.clear();
    return enact_policy(enacted_policy, false);
}

bool Game::execute_player(const string& player_id) {
    if (phase_ != GamePhase::ExecutiveAction || pending_executive_power_ != ExecutivePower::Execution) {
        return false;
    }

    const auto index = player_index(player_id);
    if (!index.has_value() || !players_[*index].alive || player_id == *president_id_) {
        return false;
    }

    players_[*index].alive = false;
    if (players_[*index].identity == Role::Hitler) {
        winner_ = "liberals";
        phase_ = GamePhase::Complete;
        pending_executive_power_.reset();
        clear_legislative_state();
        return true;
    }

    pending_executive_power_.reset();
    finalize_round_after_policy();
    return true;
}

bool Game::investigate_player(const string& player_id) {
    if (phase_ != GamePhase::ExecutiveAction || pending_executive_power_ != ExecutivePower::InvestigateLoyalty) {
        return false;
    }

    const auto index = player_index(player_id);
    if (!index.has_value() || !players_[*index].alive || player_id == *president_id_) {
        return false;
    }

    investigation_result_ = players_[*index].party;
    investigation_result_owner_id_ = president_id_;
    pending_executive_power_.reset();
    finalize_round_after_policy();
    return true;
}

bool Game::call_special_election(const string& player_id) {
    if (phase_ != GamePhase::ExecutiveAction || pending_executive_power_ != ExecutivePower::SpecialElection) {
        return false;
    }

    const auto index = player_index(player_id);
    if (!index.has_value() || !players_[*index].alive || player_id == *president_id_) {
        return false;
    }

    special_election_return_index_ = president_index_;
    pending_special_election_index_ = *index;
    pending_executive_power_.reset();
    finalize_round_after_policy();
    return true;
}

#ifdef SH_ENABLE_TEST_HOOKS
void Game::test_seed_rng(uint32_t seed) {
    rng_.seed(seed);
}

void Game::test_set_roles(const vector<Role>& roles) {
    if (roles.size() != players_.size()) {
        return;
    }

    for (size_t i = 0; i < roles.size(); ++i) {
        players_[i].identity = roles[i];
        players_[i].party = roles[i] == Role::Liberal ? Party::Liberal : Party::Fascist;
    }
}

void Game::test_set_draw_pile(const vector<Policy>& draw_pile) {
    draw_pile_ = draw_pile;
}

void Game::test_set_discard_pile(const vector<Policy>& discard_pile) {
    discard_pile_ = discard_pile;
}

void Game::test_set_policy_counts(int liberal_policies, int fascist_policies) {
    liberal_policies_ = liberal_policies;
    fascist_policies_ = fascist_policies;
}

void Game::test_set_previous_officeholders(optional<string> president_id, optional<string> chancellor_id) {
    last_president_id_ = move(president_id);
    last_chancellor_id_ = move(chancellor_id);
}

void Game::test_set_president(const string& player_id) {
    const auto index = player_index(player_id);
    if (!index.has_value()) {
        return;
    }

    president_index_ = *index;
    president_id_ = player_id;
}

void Game::test_set_phase(GamePhase phase) {
    phase_ = phase;
}

void Game::test_set_pending_executive_power(optional<ExecutivePower> power) {
    pending_executive_power_ = power;
}
#endif

}  // namespace sh
