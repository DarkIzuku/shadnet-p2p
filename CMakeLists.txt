cmake_minimum_required(VERSION 3.16)
project(shadnet VERSION 1.0.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# === Runtime library: must match Qt6 which always uses DLL runtime ===
if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    # /MD (release) or /MDd (debug) — DLL runtime, same as Qt6.
    # protobuf_MSVC_STATIC_RUNTIME=OFF ensures protobuf follows the same setting.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
    add_compile_options(/EHsc /utf-8)
    # Remove /VERBOSE:LIB from link options (noisy, only needed for debugging)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Sql Concurrent HttpServer)

add_subdirectory(externals)

set(PROTO_OUT "${CMAKE_CURRENT_BINARY_DIR}/proto_gen")
file(MAKE_DIRECTORY "${PROTO_OUT}")

add_custom_command(
    OUTPUT
        "${PROTO_OUT}/shadnet.pb.cc"
        "${PROTO_OUT}/shadnet.pb.h"
    COMMAND $<TARGET_FILE:protoc>
        "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}/src"
        "--cpp_out=${PROTO_OUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shadnet.proto"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shadnet.proto"
        protoc
    COMMENT "Generating shadnet protobuf sources"
    VERBATIM
)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/config.h
    src/account_registration.cpp
    src/account_registration.h
    src/server_logger.cpp
    src/server_logger.h
    src/server.cpp
    src/server.h
    src/client_session.cpp
    src/client_session.h
    src/protocol.h
    src/stream_extractor.h
    src/database.cpp
    src/database.h
    src/cmd_account.cpp
    src/cmd_friend.cpp
    src/score_types.h
    src/score_db.h
    src/score_db.cpp
    src/score_cache.cpp
    src/score_cache.h
    src/score_files.h
    src/cmd_score.cpp
    src/proto_utils.h
    "${PROTO_OUT}/shadnet.pb.cc"
    # Matchmaking
    src/matching.h
    src/cmd_matching.cpp
    src/stun_server.h
    src/stun_server.cpp
    # WebAPI HTTP/JSON listener
    src/webapi_server.h
    src/webapi_server.cpp
    src/bloodborne_website.h
    src/bloodborne_website.cpp
    web/website.qrc
    src/webapi_auth.h
    src/webapi_auth.cpp
    src/webapi_routes_users.h
    src/webapi_routes_users.cpp
    src/webapi_routes_profile.h
    src/webapi_routes_profile.cpp
    src/webapi_routes_presence.h
    src/webapi_routes_presence.cpp
    src/webapi_routes_session.h
    src/webapi_routes_session.cpp
    src/bloodborne_summon_broker.h
    src/bloodborne_summon_broker.cpp
    src/bloodborne_bootstrap.h
    src/bloodborne_bootstrap.cpp
    src/bloodborne_online_service.h
    src/bloodborne_online_service.cpp
    src/bloodborne_ssinfo_reference.h
    src/bloodborne_ssinfo_reference.cpp
    src/bloodborne_reference_proxy.h
    src/bloodborne_reference_proxy.cpp
    src/webapi_routes_bloodborne.h
    src/webapi_routes_bloodborne.cpp
    src/webapi_routes_bloodborne_bootstrap.h
    src/webapi_routes_bloodborne_bootstrap.cpp
    src/webapi_routes_common.h
    # Read-only stats HTTP/JSON listener
    src/stats_server.h
    src/stats_server.cpp
)

add_executable(shadnet ${SOURCES})

target_include_directories(shadnet PRIVATE
    src
    "${PROTO_OUT}"                                    
    "${CMAKE_CURRENT_SOURCE_DIR}/externals/protobuf/src"
)

target_link_libraries(shadnet PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::HttpServer
    libprotobuf
)

if(BUILD_TESTING)
    add_executable(test_bloodborne_summon_broker EXCLUDE_FROM_ALL
        tests/test_bloodborne_summon_broker.cpp
        src/bloodborne_summon_broker.cpp
    )
    target_include_directories(test_bloodborne_summon_broker PRIVATE src)
    target_link_libraries(test_bloodborne_summon_broker PRIVATE Qt6::Core)
    add_test(NAME bloodborne_summon_broker COMMAND test_bloodborne_summon_broker)

    add_executable(test_bloodborne_bootstrap EXCLUDE_FROM_ALL
        tests/test_bloodborne_bootstrap.cpp
        src/bloodborne_bootstrap.cpp
        src/bloodborne_online_service.cpp
        src/bloodborne_ssinfo_reference.cpp
        src/bloodborne_summon_broker.cpp
        src/config.cpp
        src/webapi_routes_bloodborne.cpp
        src/webapi_routes_bloodborne_bootstrap.cpp
        src/database.cpp
        "${PROTO_OUT}/shadnet.pb.h"
    )
    target_include_directories(test_bloodborne_bootstrap PRIVATE
        src
        "${PROTO_OUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/externals/protobuf/src"
    )
    target_link_libraries(test_bloodborne_bootstrap PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Sql
        Qt6::HttpServer
        libprotobuf
    )
    add_test(NAME bloodborne_bootstrap COMMAND test_bloodborne_bootstrap)

    add_executable(test_bloodborne_online_service EXCLUDE_FROM_ALL
        tests/test_bloodborne_online_service.cpp
        src/bloodborne_online_service.cpp
        src/database.cpp
    )
    target_include_directories(test_bloodborne_online_service PRIVATE src)
    target_link_libraries(test_bloodborne_online_service PRIVATE Qt6::Core Qt6::Sql)
    add_test(NAME bloodborne_online_service COMMAND test_bloodborne_online_service)

    add_executable(test_bloodborne_reference_proxy EXCLUDE_FROM_ALL
        tests/test_bloodborne_reference_proxy.cpp
        src/bloodborne_reference_proxy.cpp
    )
    target_include_directories(test_bloodborne_reference_proxy PRIVATE src)
    target_link_libraries(test_bloodborne_reference_proxy PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::HttpServer
    )
    add_test(NAME bloodborne_reference_proxy COMMAND test_bloodborne_reference_proxy)

    add_executable(test_server_logger EXCLUDE_FROM_ALL
        tests/test_server_logger.cpp
        src/server_logger.cpp
    )
    target_include_directories(test_server_logger PRIVATE src)
    target_link_libraries(test_server_logger PRIVATE Qt6::Core)
    add_test(NAME server_logger COMMAND test_server_logger)

    add_executable(test_bloodborne_website EXCLUDE_FROM_ALL
        tests/test_bloodborne_website.cpp
        src/account_registration.cpp
        src/bloodborne_website.cpp
        src/config.cpp
        src/database.cpp
        web/website.qrc
        "${PROTO_OUT}/shadnet.pb.h"
    )
    target_include_directories(test_bloodborne_website PRIVATE
        src
        "${PROTO_OUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/externals/protobuf/src"
    )
    target_link_libraries(test_bloodborne_website PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Sql
        Qt6::HttpServer
        libprotobuf
    )
    add_test(NAME bloodborne_website COMMAND test_bloodborne_website)

    add_executable(bloodborne_matching_guest_probe EXCLUDE_FROM_ALL
        tests/bloodborne_matching_guest_probe.cpp
        "${PROTO_OUT}/shadnet.pb.cc"
    )
    target_include_directories(bloodborne_matching_guest_probe PRIVATE
        src
        "${PROTO_OUT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/externals/protobuf/src"
    )
    target_link_libraries(bloodborne_matching_guest_probe PRIVATE libprotobuf)
endif()

add_custom_command(TARGET shadnet POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/worlds.cfg"
        "$<TARGET_FILE_DIR:shadnet>/worlds.cfg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scoreboards.cfg"
        "$<TARGET_FILE_DIR:shadnet>/scoreboards.cfg"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:shadnet>/web"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
        "$<TARGET_FILE_DIR:shadnet>/web/index.html"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/web/assets"
        "$<TARGET_FILE_DIR:shadnet>/web/assets"
)


install(TARGETS shadnet RUNTIME DESTINATION bin)
install(FILES worlds.cfg scoreboards.cfg DESTINATION bin)
install(FILES web/index.html DESTINATION bin/web)
install(DIRECTORY web/assets DESTINATION bin/web)
