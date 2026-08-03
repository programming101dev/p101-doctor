#include "cli.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_cli/cli.h>
#include <stdlib.h>

void p101_doctor_arguments_init(const struct p101_env *env, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    p101_memset(env, args, 0, sizeof(*args));
    args->source_paths[0]     = DEFAULT_SOURCE_PATH;
    args->source_count        = 1;
    args->p101_wrapper_audit  = DEFAULT_WRAPPER_AUDIT;
    args->p101_error_contract = DEFAULT_ERROR_CONTRACT;
    args->p101_module_map     = DEFAULT_MODULE_MAP;
}

void p101_doctor_parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;
#ifdef P101_DOCTOR_TESTING
    const char *forced_option;
#endif

    P101_TRACE_SCOPE(env);
    opterr = 0;
#ifdef P101_DOCTOR_TESTING
    forced_option = getenv("P101_DOCTOR_TEST_OPTION");
#endif

    if(argc == 2 && p101_strcmp(env, argv[1], "--help") == 0)
    {
        p101_doctor_usage(env, err, argv[0], EXIT_SUCCESS, NULL);
    }

    while(
#ifdef P101_DOCTOR_TESTING
        (opt = (forced_option == NULL) ? p101_getopt(env, argc, argv, ":hvxo:s:C:A:E:M:") : (unsigned char)*forced_option) != -1 &&
#else
        (opt = p101_getopt(env, argc, argv, ":hvxo:s:C:A:E:M:")) != -1 &&
#endif
        p101_error_has_no_error(err))
    {
#ifdef P101_DOCTOR_TESTING
        forced_option = NULL;
#endif
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
                args->skip_source_contracts = true;
                break;
            }
            case 'o':
            {
                args->doctor_dir = optarg;
                break;
            }
            case 's':
            {
                if(!args->source_paths_set)
                {
                    args->source_count     = 0;
                    args->source_paths_set = true;
                }

                if(args->source_count >= MAX_SOURCE_PATHS)
                {
                    P101_ERROR_RAISE_USER(err, "Too many source paths were provided.", ERR_USAGE);
                    break;
                }

                args->source_paths[args->source_count] = optarg;
                args->source_count++;
                break;
            }
            case 'C':
            {
                args->compile_db_path = optarg;
                break;
            }
            case 'A':
            {
                args->p101_wrapper_audit = optarg;
                break;
            }
            case 'E':
            {
                args->p101_error_contract = optarg;
                break;
            }
            case 'M':
            {
                args->p101_module_map = optarg;
                break;
            }
            case ':':
            {
                char msg[MSG_LEN];

                p101_snprintf(env, err, msg, sizeof(msg), "Option '-%c' requires an argument.", optopt);
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
    P101_TRACE_SCOPE(env);

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

    if(args->source_count <= 0)
    {
        P101_ERROR_RAISE_USER(err, "At least one source path is required.", ERR_USAGE);
        goto done;
    }

    for(int i = 0; i < args->source_count; i++)
    {
        if(args->source_paths[i] == NULL || args->source_paths[i][0] == '\0')
        {
            P101_ERROR_RAISE_USER(err, "Source paths must not be empty.", ERR_USAGE);
            goto done;
        }
    }

    if(args->compile_db_path != NULL && args->compile_db_path[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The compile database path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->skip_source_contracts && (args->p101_wrapper_audit == NULL || args->p101_wrapper_audit[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-wrapper-audit path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(!args->skip_source_contracts && (args->p101_error_contract == NULL || args->p101_error_contract[0] == '\0'))
    {
        P101_ERROR_RAISE_USER(err, "The p101-error-contract path must not be empty.", ERR_USAGE);
        goto done;
    }

    if(args->p101_module_map == NULL || args->p101_module_map[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-module-map path must not be empty.", ERR_USAGE);
        goto done;
    }

done:
    return;
}

void p101_doctor_convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE_SCOPE(env);
    (void)err;
    (void)args;
}

_Noreturn void p101_doctor_usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env, err, stderr, "Usage: %s [-h] [-v] [-x] [-o <doctor-dir>] [-s <source-path>]... [-C <compile_commands.json>] [-A <p101-wrapper-audit>] [-E <p101-error-contract>] [-M <p101-module-map>] -- <command> [args...]\n", program_name);
    p101_fputs(env, err, "\n", stderr);
    p101_fputs(env, err, "Run source-contract and module-boundary checks for a p101 project.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                      Show this help\n", stderr);
    p101_fputs(env, err, "  -v                      Enable p101 tracing inside p101-doctor\n", stderr);
    p101_fputs(env, err, "  -x                      Skip wrapper and error-contract checks; still run the module check\n", stderr);
    p101_fputs(env, err, "  -o <doctor-dir>         Output directory; default is p101-doctor-<pid>\n", stderr);
    p101_fputs(env, err, "  -s <source-path>        Source/header path to scan; repeatable; default is .\n", stderr);
    p101_fputs(env, err, "  -C <compile_commands.json> Use one explicit compile database for every source tool\n", stderr);
    p101_fputs(env, err, "  -A <p101-wrapper-audit> p101-wrapper-audit executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -E <p101-error-contract> p101-error-contract executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -M <p101-module-map>    p101-module-map executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o doctor -s src -s include -- ./my-program\n", program_name);
    p101_exit(env, exit_code);
}
