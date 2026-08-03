set(PROJECT_NAME "p101-doctor")
set(PROJECT_VERSION "1.0.0")
set(PROJECT_DESCRIPTION "Programming 101 doctor runner")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Common compiler flags
set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS
)

set(BSD_STANDARD_FLAGS
)

# Define targets
set(EXECUTABLE_TARGETS main)
set(LIBRARY_TARGETS "")
set(main_OUTPUT_NAME p101-doctor)

set(main_SOURCES
        src/cli.c
        src/main.c
        src/paths.c
        src/report.c
        src/runner.c
        src/source_inputs.c
        src/status.c
)

set(main_HEADERS
        include/arguments.h
        include/cli.h
        include/constants.h
        include/errors.h
        include/paths.h
        include/report.h
        include/runner.h
        include/source_inputs.h
        include/status.h
)

set(main_LINK_LIBRARIES
        p101_error
        p101_env
        p101_record
        p101_c
        p101_cli
        p101_filesystem
        p101_process
        p101_convert
        p101_c_facts
        p101_util
        m
)
