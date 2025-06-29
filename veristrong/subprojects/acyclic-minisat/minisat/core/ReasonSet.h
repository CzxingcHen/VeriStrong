#ifndef MINISAT_REASONSET_H
#define MINISAT_REASONSET_H

#include <unordered_set>
#include <unordered_map>

namespace Minisat {

constexpr static auto pair_hash_endpoint3 = [](const auto &t) {
  auto &[t1, t2] = t;
  std::hash<int> h;
  return h(t1) ^ h(t2); 
};

struct ReasonSet {
  std::unordered_map<int, std::unordered_map<int, bool>> is_known;
  std::unordered_map<int, std::unordered_map<int, std::unordered_multiset<int>>> ww_only, wr_only;
  std::unordered_map<int, std::unordered_map<int, std::unordered_multiset<std::pair<int, int>, decltype(pair_hash_endpoint3)>>> ww_and_wr;

  void add_reason_edge(int from, int to, const std::pair<int, int> &reason) {
    // if (from, to) exists, return false, else return true
    const auto &[ww_reason, wr_reason] = reason;
    if (ww_reason == -1 && wr_reason == -1) { // known
      assert(!is_known[from][to]);
      is_known[from][to] = true;
    } else if (ww_reason != -1 && wr_reason == -1) {
      ww_only[from][to].insert(ww_reason);
    } else if (ww_reason == -1 && wr_reason != -1) {
      wr_only[from][to].insert(wr_reason);
    } else {
      ww_and_wr[from][to].insert(reason);
    }
  }

  void remove_reason_edge(int from, int to, const std::pair<int, int> &reason) {
    const auto &[ww_reason, wr_reason] = reason;
    if (ww_reason == -1 && wr_reason == -1) { // known
      assert(false); // cannot remove known edge
    } else if (ww_reason != -1 && wr_reason == -1) {
      assert(ww_only.contains(from) && ww_only[from].contains(to) && ww_only[from][to].contains(ww_reason)); 
      ww_only[from][to].erase(ww_only[from][to].find(ww_reason));
    } else if (ww_reason == -1 && wr_reason != -1) {
      assert(wr_only.contains(from) && wr_only[from].contains(to) && wr_only[from][to].contains(wr_reason)); 
      wr_only[from][to].erase(wr_only[from][to].find(wr_reason));
    } else {
      assert(ww_and_wr.contains(from) && ww_and_wr[from].contains(to) && ww_and_wr[from][to].contains(reason)); 
      ww_and_wr[from][to].erase(ww_and_wr[from][to].find(reason));
    }
  }

  bool any(int from, int to) {
    if (is_known.contains(from) && is_known[from][to]) return true;
    if (ww_only.contains(from) && ww_only[from].contains(to) && !ww_only[from][to].empty()) return true;
    if (wr_only.contains(from) && wr_only[from].contains(to) && !wr_only[from][to].empty()) return true;
    if (ww_and_wr.contains(from) && ww_and_wr[from].contains(to) && !ww_and_wr[from][to].empty()) return true;
    return false;
  }

  bool known(int from, int to) { return is_known.contains(from) && is_known[from][to]; }

  bool dep(int from, int to) { return any(from, to) && (!known(from, to)); }

  std::pair<int, int> get_minimal_reason(int from, int to) {
    assert(any(from, to)); // should call any(from, to) first
    if (is_known.contains(from) && is_known[from][to]) return {-1, -1};
    if (ww_only.contains(from) && ww_only[from].contains(to) && !ww_only[from][to].empty()) {
      int r = *ww_only[from][to].begin();
      return {r, -1};
    }
    if (wr_only.contains(from) && wr_only[from].contains(to) && !wr_only[from][to].empty()) {
      int r = *wr_only[from][to].begin();
      return {-1, r};
    }
    if (ww_and_wr.contains(from) && ww_and_wr[from].contains(to) && !ww_and_wr[from][to].empty()) {
      return *ww_and_wr[from][to].begin();
    }
    assert(false); // never reach here
  }

}; // struct ReasonSet

} // namespace Minisat



#endif // MINISAT_REASONSET_H