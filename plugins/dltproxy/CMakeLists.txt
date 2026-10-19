dlt_add_plugin(dltproxyplugin
    dltproxy_plugin.h
    dltproxy_plugin.cpp
)

find_package(Threads REQUIRED)
target_link_libraries(dltproxyplugin PRIVATE dlt_proxy Threads::Threads)
