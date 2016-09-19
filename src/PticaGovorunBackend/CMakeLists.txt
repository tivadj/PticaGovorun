project(PticaGovorunBackend)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

#
message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}") #
message(STATUS "CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P}") #
if (CMAKE_SIZEOF_VOID_P MATCHES 8)
	set(PROJECT_ARCH "x86_64")
	set(PLATFORM_SHORT_NAME "x64")
else(CMAKE_SIZEOF_VOID_P MATCHES 4)
	set(PROJECT_ARCH "x86")
	set(PLATFORM_SHORT_NAME "x86")
endif()
#message("Debug=" $<CONFIG:DEBUG>)
message(STATUS "PROJECT_ARCH=${PROJECT_ARCH}") #
message(STATUS "PLATFORM_SHORT_NAME=${PLATFORM_SHORT_NAME}") #


# compiler directives
add_definitions(-D_USE_MATH_DEFINES) # define std::max in <math.h>
add_definitions(-DNOMINMAX) # do not define std::max in <windows.h>
add_definitions(-DPGAPI_EXPORTS) # declare API as exported

#find_package(OpenCV REQUIRED core highgui)
#find_package(Boost REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED) # for QApplication in AppHelpers::mapPath

#include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIR})
include_directories(${QT_INCLUDES}) # ?? required?
#add_definitions(${QT_DEFINITIONS})

# ZLib library
set(ZLib_INCLUDE_DIRS "" CACHE PATH "")
set(ZLib_LIB_DEBUG "" CACHE PATH "")
set(ZLib_LOCATION_DEBUG "" CACHE PATH "")
set(ZLib_LIB_RELEASE "" CACHE PATH "")
set(ZLib_LOCATION_RELEASE "" CACHE PATH "")

add_library(ZLib SHARED IMPORTED)
set_target_properties(ZLib PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES "${ZLib_INCLUDE_DIRS}"
	INTERFACE_COMPILE_DEFINITIONS ZLIB_WINAPI
	IMPORTED_IMPLIB_DEBUG ${ZLib_LIB_DEBUG}
	IMPORTED_LOCATION_DEBUG ${ZLib_LOCATION_DEBUG}
	IMPORTED_IMPLIB_RELEASE ${ZLib_LIB_RELEASE}
	IMPORTED_LOCATION_RELEASE ${ZLib_LOCATION_DEBUG}
)

#
file(GLOB lib_srcs *.cpp)
file(GLOB lib_hdrs *.h)

add_library(PticaGovorunBackend SHARED ${lib_srcs} ${lib_hdrs})
target_link_libraries(PticaGovorunBackend
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE Qt5::Core
	PRIVATE Qt5::Xml
	PRIVATE Qt5::Widgets
	PRIVATE LibSampleRate
)
if (WITH_LIBSNDFILE)
	target_link_libraries(PticaGovorunBackend PRIVATE LibSndFile)
endif()
if (WITH_FLAC)
	target_link_libraries(PticaGovorunBackend
			PRIVATE FlacC
			PRIVATE FlacCpp)
endif()
if (WITH_JULIUS)
	target_link_libraries(PticaGovorunBackend
			PRIVATE Julius4_LibSent
			PRIVATE Julius4_LibJulius
			PRIVATE Julius4_Julius)
endif()
if (WITH_SPHINX)
	target_link_libraries(PticaGovorunBackend
			PRIVATE Sphinx_SphinxBase
			PRIVATE Sphinx_PocketSphinx)
endif()


#target_include_directories(PticaGovorunBackend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(PticaGovorunBackend PUBLIC
	#$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/mylib>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include/mylib>  # <prefix>/include/mylib
)

export(PACKAGE PticaGovorunBackend)

install (TARGETS PticaGovorunBackend
	RUNTIME DESTINATION bin
	DESTINATION bin
)


#set_target_properties(PticaGovorunBackend PROPERTIES INTERFACE_LINK_LIBRARIES Sphinx_SphinxBase) # TODO: doesn't work (interface?)
# target_link_libraries(PticaGovorunBackend
	# LibJulius_$(PlatformShortName).lib
	# LibSent_$(PlatformShortName).lib
	# Julius_$(PlatformShortName).lib
	# ws2_32.lib
	# )
#target_link_libraries(PticaGovorunBackend ${SphinxBase_LIBRARIES})
#${CMAKE_CURRENT_BINARY_DIR}
#
#add_custom_command(TARGET PticaGovorunBackend POST_BUILD
#	COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${OpenCV_DIR}/x64/vc12/bin/opencv_core300d.dll" $<TARGET_FILE_DIR:PticaGovorunBackend>
#)

# RESULT:
#-DMSVC_BUILD_CONFIG_DIR=$(Configuration) -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/devb/PticaGovorunProj/srcrep/build11 -P copyBundledBins.cmake
#add_custom_command(TARGET PticaGovorunBackend POST_BUILD
#  COMMAND ${CMAKE_COMMAND}
#	-DMSVC_BUILD_CONFIG_DIR=${CMAKE_CFG_INTDIR}
#	-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#	-DOUT_DIR1=$<TARGET_FILE_DIR:PticaGovorunBackend>
#	#-P "${CMAKE_CURRENT_SOURCE_DIR}/copyBundledBins.cmake"
#	-P "${PROJECT_SOURCE_DIR}/../copyBundledBins.cmake"
#  VERBATIM
#)
#!!!!!!!!!!!!!!!!!
#add_custom_command(TARGET CopyDlls
#		COMMAND ${CMAKE_COMMAND}
#		-DMSVC_BUILD_CONFIG_DIR=${CMAKE_CFG_INTDIR}
#		-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
#		-P "<pathto>/yourCopyDlls.cmake"
#		VERBATIM
#		)