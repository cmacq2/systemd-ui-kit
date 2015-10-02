set(tokeniser_SRCS tokeniser_sample.cpp)

add_executable(tokeniser_sample ${tokeniser_SRCS} $<TARGET_OBJECTS:unit_file_parser> $<TARGET_OBJECTS:utf8>)
target_link_libraries(tokeniser_sample Qt5::Core)