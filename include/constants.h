#ifndef P101_DOCTOR_CONSTANTS_H
#define P101_DOCTOR_CONSTANTS_H

enum
{
    MSG_LEN             = 256,
    MAX_TOOL_ARGS       = 256,
    MAX_SOURCE_PATHS    = 16,
    STATIC_TOOL_RESERVE = 12,
    MODULE_MAP_RESERVE  = 10,
    TOOL_ARG_RESERVE    = 15,
    DEFAULT_DIR_MODE    = 0755,
    REPORT_FILE_MODE    = 0644,
    EXEC_FAILURE        = 127,
    EXIT_FINDINGS       = 1,
    EXIT_TROUBLE        = 2,
    WAIT_STATUS_SHIFT   = 8
};

#define DEFAULT_DOCTOR_PREFIX "p101-doctor"
#define DEFAULT_SOURCE_PATH "."
#define DEFAULT_WRAPPER_AUDIT "p101-wrapper-audit"
#define DEFAULT_WRAPPER_ALLOW_FILE ".p101-wrapper-audit-allow"
#define DEFAULT_ERROR_CONTRACT "p101-error-contract"
#define DEFAULT_MODULE_MAP "p101-module-map"
#endif    // P101_DOCTOR_CONSTANTS_H
