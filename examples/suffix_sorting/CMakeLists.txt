################################################################################
# examples/suffix_sorting/CMakeLists.txt
#
# Part of Project Thrill - http://project-thrill.org
#
# Copyright (C) 2015-2016 Timo Bingmann <tb@panthema.net>
# Copyright (C) 2016 Florian Kurpicz <florian.kurpicz@tu-dortmund.de>
#
# All rights reserved. Published under the BSD-2 license in the LICENSE file.
################################################################################

thrill_build_prog(prefix_doubling sa_checker.cpp)
thrill_build_prog(wavelet_tree)

thrill_test_single(prefix_doubling1_aaa3 "THRILL_LOCAL=3"
  prefix_doubling -c -s 10000 -a fl unary)
thrill_test_single(prefix_doubling1_aaa4 "THRILL_LOCAL=4"
  prefix_doubling -c -s 10000 -a fl unary)

thrill_test_single(prefix_doubling1_random3 "THRILL_LOCAL=3"
  prefix_doubling -c -s 100000 -a fl random)
thrill_test_single(prefix_doubling1_random4 "THRILL_LOCAL=4"
  prefix_doubling -c -s 100000 -a fl random)

thrill_test_single(prefix_doubling2_aaa3 "THRILL_LOCAL=3"
  prefix_doubling -c -s 10000 -a de unary)
thrill_test_single(prefix_doubling2_aaa4 "THRILL_LOCAL=4"
  prefix_doubling -c -s 10000 -a de unary)

thrill_test_single(prefix_doubling2_random3 "THRILL_LOCAL=3"
  prefix_doubling -c -s 100000 -a de random)
thrill_test_single(prefix_doubling2_random4 "THRILL_LOCAL=4"
  prefix_doubling -c -s 100000 -a de random)

if(MSVC)
  # requires /bigobj flag to build
  set_target_properties(prefix_doubling PROPERTIES COMPILE_FLAGS /bigobj)
endif()

if(THRILL_FULL_BUILD)
  thrill_build_prog(dc3 sa_checker.cpp)

  if(MSVC)
    # requires /bigobj flag to build
    set_target_properties(dc3 PROPERTIES COMPILE_FLAGS /bigobj)
  endif()

  thrill_test_single(dc3_aaa3 "THRILL_LOCAL=3" dc3 -c -s 10000 unary)
  thrill_test_single(dc3_aaa4 "THRILL_LOCAL=4" dc3 -c -s 10000 unary)

  thrill_test_single(dc3_random3 "THRILL_LOCAL=3" dc3 -c -s 100000 random)
  thrill_test_single(dc3_random4 "THRILL_LOCAL=4" dc3 -c -s 100000 random)
endif()

################################################################################
