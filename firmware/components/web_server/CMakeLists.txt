idf_component_register(
    SRCS "src/web_server.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server lwip esp_wifi esp_netif wifi_manager
             config_manager cjson mdns_manager ota_manager
    EMBED_TXTFILES "index.html"
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
    COMMAND bun install
    COMMAND bun run build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/src/index.html" "${CMAKE_CURRENT_SOURCE_DIR}/web/src/main.ts" "${CMAKE_CURRENT_SOURCE_DIR}/web/src/style.css"
    COMMENT "Building web frontend..."
)

add_custom_target(build_web_frontend
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/index.html"
)
add_dependencies(${COMPONENT_LIB} build_web_frontend)
