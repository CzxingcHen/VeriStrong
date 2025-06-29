#include "history.h"

#include <bits/ranges_algo.h>

#include <boost/endian.hpp>
#include <boost/log/trivial.hpp>
#include <cstdint>
#include <istream>
#include <iterator>
#include <numeric>
#include <ostream>
#include <ranges>
#include <string>
#include <syncstream>
#include <unordered_set>
#include <utility>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>

#include "utils/to_container.h"

namespace fs = std::filesystem;

using std::ranges::count_if;
using std::ranges::views::iota;
using std::ranges::views::transform;

static auto read_int64(std::istream &in) -> int64_t {
  int64_t n;
  in.read(reinterpret_cast<char *>(&n), sizeof(n));
  boost::endian::little_to_native_inplace(n);
  return n;
}

static auto read_int64_big_endian(std::istream &in) -> int64_t {
  int64_t n = read_int64(in);
  boost::endian::native_to_big_inplace(n);
  return n;
};

static auto read_str(std::istream &in) -> std::string {
  auto size = read_int64(in);
  auto s = std::string(size, '\0');
  in.read(s.data(), size);
  return s;
}

static auto read_char(std::istream &in) -> char {
  // TODO: little-big-endian problem
  return (char)in.get();
}

static auto read_bool(std::istream &in) -> bool { return in.get() != 0; }

static constexpr auto hash_cobra_value = [](const auto &t) {
  auto &[t1, t2, t3] = t;
  std::hash<int64_t> h;
  return h(t1) ^ h(t2) ^ h(t3);
};

namespace checker::history {

auto parse_dbcop_history(std::istream &is) -> History {
  constexpr int64_t init_session_id = 0;
  constexpr int64_t init_txn_id = 0;

  auto history = History{};
  int64_t current_session_id = 1;
  int64_t current_txn_id = 1;
  auto keys = std::unordered_set<int64_t>{};

  auto parse_event = [&](Transaction &txn) {
    auto is_write = read_bool(is);
    auto key = read_int64(is);
    auto value = read_int64(is);
    auto success = read_bool(is);

    if (success) {
      keys.insert(key);
      txn.events.emplace_back(Event{
          .key = key,
          .value = value,
          .type = is_write ? EventType::WRITE : EventType::READ,
          .transaction_id = txn.id,
      });
    }
  };

  auto parse_txn = [&](Session &session) {
    auto &txn = session.transactions.emplace_back(Transaction{
        .id = current_txn_id++,
        .session_id = session.id,
    });

    auto size = read_int64(is);
    for ([[maybe_unused]] auto i : iota(0, size)) {
      parse_event(txn);
    }

    auto success = read_bool(is);
    if (!success) {
      session.transactions.pop_back();
    }
  };

  auto parse_session = [&] {
    auto &session =
        history.sessions.emplace_back(Session{.id = current_session_id++});

    auto size = read_int64(is);
    for ([[maybe_unused]] auto i : iota(0, size)) {
      parse_txn(session);
    }
  };

  [[maybe_unused]] auto id = read_int64(is);
  [[maybe_unused]] auto session_num = read_int64(is);
  [[maybe_unused]] auto key_num = read_int64(is);
  [[maybe_unused]] auto txn_num = read_int64(is);
  [[maybe_unused]] auto event_num = read_int64(is);
  [[maybe_unused]] auto info = read_str(is);
  [[maybe_unused]] auto start = read_str(is);
  [[maybe_unused]] auto end = read_str(is);

  auto size = read_int64(is);
  for ([[maybe_unused]] auto i : iota(0, size)) {
    parse_session();
  }

  history.sessions.emplace_back(Session{
      .id = init_session_id,
      .transactions = std::vector{Transaction{
          .id = init_txn_id,
          .events = keys  //
                    | transform([](auto key) {
                        return Event{
                            .key = key,
                            .value = 0,
                            .type = EventType::WRITE,
                            .transaction_id = init_txn_id,
                        };
                      })  //
                    | utils::to<std::vector<Event>>,
          .session_id = init_session_id,
      }},
  });

  // log history meta
  auto count_all = [](auto &&) { return true; };
  BOOST_LOG_TRIVIAL(info) << "#sessions: " << history.sessions.size();
  BOOST_LOG_TRIVIAL(info) << "#transactions: "
                          << count_if(history.transactions(), count_all);
  BOOST_LOG_TRIVIAL(info) << "#events: "
                          << count_if(history.events(), count_all);

  return history;
}

auto parse_cobra_history(const std::string &history_dir, bool unique_value = false) -> History {
  fs::path history_dir_path{history_dir};
  if (!fs::exists(history_dir_path)) {
    std::ostringstream os;
    os << "Incorrect path '" << history_dir_path << "'";
    throw std::runtime_error{os.str()};
  }
  fs::directory_entry history_dir_entry{history_dir_path};

  static constexpr int64_t INIT_WRITE_ID = 0xbebeebeeL;
  static constexpr int64_t INIT_TXN_ID = 0xbebeebeeL;
  static constexpr int64_t NULL_TXN_ID = 0xdeadbeefL;  
  static constexpr int64_t GC_WID_TRUE = 0x23332333L;
  static constexpr int64_t GC_WID_FALSE = 0x66666666L;
  static constexpr int64_t UNIVERSAL_TXN_ID = 0xabddefeeL;
  static constexpr int64_t UNIVERSAL_WRITE_ID = 0xabddefeeL;
  
  auto history = History{};
  auto init_writes = std::map<int64_t, int64_t>{};
  int64_t session_id = 0;

  auto sessions = std::map<int64_t, Session *>{};
  auto transactions = std::map<int64_t, Transaction *>{};
  auto writes = std::set<std::pair<int64_t, int64_t>>{}; // <key, value>

  enum class TransactionStatus { ONGOING, COMMIT };
  auto transactions_status = std::map<Transaction *, TransactionStatus>{};

  struct CobraValue {
    int64_t write_id;
    int64_t transaction_id;
    int64_t value;

    bool operator == (const CobraValue &rhs) const { 
      return write_id == rhs.write_id 
          && transaction_id == rhs.transaction_id 
          && value == rhs.value; 
    }
  };

  std::unordered_map<CobraValue, int64_t, decltype(hash_cobra_value)> cobra_value_hash;
  int64_t cur_hash = 1;
  auto get_cobra_hash = [&](const CobraValue &cv) -> int64_t {
    if (cobra_value_hash.contains(cv)) return cobra_value_hash[cv];
    return cobra_value_hash[cv] = cur_hash++;
  };

  auto add_session = [&](int64_t id = 0) -> Session * {
    if (!id) id = ++session_id;
    if (sessions.contains(id)) throw std::runtime_error{"Invalid history: fail in add_session()!"};
    Session *session = new Session{};
    session->id = id;
    sessions[id] = session;
    return session;
  };
  auto add_transaction = [&](Session *session, int64_t id) -> Transaction * {
    if (!sessions.contains(session->id) || transactions.contains(id)) {
      throw std::runtime_error{"Invalid history: fail in add_transaction()!"};
    }

    Transaction *txn = new Transaction{};
    txn->id = id, txn->session_id = session->id;
    transactions[id] = txn;
    transactions_status[txn] = TransactionStatus::ONGOING;
    return txn;
  };
  auto add_event = [&](Transaction *transaction, EventType type, int64_t key, int64_t value) {
    if (type == EventType::WRITE) {
      if (!transactions.contains(transaction->id)) {
        throw std::runtime_error{"Invalid history: fail in add_event()!"};
      }
      writes.insert({key, value});
    }
    transaction->events.push_back(Event{key, value, type, transaction->id});
  };

  auto parse_log = [&](std::istream &in, Session *session) {
    Transaction *current = nullptr;
    while (1) {
      char op = read_char(in);
      if (op == 255 || op == EOF) { // the file ends with 255(EOF), but not reach the real end of this file
        // It's quite strange, for 255 = EOF is our common sense, however exception occured on 2 machines
        break;
      }
      // std::cout << op << std::endl;
      switch (op) {
        case 'S': { 
          // txn start 
          // assert(current == nullptr);
          auto id = read_int64_big_endian(in);
          if (!transactions.contains(id)) {
            current = add_transaction(session, id);
          } else {
            current = transactions[id];
            // previous txn reads this txn
            if (transactions_status[current] == TransactionStatus::ONGOING || current->events.size() != 0) {
              throw std::runtime_error{"Invalid history: fail in txn start()!"};
            }
          }
          break;
        }
        case 'C': {
          // txn commit
          auto id = read_int64_big_endian(in);
          assert(current != nullptr && current->id == id);
          transactions_status[current] = TransactionStatus::COMMIT;
          Session *session = sessions[current->session_id];
          session->transactions.push_back(std::move(*current));
          delete current;
          break;
        }
        case 'W': {
          // write
          assert(current != nullptr);
          auto write_id = read_int64_big_endian(in);
          auto key = read_int64_big_endian(in);
          auto value = read_int64_big_endian(in);
          if (unique_value) {
            add_event(current, EventType::WRITE, key, get_cobra_hash({write_id, current->id, value}));
          } else {
            add_event(current, EventType::WRITE, key, get_cobra_hash({write_id, UNIVERSAL_TXN_ID, value}));
          }
          // std::cerr << 'W' << " " << write_id << " " << UNIVERSAL_TXN_ID << " " << value << " " << get_cobra_hash({write_id, UNIVERSAL_TXN_ID, value}) << "\n";
          break;
        }
        case 'R': {
          // read
          assert(current != nullptr);
          auto write_txn_id = read_int64_big_endian(in);
          auto write_id = read_int64_big_endian(in);
          auto key = read_int64_big_endian(in);
          auto value = read_int64_big_endian(in);
          
          if (write_txn_id == INIT_TXN_ID || write_txn_id == NULL_TXN_ID) {
            if (write_id == INIT_WRITE_ID || write_id == NULL_TXN_ID) {
              if (unique_value) write_id = key; // why = key? copied from PolySI
              write_txn_id = INIT_TXN_ID;
              if (!init_writes.contains(key)) {
                init_writes[key] = get_cobra_hash({write_id, INIT_TXN_ID, value});
                // std::cout << "R (" << key << ", " << value << ")" << std::endl;
              }
              // TODO: init_writes.computeIfAbsent(key, k -> new CobraValue(key, INIT_TXN_ID, value))
            } 
            // else if ((write_id != GC_WID_FALSE && write_id != GC_WID_TRUE) || write_txn_id != INIT_TXN_ID) {
            //   throw std::runtime_error{"Invalid history: fail in read()!"};
            // }
          }
          
          // std::cerr << 'R' << " " << write_id << " " << write_txn_id << " " << value << " " << get_cobra_hash({write_id, write_txn_id, value}) << "\n";
          add_event(current, EventType::READ, key, get_cobra_hash({write_id, write_txn_id, value}));
          break;
        }
        default: 
          throw std::runtime_error{"Invalid history: no opt type is matched!"};
      }
    }
  };

  for (const auto &entry : fs::directory_iterator(history_dir_entry)) {
    if (entry.path().extension() != ".log") {
      BOOST_LOG_TRIVIAL(trace) << "skip file " << entry.path().filename();
      continue;
    }
    std::string log_file_s = entry.path().string();
    auto log_file = std::ifstream{log_file_s};
    if (!log_file.is_open()) {
      std::ostringstream os;
      os << "Cannot open file '" << entry << "'";
      throw std::runtime_error{os.str()};
    }
    BOOST_LOG_TRIVIAL(trace) << "parse file " << entry.path().filename();
    try {
      auto session = add_session();
      parse_log(log_file, session);
      history.sessions.push_back(std::move(*session)); // TODO: std::move ?
      delete session;
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << std::endl;
      assert(0);
    }
  }

  auto init_session = add_session(INIT_TXN_ID);
  Transaction *init_txn = add_transaction(init_session, INIT_TXN_ID);
  for (const auto &[key, value] : init_writes) {
    add_event(init_txn, EventType::WRITE, key, value);
  }
  transactions_status[init_txn] = TransactionStatus::COMMIT;
  init_session->transactions.push_back(std::move(*init_txn));
  history.sessions.push_back(std::move(*init_session));
  delete init_session;
  delete init_txn;

  for (const auto &[txn_ref, status] : transactions_status) {
    if (status == TransactionStatus::ONGOING) {
      throw std::runtime_error{"Invalid history: uncommited transaction!"};
    }
  }

  // log history meta
  auto count_all = [](auto &&) { return true; };
  BOOST_LOG_TRIVIAL(info) << "#sessions: " << history.sessions.size();
  BOOST_LOG_TRIVIAL(info) << "#transactions: "
                          << count_if(history.transactions(), count_all);
  BOOST_LOG_TRIVIAL(info) << "#events: "
                          << count_if(history.events(), count_all);

  return history;
}

auto operator<<(std::ostream &os, const History &history) -> std::ostream & {
  auto out = std::osyncstream{os};

  for (const auto &session : history.sessions) {
    out << "\nSession " << session.id << ":\n";

    for (const auto &txn : session.transactions) {
      out << "Txn " << txn.id << ": ";

      for (const auto &event : txn.events) {
        out << (event.type == EventType::READ ? "R(" : "W(") << event.key
            << ", " << event.value << "), ";
      }

      out << "\n";
    }
  }

  return os;
}

auto n_rw_same_key_txns_of(History &history) -> int {
  int ret = 0;
  for (auto transaction : history.transactions()) {
    std::unordered_map<int64_t, bool> r_keys;
    for (auto [key, value, type, _] : transaction.events) {
      if (type == EventType::READ) {
        r_keys[key] = true;
      } else { // type == EventType::WRITE
        if (r_keys.contains(key)) {
          ++ret;
          break;
        }
      }
    }
  }
  return ret;
}

auto n_txns_of(History &history) -> int {
  int ret = 0;
  for (auto transaction : history.transactions()) ++ret;
  return ret;
}

auto n_written_key_txns_of(History &history) -> std::unordered_map<int64_t, int> {
  std::unordered_map<int64_t, std::unordered_set<int64_t>> written_txns_of_key;
  for (auto transaction : history.transactions()) {
    for (auto [key, value, type, txn_id] : transaction.events) {
      if (type != EventType::WRITE) continue;
      written_txns_of_key[key].insert(txn_id);
    }
  }
  std::unordered_map<int64_t, int> n_written_txns_of_key;
  for (const auto &[key, txns] : written_txns_of_key) {
    n_written_txns_of_key[key] = txns.size();
  }
  return n_written_txns_of_key;
}

auto parse_elle_list_append_history(std::ifstream &is) 
  -> std::pair<History, std::vector<std::tuple<int64_t, int64_t, int64_t>>> {
  // [sess] [txn_id] [type = 'R' or 'A'] [key] [value]
  // if type == 'R', value = n a1 a2 ... an
  // if type == 'A', value = v

  constexpr int64_t init_session_id = 0;
  constexpr int64_t init_txn_id = 0;
  constexpr int64_t INIT_VALUE = 0x7ff7f7f7f7f7f7f7;

  int n_lines = 0;
  is >> n_lines;

  auto txns = std::unordered_map<int64_t, Transaction>{};
  auto sessions = std::unordered_map<int64_t, std::vector<int64_t>>{}; // sess_id -> { txn_ids in SO order }
  auto known_ww = std::vector<std::tuple<int64_t, int64_t, int64_t>>{}; // <key, v1, v2>
  auto init_write_keys = std::set<int64_t> {};
  while (n_lines--) {
    int64_t session_id, txn_id, key, value;
    std::string type;
    is >> session_id >> txn_id >> type >> key;
    if (type == "R") { // read
      int n_list = 0;
      is >> n_list;
      auto list = std::vector<int64_t>(n_list);
      for (int i = 0; i < n_list; i++) is >> list[i];
      if (n_list == 0) {
        value = INIT_VALUE;
        init_write_keys.insert(key);
      } else {
        value = list[n_list - 1];
        for (int i = 0; i + 1 < n_list; i++) {
          known_ww.emplace_back(std::tuple<int64_t, int64_t, int64_t>{key, list[i], list[i + 1]});
        }
      }
    } else if (type == "A") { // append
      is >> value;
    }
    if (!txns.contains(txn_id)) {
      txns[txn_id] = Transaction{ .id = txn_id, .events = {}, .session_id = session_id, };
      sessions[session_id].emplace_back(txn_id);
    } 
    txns[txn_id].events.emplace_back((Event) {
      .key = key, 
      .value = value, 
      .type = (type == "R" ? EventType::READ : EventType::WRITE),
      .transaction_id = txn_id,
    });
  } 

  auto history = History{};
  history.sessions.emplace_back(Session{
    .id = init_session_id,
    .transactions = std::vector{Transaction{
        .id = init_txn_id,
        .events = init_write_keys  //
                  | transform([](auto key) {
                      return Event{
                          .key = key,
                          .value = INIT_VALUE,
                          .type = EventType::WRITE,
                          .transaction_id = init_txn_id,
                      };
                    })  //
                  | utils::to<std::vector<Event>>,
        .session_id = init_session_id,
    }},
  });
  for (const auto &[sess_id, txn_ids] : sessions) {
    auto session = Session{ .id = sess_id, .transactions = {} };
    for (const auto &txn_id : txn_ids) session.transactions.emplace_back(txns.at(txn_id));
    history.sessions.emplace_back(session);
  }

  return {history, known_ww};
}

auto compute_history_meta_info(const History &history) -> HistoryMetaInfo {
  auto history_meta_info = HistoryMetaInfo{
    .n_sessions = 0, 
    .n_total_transactions = 0, 
    .n_total_events = 0,
    .write_steps = {},
    .read_steps = {},
    .txn_distance = {},
    .session_id_of = {}
  };
  history_meta_info.n_sessions = history.sessions.size();
  for (const auto &txn : history.transactions()) {
    ++history_meta_info.n_total_transactions;
    history_meta_info.n_total_events += txn.events.size();
  }

  for (const auto &session : history.sessions) {
    int session_id = session.id;
    for (const auto &txn : session.transactions) {
      // assert(txn.session_id == session_id); // fail on Cobra Histories
      history_meta_info.session_id_of[txn.id] = session_id;
    }
  }

  for (const auto &session : history.sessions) {
    int steps = 0; // for each session, calculate a steps
    for (const auto &txn : session.transactions) {
      ++steps;
      history_meta_info.txn_distance[txn.id] = steps;
      auto cur_value = std::unordered_map<int64_t, int64_t>{}; // key -> value (for current txn)
      for (const auto &event : txn.events) {
        const auto &[key, value, type, txn_id] = event;
        if (type == EventType::READ) {
          if (!cur_value.contains(key)) {
            history_meta_info.read_steps[txn_id][key] = steps; // once update read steps
          } else {
            if (cur_value[key] != value) 
              throw std::runtime_error{"exception found in 1 txn."}; // violate ser
          }
        } else { // EventType::WRITE
          cur_value[key] = value;
          history_meta_info.write_steps[txn_id][key] = steps; // iteratively update write steps
        }
      }
    }
  }

  return history_meta_info;
};

auto analyze_repeat_values(const History &history) -> void {
  auto writes_on_key_value = std::unordered_map<int64_t, std::unordered_map<int64_t, int>>{}; // key -> (to -> count)
  bool unique_value = true;
  for (const auto &[key, value, type, txn_id] : history.events()) {
    if (type == EventType::READ) continue;
    // type == EventType::Write
    if (writes_on_key_value[key][value] != 0) unique_value = false;
    writes_on_key_value[key][value]++;
  }
  if (unique_value) {
    BOOST_LOG_TRIVIAL(debug) << "No multiple write on same (key, value). This history satisfies UniqueValue.";
  } else {
    BOOST_LOG_TRIVIAL(debug) << "Find multiple writes on same (key, value). This history does NOT satisfy UniqueValue";
  }
}

}  // namespace checker::history
