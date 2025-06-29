#include <cstdint>
#include <ranges>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "history/constraint.h"
#include "history/dependencygraph.h"
#include "history/history.h"
#include "solver/pruner.h"
#include "solver/acyclicMinisatSolver.h"
#include "utils/log.h"
#include "utils/to_container.h"

#define BOOST_TEST_MODULE acyclicMinisatSolver
#include <boost/test/included/unit_test.hpp>

using checker::history::Event;
using checker::history::EventType;
using checker::history::History;
using checker::history::Session;
using checker::history::Transaction;
using checker::utils::to;
using EventType::READ;
using EventType::WRITE;
using std::tuple;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::ranges::views::all;
using std::ranges::views::transform;

static auto check_history(const History &h) {
  auto depgraph = checker::history::known_graph_of(h);
  auto cons = checker::history::constraints_of(h);
  auto hist_meta = checker::history::compute_history_meta_info(h);

  CHECKER_LOG_COND(trace, logger) {
    logger << "history: " << h << "\ndependency graph:\n"
           << depgraph;

    logger << "constraints\n";
    logger << "ww: \n";
    const auto &[ww_constraints, wr_constraints] = cons;
    for (const auto &c : ww_constraints) { logger << c; }
    logger << "wr: \n";
    for (const auto &c : wr_constraints) { logger << c; }
  }
  return checker::solver::prune_constraints(depgraph, cons) &&
    checker::solver::AcyclicMinisatSolver{depgraph, cons, hist_meta, "ser"}.solve();
}

static auto create_history(
    const unordered_map<int64_t, vector<int64_t>> &session_txn_ids,
    const unordered_map<int64_t, vector<tuple<EventType, int64_t, int64_t>>>
        &events) -> History {
  // clang-format off
  return History{
      .sessions = session_txn_ids | transform([&](const auto &s) {
        const auto &[id, txn_ids] = s;
        return Session{
          .id = id,
          .transactions = txn_ids | transform([&](auto txn_id) {
            return Transaction{
              .id = txn_id,
              .events = events.at(txn_id) | transform([&](auto e) {
                auto [t, k, v] = e;
                return Event {
                  .key = k,
                  .value = v,
                  .type = t,
                  .transaction_id = txn_id,
                };
              }) | to<vector<Event>>,
              .session_id = id,
            };
          }) | to<vector<Transaction>>,
        };
      }) | to<vector<Session>>,
  };
  // clang-format on
}

BOOST_AUTO_TEST_CASE(read_committed) {
  auto h = create_history(
      {
          {0, {0, 1}},
          {1, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {WRITE, 1, 2},
               {WRITE, 2, 2},
           }},
          {2,
           {
               {READ, 2, 2},
               {READ, 1, 1},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(repeatable_read) {
  auto h = create_history(
      {
          {0, {0, 1}},
          {1, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {READ, 1, 2},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(read_my_writes) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1, 2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
               {WRITE, 2, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {WRITE, 2, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {READ, 2, 1},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(repeatable_read_2) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
               {WRITE, 2, 1},
           }},
          {1,
           {
               {WRITE, 1, 2},
               {WRITE, 2, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {READ, 2, 2},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(causal) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
          {3, {3}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {READ, 1, 2},
               {WRITE, 2, 1},
           }},
          {2,
           {
               {READ, 1, 1},
               {WRITE, 1, 2},
           }},
          {3,
           {
               {READ, 1, 1},
               {READ, 2, 1},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(prefix) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
          {3, {3}},
          {4, {4}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
               {WRITE, 2, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 2},
               {READ, 2, 1},
           }},
          {3,
           {
               {READ, 2, 1},
               {WRITE, 2, 2},
           }},
          {4,
           {
               {READ, 2, 2},
               {READ, 1, 1},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(conflict) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {WRITE, 1, 3},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(serializability) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
               {WRITE, 2, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {READ, 2, 1},
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {READ, 2, 1},
               {WRITE, 2, 2},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(tidb1) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 0},
               {WRITE, 2, 0},
               {WRITE, 3, 0},
           }},
          {1,
           {
               {READ, 1, 0},
               {WRITE, 2, 1},
               {WRITE, 3, 1},
           }},
          {2,
           {
               {READ, 2, 0},
               {WRITE, 1, 1},
               {WRITE, 3, 2},
           }},
      });

  BOOST_TEST(!check_history(h));
}

BOOST_AUTO_TEST_CASE(serializability_2) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
               {WRITE, 2, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {READ, 2, 1},
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 2},
               {READ, 2, 1},
               {WRITE, 2, 2},
           }},
      });

  BOOST_TEST(check_history(h));
}

BOOST_AUTO_TEST_CASE(write_propagation) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {WRITE, 1, 2},
               {WRITE, 2, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {WRITE, 2, 1},
           }},
      });

  BOOST_TEST(check_history(h));
}

BOOST_AUTO_TEST_CASE(pruning) {
  auto h = create_history(
      {
          {0, {0}},
          {1, {1}},
          {2, {2}},
          {3, {3}},
      },
      {
          {0,
           {
               {WRITE, 1, 1},
           }},
          {1,
           {
               {READ, 1, 1},
               {READ, 2, 2},
               {WRITE, 1, 2},
           }},
          {2,
           {
               {READ, 1, 1},
               {WRITE, 1, 1},
           }},
          {3,
           {
               {WRITE, 2, 2},
           }},
      });

  BOOST_TEST(check_history(h));
}
