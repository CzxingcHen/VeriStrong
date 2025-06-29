#ifndef CHECKER_HISTORY_HISTORY_H
#define CHECKER_HISTORY_HISTORY_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <ranges>
#include <vector>
#include <unordered_map>

namespace checker::history {

enum class EventType { READ, WRITE };

struct Event {
  int64_t key;
  int64_t value;
  EventType type;
  int64_t transaction_id;
};

struct Transaction {
  int64_t id;
  std::vector<Event> events;
  int64_t session_id;
};

struct Session {
  int64_t id;
  std::vector<Transaction> transactions;
};

struct History {
  std::vector<Session> sessions;

  friend auto operator<<(std::ostream &os, const History &history)
      -> std::ostream &;

  std::ranges::range auto transactions() const {
    return sessions  //
           | std::ranges::views::transform([](const auto &s) {
               return std::ranges::views::all(s.transactions);
             })  //
           | std::ranges::views::join;
  }

  std::ranges::range auto events() const {
    return transactions()  //
           | std::ranges::views::transform([](const auto &txn) {
               return std::ranges::views::all(txn.events);
             })  //
           | std::ranges::views::join;
  }
};

struct HistoryMetaInfo {
  int n_sessions, n_total_transactions, n_total_events;
  std::unordered_map<int64_t, std::unordered_map<int64_t, int>> write_steps, read_steps; // write_steps read_steps [txn][key] = step
  std::unordered_map<int64_t, int> txn_distance;
  std::unordered_map<int64_t, int64_t> session_id_of;
};

auto compute_history_meta_info(const History &history) -> HistoryMetaInfo;

/**
 * Read history from an input stream. The history is in dbcop format.
 *
 * Dbcop format:
 *
 * DBCOP_FILE := ID SESSION_NUM KEY_NUM TXN_NUM EVENT_NUM INFO START_TIME END_TIME
 * HISTORY := SIZE SESSION_1 ... SESSION_{SIZE}
 * SESSION := SIZE TRANSACTION_1 ... TRANSACTION_{SIZE}
 * TRANSACTION := SIZE EVENT_1 ... EVENT_{SIZE}
 * EVENT := IS_WRITE KEY VALUE SUCCESS
 * ID, SESSION_NUM, KEY_NUM, TXN_NUM, EVENT_NUM, SIZE, KEY, VALUE := int64_t
 * INFO, START, END := null terminated string
 * IS_WRITE, SUCCESS := bool
 */
auto parse_dbcop_history(std::istream &is) -> History;
auto parse_cobra_history(const std::string &history_dir, bool unique_value) -> History;
auto parse_elle_list_append_history(std::ifstream &is) -> std::pair<History, std::vector<std::tuple<int64_t, int64_t, int64_t>>>;

auto analyze_repeat_values(const History &history) -> void;

auto n_txns_of(History &history) -> int;
auto n_rw_same_key_txns_of(History &history) -> int;
auto n_written_key_txns_of(History &history) -> std::unordered_map<int64_t, int>;
}  // namespace checker::history

namespace std {
template <>
struct hash<checker::history::Session> {
  auto operator()(const checker::history::Session &session) const {
    return hash<int64_t>{}(session.id);
  }
};

template <>
struct hash<checker::history::Transaction> {
  auto operator()(const checker::history::Transaction &txn) const {
    return hash<int64_t>{}(txn.id);
  }
};
}  // namespace std

#endif /* CHECKER_HISTORY_HISTORY_H */
