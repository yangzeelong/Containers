if(WITH_TESTING)
    include("cmake/3rdparty/gtest.cmake")
endif()

if(WITH_BENCHMARK)
    include("cmake/3rdparty/gbenchmark.cmake")
endif()

include("cmake/Summary.cmake")
