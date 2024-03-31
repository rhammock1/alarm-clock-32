file(GLOB_RECURSE SOURCES "*.c")

idf_component_register(SRCS "${SOURCES}" "../lib/littlefs/lfs.c" "../lib/littlefs/lfs_util.c"
                       INCLUDE_DIRS "." "include" "../lib/littlefs")
