add_executable(gen_sys gen_sys.c)
add_executable(gen_pthread gen_pthread.c)

add_custom_command(TARGET gen_sys POST_BUILD
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gen_sys > ${CMAKE_CURRENT_SOURCE_DIR}/../modules/__sys.bjou
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating '__sys.bjou'")
add_custom_command(TARGET gen_pthread POST_BUILD
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gen_pthread > ${CMAKE_CURRENT_SOURCE_DIR}/../modules/__pthread.bjou
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating '__pthread.bjou'")
