add_subdirectory (include/platform)
add_subdirectory (rpmsg_lite/porting/platform)
add_subdirectory (rpmsg_lite/porting/environment)

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

zephyr_sources (rpmsg_lite/rpmsg_lite.c)
zephyr_sources (rpmsg_lite/rpmsg_ns.c)
zephyr_sources (rpmsg_lite/rpmsg_queue.c)

zephyr_sources (virtio/virtqueue.c)
zephyr_sources (common/llist.c)
