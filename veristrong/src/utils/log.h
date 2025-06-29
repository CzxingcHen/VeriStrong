#ifndef CHECKER_LOG_H
#define CHECKER_LOG_H

#include <boost/log/trivial.hpp>

#define CHECKER_LOG_COND(level, var_name)                                   \
  if (auto __record = ::boost::log::trivial::logger::get().open_record(     \
          ::boost::log::keywords::severity = ::boost::log::trivial::level); \
      __record)                                                             \
    if (auto __pump = ::boost::log::aux::make_record_pump(                  \
            ::boost::log::trivial::logger::get(), __record);                \
        true)                                                               \
      if (auto &var_name = __pump.stream(); true)

#endif  // CHECKER_LOG_H
