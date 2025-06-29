#include "minisat/core/Logger.h"

namespace Minisat::Logger {

auto log(std::string str, std::string end) -> bool {

#ifdef LOGGER_ENABLED
  std::cout << str << end;
  return true;
#else
  return false;
#endif

}

auto type2str(int type) -> std::string {
  std::string ret = "";
  switch (type) {
    case 0: ret = "SO"; break;
    case 1: ret = "WW"; break;
    case 2: ret = "WR"; break;
    case 3: ret = "RW"; break;
    default: assert(false);
  } 
  return ret;
}

// Templates vec2str, set2str and urdset2str have been moved into Logger.h

auto lits2str(vec<Lit> &lits) -> std::string {
  auto os = std::ostringstream{};
  for (int i = 0; i < lits.size(); i++) {
    const Lit &l = lits[i];
    int x = l.x >> 1;
    if (l.x & 1) os << "-";
    os << std::to_string(x);
    if (i + 1 != lits.size()) os << ", ";
  }
  return os.str(); 
}

} // namespace Minisat::Logger