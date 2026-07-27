#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_unistd.h>
#include <stdlib.h>

void p101_doctor_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->source_path          = DEFAULT_SOURCE_PATH;
    args->fault_count_str      = DEFAULT_FAULT_COUNT;
    args->p101_wrapper_audit   = DEFAULT_WRAPPER_AUDIT;
    args->p101_module_map      = DEFAULT_MODULE_MAP;
    args->p101_observe         = DEFAULT_OBSERVE_PATH;
    args->p101_error_path_walk = DEFAULT_ERROR_WALK_PATH;
    args->resource_tracker     = DEFAULT_TRACKER_PATH;
    args->p101_trace           = DEFAULT_TRACE_PATH;
    args->p101_report          = DEFAULT_REPORT_PATH;
}

void p101_doctor_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        p101_doctor_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
    }

    while((opt = p101_getopt(env, argc, argv, ":hvxo:s:n:A:M:O:W:r:t:p:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                p101_doctor_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
                break;
            }
            case 'x':
            {
                args->skip_wrapper_audit = true;
                break;
            }
            case 'o':
            {
                args->doctor_dir = optarg;
                break;
            }
            case 's':
            {
                args->source_path = optarg;
                break;
            }
            case 'n':
            {
                args->fault_count_str = optarg;
                break;
            }
            case 'A':
            {
                args->p101_wrapper_audit = optarg;
                break;
            }
            case 'M':
            {
                args->p101_module_map = optarg;
                break;
            }
            case 'O':
            {
                args->p101_observe = optarg;
                break;
            }
            case 'W':
            {
                args->p101_error_path_walk = optarg;
                break;
            }
            case 'r':
            {
                args->resource_tracker = optarg;
                break;
            }
            case 't':
            {
                args->p101_trace = optarg;
                break;
            }
            case 'p':
            {
                args->p101_report = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt ? optopt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            case '?':
            {
                char msg[MSG_LEN];

                if(p101_isprint(env, optopt))
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option '-%c'.", optopt);
                }
                else
                {
                    p101_snprintf(env, err, msg, sizeof(msg), "Unknown option character 0x%02X.", (unsigned)(unsigned char)optopt);
                }

                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
            default:
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Internal error: unhandled option '-%c' returned by getopt.", p101_isprint(env, opt) ? opt : '?');
                P101_ERROR_RAISE_USER(err, msg, ERR_USAGE);
                break;
            }
        }
    }

    if(p101_error_has_no_error(err))
    {
        args->command_argv = &argv[optind];
        args->command_argc = argc - optind;
    }
}

void p101_doctor_check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    P101_TRACE(env);

    if(args->command_argv == NULL || args->command_argv[0] == NULL)
    {
        P101_ERROR_RAISE_USER(err, "A command is required.", ERR_USAGE);
        goto done;
    }

    if(args->command_argc > MAX_TOOL_ARGS - TOOL_ARG_RESERVE)
    {
        P101_ERROR_RAISE_USER(err, "The command has too many arguments for p101-doctor.", ERR_USAGE);
        goto done;
    }

    if(args->doctor_dir != NULL && args->doctor_dir[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The doctor directory must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->source_path == NULL || args->source_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The source path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->fault_count_str == NULL || args->fault_count_str[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The fault count must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->skip_wrapper_audit && (args->p101_wrapper_audit == NULL || args->p101_wrapper_audit[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-wrapper-audit path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_module_map == NULL || args->p101_module_map[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-module-map path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_observe == NULL || args->p101_observe[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-observe path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_error_path_walk == NULL || args->p101_error_path_walk[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-error-path-walk path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->resource_tracker == NULL || args->resource_tracker[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-resource-tracker path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_trace == NULL || args->p101_trace[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-trace path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_report == NULL || args->p101_report[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-report path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void p101_doctor_convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE(env);
    args->fault_count = p101_parse_unsigned_int(env, err, args->fault_count_str, 0U);

    if(p101_error_has_error(err))
    {
        P101_ERROR_RAISE_USER(err, "The fault count must be an unsigned integer.", ERR_USAGE);
    }
}

_Noreturn void p101_doctor_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(
        env,
        err,
        stderr,
        "Usage: %s [-h] [-v] [-x] [-o <doctor-dir>] [-s <source-path>] [-n <count>] [-A <p101-wrapper-audit>] [-M <p101-module-map>] [-O <p101-observe>] [-W <p101-error-path-walk>] [-r <p101-resource-tracker>] [-t <p101-trace>] [-p <p101-report>] -- <command> [args...]\n",
        program_name);
    p101_fputs(env, err, "\n", stderr);
    p101_fputs(env, err, "Run a p101 program through wrapper, observation, and fault-injected error-path checks.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                      Show this help\n", stderr);
    p101_fputs(env, err, "  -v                      Enable p101 tracing inside p101-doctor\n", stderr);
    p101_fputs(env, err, "  -x                      Skip static wrapper audit; still run module, observe, and error-path checks\n", stderr);
    p101_fputs(env, err, "  -o <doctor-dir>         Output directory; default is p101-doctor-<pid>\n", stderr);
    p101_fputs(env, err, "  -s <source-path>        Source path for p101-wrapper-audit; default is .\n", stderr);
    p101_fputs(env, err, "  -n <count>              Fault cases for p101-error-path-walk; default is 16\n", stderr);
    p101_fputs(env, err, "  -A <p101-wrapper-audit> p101-wrapper-audit executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -M <p101-module-map>    p101-module-map executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -O <p101-observe>       p101-observe executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -W <p101-error-path-walk> p101-error-path-walk executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -r <p101-resource-tracker> p101-resource-tracker executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -t <p101-trace>         p101-trace executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -p <p101-report>        p101-report executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o doctor -- ./my-program config.txt\n", program_name);
    p101_exit(env, exit_code);
}
