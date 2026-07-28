#ifndef P101_DOCTOR_ARGUMENTS_H
#define P101_DOCTOR_ARGUMENTS_H

#include "constants.h"
#include <stdbool.h>

enum
{
    PATH_LEN = 1024
};

struct arguments
{
    const char  *doctor_dir;
    const char  *source_paths[MAX_SOURCE_PATHS];
    const char  *fault_count_str;
    unsigned int fault_count;
    const char  *p101_wrapper_audit;
    const char  *p101_error_contract;
    const char  *p101_module_map;
    const char  *p101_observe;
    const char  *p101_error_path_walk;
    const char  *resource_tracker;
    const char  *p101_trace;
    const char  *p101_report;
    char *const *command_argv;
    int          command_argc;
    int          source_count;
    bool         verbose;
    bool         skip_source_contracts;
    bool         source_paths_set;
};

#endif    // P101_DOCTOR_ARGUMENTS_H
