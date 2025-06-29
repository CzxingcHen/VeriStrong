project('smt-tc-checker', 'cpp', version: '0.0.1')

cmake = import('cmake')
fs = import('fs')

# z3
# z3_dep = dependency('z3', required: false)

# if not z3_dep.found()
#   z3_opts = cmake.subproject_options()
#   z3_opts.add_cmake_defines({
#     'Z3_ENABLE_EXAMPLE_TARGETS': 'OFF',
#     'Z3_BUILD_EXECUTABLE': 'OFF',
#     'Z3_BUILD_TEST_EXECUTABLES': 'OFF',
#     'Z3_BUILD_LIBZ3_SHARED': 'OFF',
#   })
#   if get_option('buildtype') == 'debug'
#     z3_opts.add_cmake_defines({
#       'CMAKE_BUILD_TYPE': 'Debug',
#     })
#   elif get_option('buildtype') == 'release'
#     z3_opts.add_cmake_defines({
#       'CMAKE_BUILD_TYPE': 'Release',
#       'Z3_LINK_TIME_OPTIMIZATION': 'ON',
#       'Z3_SINGLE_THREADED': 'ON',
#     })
#   elif get_option('buildtype') == 'debugoptimized'
#     z3_opts.add_cmake_defines({
#       'CMAKE_BUILD_TYPE': 'RelWithDebInfo',
#       'CMAKE_CXX_FLAGS': '-pg',
#       'Z3_SINGLE_THREADED': 'ON',
#     })
#   endif

#   z3_pro = cmake.subproject('z3', options: z3_opts)
#   z3_dep = declare_dependency(
#     include_directories: include_directories('subprojects/z3/src/api/c++'),
#     dependencies: z3_pro.dependency('libz3')
#   )
# endif

# argparse
argparse_dep = dependency('argparse', required: false)

if not argparse_dep.found()
  argparse_pro = cmake.subproject('argparse')
  argparse_dep = argparse_pro.dependency('argparse')
endif

# boost
boost_dep = dependency('boost', modules: ['log'])

# monosat
monosat_dep = dependency('monosat', required: false)
if not monosat_dep.found()
  monosat_pro = cmake.subproject('monosat')
  monosat_dep = declare_dependency(
    include_directories: include_directories('subprojects/monosat/src/monosat/api'),
    dependencies: monosat_pro.dependency('libmonosat')
  )
endif

# acyclic-minisat
acyclic_minisat_dep = dependency('acyclic_minisat', required: false)
if not acyclic_minisat_dep.found()
  acyclic_minisat_pro = cmake.subproject('acyclic-minisat')
  acyclic_minisat_dep = declare_dependency(
    include_directories: include_directories('subprojects/acyclic-minisat/minisat/api'),
    dependencies: acyclic_minisat_pro.dependency('minisat-lib-shared')
  )
endif

# acyclic-minisat-si
acyclic_minisat_si_dep = dependency('acyclic_minisat_si', required: false)
if not acyclic_minisat_si_dep.found()
  acyclic_minisat_si_pro = cmake.subproject('acyclic-minisat-si')
  acyclic_minisat_si_dep = declare_dependency(
    include_directories: include_directories('subprojects/acyclic-minisat-si/minisat/api'),
    dependencies: acyclic_minisat_si_pro.dependency('minisat-si-lib-shared')
  )
endif

subdir('src')

checker_deps = [argparse_dep, boost_dep, monosat_dep, acyclic_minisat_dep, acyclic_minisat_si_dep] # remove 'z3_dep'
checker_opts = ['cpp_std=c++20']
checker_incs = ['src']
checker_cflags = []
checker_ldflags = []
if get_option('buildtype') == 'debug'
  # temporarily disable address sanitizer, for monosat leads to too many mistakes, qaq
  # checker_opts += ['b_sanitize=address']
elif get_option('buildtype') == 'debugoptimized'
  checker_cflags += ['-pg']
  checker_ldflags += ['-pg']
elif get_option('buildtype') == 'release'
  checker_opts += ['b_lto=true']
  checker_cflags += ['-Ofast']
endif

checker_common_deps = declare_dependency(
  dependencies: checker_deps,
  include_directories: checker_incs,
  link_with: [
    static_library(
      'checker_lib',
      checker_srcs,
      dependencies: checker_deps,
      override_options: checker_opts,
      include_directories: checker_incs,
      cpp_args: checker_cflags,
      link_args: checker_ldflags,
    )
  ],
)

executable(
  'checker',
  files('src/main.cpp'),
  dependencies: checker_common_deps,
  override_options: checker_opts,
  cpp_args: checker_cflags,
  link_args: checker_ldflags,
)

foreach f : checker_test_srcs
  test(
    fs.stem(f),
    executable(
      fs.stem(f) + '_test',
      [f],
      dependencies: checker_common_deps,
      override_options: checker_opts,
      cpp_args: checker_cflags,
      link_args: checker_ldflags,
    )
  )
endforeach
