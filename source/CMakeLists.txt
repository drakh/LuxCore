#############################################################################
#
# vectary pt binary
#
#############################################################################

include_directories(utilities)
include_directories(vectary_data)

set(VCTRPT_SRCS
		main.cpp
		utilities/log.cpp
		utilities/luxutil.cpp
		vectary_data/decoder.cpp
	)

add_executable(pt ${VCTRPT_SRCS})

TARGET_LINK_LIBRARIES(pt ${LUXCORE_LIBRARY} ${Boost_LIBRARIES} ${OPENCL_LIBRARIES})