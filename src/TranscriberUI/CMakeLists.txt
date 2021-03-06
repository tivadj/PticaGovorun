project(TranscriberUI)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}")

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON) # runs uic


#find_package(PticaGovorunBackend REQUIRED)

find_package(Boost COMPONENTS filesystem REQUIRED) # TODO: TranscriberUI doesn't use boost::filesystem; why to enumerate?
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Gui REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

file(GLOB lib_srcs *.cpp)
file(GLOB lib_hdrs *.h)

add_executable(TranscriberUI ${lib_srcs} ${lib_hdrs})
target_link_libraries(TranscriberUI
	PRIVATE PticaGovorunBackend
	PRIVATE ${Boost_LIBRARIES}
	PRIVATE Qt5::Core
	PRIVATE Qt5::Widgets
	PRIVATE Qt5::Gui
)
if (WITH_PORTAUDIO)
	target_link_libraries(TranscriberUI PRIVATE PortAudio)
endif()


#set_target_properties(TranscriberUI PROPERTIES OUTPUT_NAME TranscriberUIOut)
#set (outName "tmp1")
#get_target_property(outName TranscriberUI OUTPUT_NAME)
#message("outName=${outName}")


#if (WITH_SPHINX)
#	target_link_libraries(TranscriberUI 
#		PRIVATE Sphinx_SphinxBase
#		PRIVATE Sphinx_PocketSphinx
#)
#endif()

#install (TARGETS TranscriberUI
#	RUNTIME DESTINATION bin
#	DESTINATION bin
#)

#add_custom_command(TARGET TranscriberUI POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        "${PROJECT_SOURCE_DIR}/libs/test.dll"
#        $<TARGET_FILE_DIR:MyTest>) 