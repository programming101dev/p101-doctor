#include "arguments.h"
#include "errors.h"
#include <fcntl.h>
#include <limits.h>
#include <p101_c/p101_ctype.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_convert/integer.h>
#include <p101_posix/p101_fcntl.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

struct doctor_paths
{
    char dir[PATH_LEN];
    char command[PATH_LEN];
    char wrapper_stdout[PATH_LEN];
    char wrapper_stderr[PATH_LEN];
    char observe_dir[PATH_LEN];
    char observe_stdout[PATH_LEN];
    char observe_stderr[PATH_LEN];
    char fault_dir[PATH_LEN];
    char fault_prefix[PATH_LEN];
    char fault_stdout[PATH_LEN];
    char fault_stderr[PATH_LEN];
    char summary[PATH_LEN];
    char json[PATH_LEN];
};

struct doctor_result
{
    int wrapper_status;
    int observe_status;
    int fault_walk_status;
};

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args);
static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args);
static void convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args);
static int  run_doctor(const struct p101_env *env, struct p101_error *err, const struct arguments *args);

static void make_doctor_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct doctor_paths *paths);
static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);
static void create_dir(const struct p101_env *env, struct p101_error *err, const char *path);
static void write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *command_argv);
static void write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result);
static void write_json_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result);

static int  run_p101_wrapper_audit(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_p101_error_path_walk(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);
static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path);
static void clear_p101_observer_environment(const struct p101_env *env, struct p101_error *err);

static bool           tool_status_is_clean(int status);
static bool           tool_status_has_findings(int status);
static bool           tool_status_is_acceptable(int status);
static void           print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static void           print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static const char    *status_word(int status);
_Noreturn static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message);

static const char DEFAULT_AUDIT_PREFIX[]    = "p101-doctor";
static const char DEFAULT_SOURCE_PATH[]     = ".";
static const char DEFAULT_WRAPPER_AUDIT[]   = "p101-wrapper-audit";
static const char DEFAULT_OBSERVE_PATH[]    = "p101-observe";
static const char DEFAULT_ERROR_WALK_PATH[] = "p101-error-path-walk";
static const char DEFAULT_TRACKER_PATH[]    = "p101-resource-tracker";
static const char DEFAULT_TRACE_PATH[]      = "p101-trace";
static const char DEFAULT_REPORT_PATH[]     = "p101-report";
static const char DEFAULT_FAULT_COUNT[]     = "16";
static const char RESOURCE_LOG_ENV[]        = "P101_RESOURCE_LOG";
static const char CALL_LOG_ENV[]            = "P101_CALL_LOG";
static const char CALL_LOG_ARGS_ENV[]       = "P101_CALL_LOG_ARGS";
static const char CALL_LOG_RESULT_ENV[]     = "P101_CALL_LOG_RESULT";
static const char FAULT_CALL_ENV[]          = "P101_FAULT_CALL";
static const char FAULT_ERRNO_ENV[]         = "P101_FAULT_ERRNO";
static const char FAULT_NAME_ENV[]          = "P101_FAULT_NAME";
static const char FAULT_LOG_ENV[]           = "P101_FAULT_LOG";

enum
{
    MSG_LEN          = 256,
    MAX_TOOL_ARGS    = 256,
    TOOL_ARG_RESERVE = 12,
    UINT_TEXT_LEN    = 32,
    DEFAULT_DIR_MODE = 0755,
    REPORT_FILE_MODE = 0644,
    EXEC_FAILURE     = 127,
    EXIT_FINDINGS    = 1,
    EXIT_TROUBLE     = 2
};

int main(int argc, char *argv[])
{
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;
    int                ret_val;

    ret_val = EXIT_TROUBLE;
    err     = p101_error_create(false);
    env     = p101_env_create(err, NULL);
    p101_memset(env, &args, 0, sizeof(args));

    args.source_path          = DEFAULT_SOURCE_PATH;
    args.fault_count_str      = DEFAULT_FAULT_COUNT;
    args.p101_wrapper_audit   = DEFAULT_WRAPPER_AUDIT;
    args.p101_observe         = DEFAULT_OBSERVE_PATH;
    args.p101_error_path_walk = DEFAULT_ERROR_WALK_PATH;
    args.resource_tracker     = DEFAULT_TRACKER_PATH;
    args.p101_trace           = DEFAULT_TRACE_PATH;
    args.p101_report          = DEFAULT_REPORT_PATH;

    parse_arguments(env, err, argc, argv, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(args.verbose)
    {
        p101_env_set_tracer(env, p101_env_default_tracer);
    }

    check_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    convert_arguments(env, err, &args);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    ret_val = run_doctor(env, err, &args);

done:
    if(p101_error_has_error(err))
    {
        if(p101_error_is_error(err, P101_ERROR_USER, ERR_USAGE))
        {
            const char *msg;

            msg = p101_error_get_message(err);
            usage(env, err, argv[0], EXIT_TROUBLE, msg);
        }

        p101_fprintf(env, err, stderr, "%s\n", p101_error_get_message(err));
        ret_val = EXIT_TROUBLE;
    }

    p101_env_destroy(env);
    p101_error_destroy(err);

    return ret_val;
}

static void parse_arguments(const struct p101_env *env, struct p101_error *err, int argc, char *argv[], struct arguments *args)
{
    int opt;

    P101_TRACE(env);
    opterr = 0;

    while((opt = p101_getopt(env, argc, argv, ":hvo:s:n:A:O:W:r:t:p:")) != -1 && p101_error_has_no_error(err))
    {
        switch(opt)
        {
            case 'h':
            {
                usage(env, err, argv[0], EXIT_SUCCESS, NULL);
            }
            case 'v':
            {
                args->verbose = true;
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

static void check_arguments(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
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

    if(args->p101_wrapper_audit == NULL || args->p101_wrapper_audit[0] == '\0')
    {
        P101_ERROR_RAISE_USER(err, "The p101-wrapper-audit path must not be empty.", ERR_USAGE);
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

static void convert_arguments(const struct p101_env *env, struct p101_error *err, struct arguments *args)
{
    P101_TRACE(env);
    args->fault_count = p101_parse_unsigned_int(env, err, args->fault_count_str, 0U);

    if(p101_error_has_error(err))
    {
        P101_ERROR_RAISE_USER(err, "The fault count must be an unsigned integer.", ERR_USAGE);
    }
}

static int run_doctor(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct doctor_paths  paths;
    struct doctor_result result;
    int                  ret_val;

    P101_TRACE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    make_doctor_paths(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    create_dir(env, err, paths.dir);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    create_dir(env, err, paths.fault_dir);
    write_command_file(env, err, paths.command, args->command_argv);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result.wrapper_status = run_p101_wrapper_audit(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result.observe_status = run_p101_observe(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    result.fault_walk_status = run_p101_error_path_walk(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    write_summary_file(env, err, args, &paths, &result);
    write_json_file(env, err, args, &paths, &result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_printf(env, err, "p101-doctor: wrote doctor report to %s\n", paths.dir);

    if(!tool_status_is_acceptable(result.wrapper_status) || !tool_status_is_acceptable(result.observe_status) || !tool_status_is_acceptable(result.fault_walk_status))
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if(tool_status_has_findings(result.wrapper_status) || tool_status_has_findings(result.observe_status) || tool_status_has_findings(result.fault_walk_status))
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
}

static void make_doctor_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct doctor_paths *paths)
{
    P101_TRACE(env);

    if(args->doctor_dir == NULL)
    {
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_AUDIT_PREFIX, (long)p101_getpid(env));
    }
    else
    {
        p101_strncpy(env, paths->dir, args->doctor_dir, sizeof(paths->dir) - 1U);
        paths->dir[sizeof(paths->dir) - 1U] = '\0';
    }

    join_path(env, err, paths->command, paths->dir, "command.txt");
    join_path(env, err, paths->wrapper_stdout, paths->dir, "wrapper-audit.stdout.txt");
    join_path(env, err, paths->wrapper_stderr, paths->dir, "wrapper-audit.stderr.txt");
    join_path(env, err, paths->observe_dir, paths->dir, "observe");
    join_path(env, err, paths->observe_stdout, paths->dir, "observe.stdout.txt");
    join_path(env, err, paths->observe_stderr, paths->dir, "observe.stderr.txt");
    join_path(env, err, paths->fault_dir, paths->dir, "fault-walk");
    join_path(env, err, paths->fault_stdout, paths->dir, "error-path-walk.stdout.txt");
    join_path(env, err, paths->fault_stderr, paths->dir, "error-path-walk.stderr.txt");
    join_path(env, err, paths->summary, paths->dir, "summary.md");
    join_path(env, err, paths->json, paths->dir, "doctor.json");
    join_path(env, err, paths->fault_prefix, paths->fault_dir, "case");
}

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written < 0 || written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A doctor path is too long.", ERR_USAGE);
    }
}

static void create_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE(env);
    p101_mkdir(env, err, path, DEFAULT_DIR_MODE);
}

static void write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    for(size_t i = 0; command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, paths->summary, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "# p101 doctor\n\n", stream);
    p101_fprintf(env, err, stream, "Command: `%s`\n\n", args->command_argv[0]);
    p101_fprintf(env, err, stream, "Source path: `%s`\n\n", args->source_path);
    p101_fprintf(env, err, stream, "Fault-injection cases requested: `%u`\n\n", args->fault_count);

    p101_fputs(env, err, "## Results\n\n", stream);
    p101_fputs(env, err, "| Step | Status |\n", stream);
    p101_fputs(env, err, "| --- | --- |\n", stream);
    print_status_markdown(env, err, stream, "p101-wrapper-audit", result->wrapper_status);
    print_status_markdown(env, err, stream, "p101-observe", result->observe_status);
    print_status_markdown(env, err, stream, "p101-error-path-walk", result->fault_walk_status);

    p101_fputs(env, err, "\n## Artifacts\n\n", stream);
    p101_fputs(env, err, "- Command: [command.txt](./command.txt)\n", stream);
    p101_fputs(env, err, "- Wrapper audit stdout: [wrapper-audit.stdout.txt](./wrapper-audit.stdout.txt)\n", stream);
    p101_fputs(env, err, "- Wrapper audit stderr: [wrapper-audit.stderr.txt](./wrapper-audit.stderr.txt)\n", stream);
    p101_fputs(env, err, "- Observe stdout: [observe.stdout.txt](./observe.stdout.txt)\n", stream);
    p101_fputs(env, err, "- Observe stderr: [observe.stderr.txt](./observe.stderr.txt)\n", stream);
    p101_fputs(env, err, "- Observed run directory: [observe](./observe/)\n", stream);
    p101_fputs(env, err, "- Observed run summary: [observe/summary.txt](./observe/summary.txt)\n", stream);
    p101_fputs(env, err, "- Correlated observed report: [observe/correlated-report.txt](./observe/correlated-report.txt)\n", stream);
    p101_fputs(env, err, "- Correlated observed JSON: [observe/correlated-report.json](./observe/correlated-report.json)\n", stream);
    p101_fputs(env, err, "- Fault-walk stdout: [error-path-walk.stdout.txt](./error-path-walk.stdout.txt)\n", stream);
    p101_fputs(env, err, "- Fault-walk stderr: [error-path-walk.stderr.txt](./error-path-walk.stderr.txt)\n", stream);
    p101_fputs(env, err, "- Fault-walk per-case logs: [fault-walk](./fault-walk/)\n", stream);
    p101_fputs(env, err, "- Machine-readable doctor index: [doctor.json](./doctor.json)\n", stream);

    p101_fputs(env, err, "\n## How to read this\n\n", stream);
    p101_fputs(env, err, "`p101-wrapper-audit` is the static boundary story: it reports calls that bypass available p101 wrappers.\n\n", stream);
    p101_fputs(env, err, "`p101-observe` is the clean/ordinary execution story: resources, calls, trace tree, and correlated findings.\n\n", stream);
    p101_fputs(env, err, "`p101-error-path-walk` is the unhappy-path story: it fails p101 calls one at a time and checks whether those error paths leak or release invalid resources.\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void write_json_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    FILE *stream;

    P101_TRACE(env);
    stream = p101_fopen(env, err, paths->json, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "{\n", stream);
    p101_fprintf(env, err, stream, "  \"command\": \"%s\",\n", args->command_argv[0]);
    p101_fprintf(env, err, stream, "  \"source_path\": \"%s\",\n", args->source_path);
    p101_fprintf(env, err, stream, "  \"fault_count\": %u,\n", args->fault_count);
    p101_fprintf(env, err, stream, "  \"doctor_dir\": \"%s\",\n", paths->dir);
    p101_fprintf(env, err, stream, "  \"observe_dir\": \"%s\",\n", paths->observe_dir);
    p101_fprintf(env, err, stream, "  \"fault_dir\": \"%s\",\n", paths->fault_dir);
    p101_fputs(env, err, "  \"statuses\": {\n", stream);
    print_status_json(env, err, stream, "p101_wrapper_audit", result->wrapper_status);
    p101_fputs(env, err, ",\n", stream);
    print_status_json(env, err, stream, "p101_observe", result->observe_status);
    p101_fputs(env, err, ",\n", stream);
    print_status_json(env, err, stream, "p101_error_path_walk", result->fault_walk_status);
    p101_fputs(env, err, "\n  }\n", stream);
    p101_fputs(env, err, "}\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static int run_p101_wrapper_audit(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    char *tool_argv[4];
    char  audit_path[PATH_LEN];
    char  source_path[PATH_LEN];
    int   ret_val;

    P101_TRACE(env);
    p101_strncpy(env, audit_path, args->p101_wrapper_audit, sizeof(audit_path) - 1U);
    audit_path[sizeof(audit_path) - 1U] = '\0';
    p101_strncpy(env, source_path, args->source_path, sizeof(source_path) - 1U);
    source_path[sizeof(source_path) - 1U] = '\0';

    tool_argv[0] = audit_path;
    tool_argv[1] = source_path;
    tool_argv[2] = NULL;

    ret_val = run_tool_capture(env, err, tool_argv, paths->wrapper_stdout, paths->wrapper_stderr);
    return ret_val;
}

static int run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    char  *tool_argv[MAX_TOOL_ARGS];
    char   observe_path[PATH_LEN];
    char   observe_dir[PATH_LEN];
    char   tracker_path[PATH_LEN];
    char   trace_path[PATH_LEN];
    char   report_path[PATH_LEN];
    char   output_option[]  = "-o";
    char   tracker_option[] = "-r";
    char   trace_option[]   = "-t";
    char   report_option[]  = "-p";
    char   separator[]      = "--";
    size_t index;

    P101_TRACE(env);
    p101_strncpy(env, observe_path, args->p101_observe, sizeof(observe_path) - 1U);
    observe_path[sizeof(observe_path) - 1U] = '\0';
    p101_strncpy(env, observe_dir, paths->observe_dir, sizeof(observe_dir) - 1U);
    observe_dir[sizeof(observe_dir) - 1U] = '\0';
    p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
    tracker_path[sizeof(tracker_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';

    index              = 0;
    tool_argv[index++] = observe_path;
    tool_argv[index++] = output_option;
    tool_argv[index++] = observe_dir;
    tool_argv[index++] = tracker_option;
    tool_argv[index++] = tracker_path;
    tool_argv[index++] = trace_option;
    tool_argv[index++] = trace_path;
    tool_argv[index++] = report_option;
    tool_argv[index++] = report_path;
    tool_argv[index++] = separator;

    for(int i = 0; i < args->command_argc; i++)
    {
        tool_argv[index++] = args->command_argv[i];
    }
    tool_argv[index] = NULL;

    return run_tool_capture(env, err, tool_argv, paths->observe_stdout, paths->observe_stderr);
}

static int run_p101_error_path_walk(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    char  *tool_argv[MAX_TOOL_ARGS];
    char   walk_path[PATH_LEN];
    char   fault_prefix[PATH_LEN];
    char   tracker_path[PATH_LEN];
    char   report_path[PATH_LEN];
    char   fault_count[UINT_TEXT_LEN];
    char   count_option[]   = "-n";
    char   prefix_option[]  = "-l";
    char   tracker_option[] = "-r";
    char   report_option[]  = "-p";
    char   separator[]      = "--";
    size_t index;

    P101_TRACE(env);
    p101_strncpy(env, walk_path, args->p101_error_path_walk, sizeof(walk_path) - 1U);
    walk_path[sizeof(walk_path) - 1U] = '\0';
    p101_strncpy(env, fault_prefix, paths->fault_prefix, sizeof(fault_prefix) - 1U);
    fault_prefix[sizeof(fault_prefix) - 1U] = '\0';
    p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
    tracker_path[sizeof(tracker_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';
    p101_snprintf(env, err, fault_count, sizeof(fault_count), "%u", args->fault_count);

    index              = 0;
    tool_argv[index++] = walk_path;
    tool_argv[index++] = count_option;
    tool_argv[index++] = fault_count;
    tool_argv[index++] = prefix_option;
    tool_argv[index++] = fault_prefix;
    tool_argv[index++] = tracker_option;
    tool_argv[index++] = tracker_path;
    tool_argv[index++] = report_option;
    tool_argv[index++] = report_path;
    tool_argv[index++] = separator;

    for(int i = 0; i < args->command_argc; i++)
    {
        tool_argv[index++] = args->command_argv[i];
    }
    tool_argv[index] = NULL;

    return run_tool_capture(env, err, tool_argv, paths->fault_stdout, paths->fault_stderr);
}

static int run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path)
{
    int   status;
    pid_t pid;

    P101_TRACE(env);
    status = 0;
    pid    = p101_fork(env, err);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(pid == 0)
    {
        clear_p101_observer_environment(env, err);
        redirect_child_output(env, err, stdout_path, stderr_path);

        if(p101_error_has_error(err))
        {
            p101_fprintf(env, err, stderr, "p101-doctor: tool setup failed: %s\n", p101_error_get_message(err));
            p101__exit(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-doctor: exec failed for %s: %s\n", tool_argv[0], p101_error_get_message(err));
        p101__exit(env, EXEC_FAILURE);
    }

    p101_waitpid(env, err, pid, &status, 0);

done:
    return status;
}

static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path)
{
    int stdout_fd;
    int stderr_fd;

    P101_TRACE(env);
    stdout_fd = p101_open(env, err, stdout_path, O_WRONLY | O_CREAT | O_TRUNC, REPORT_FILE_MODE);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    stderr_fd = p101_open(env, err, stderr_path, O_WRONLY | O_CREAT | O_TRUNC, REPORT_FILE_MODE);

    if(p101_error_has_error(err))
    {
        p101_close(env, err, stdout_fd);
        goto done;
    }

    p101_dup2(env, err, stdout_fd, STDOUT_FILENO);
    p101_dup2(env, err, stderr_fd, STDERR_FILENO);
    p101_close(env, err, stdout_fd);
    p101_close(env, err, stderr_fd);

done:
    return;
}

static void clear_p101_observer_environment(const struct p101_env *env, struct p101_error *err)
{
    P101_TRACE(env);
    p101_unsetenv(env, err, RESOURCE_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ENV);
    p101_unsetenv(env, err, CALL_LOG_ARGS_ENV);
    p101_unsetenv(env, err, CALL_LOG_RESULT_ENV);
    p101_unsetenv(env, err, FAULT_CALL_ENV);
    p101_unsetenv(env, err, FAULT_ERRNO_ENV);
    p101_unsetenv(env, err, FAULT_NAME_ENV);
    p101_unsetenv(env, err, FAULT_LOG_ENV);
}

static bool tool_status_is_clean(int status)
{
    return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS;
}

static bool tool_status_has_findings(int status)
{
    return WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FINDINGS;
}

static bool tool_status_is_acceptable(int status)
{
    return (tool_status_is_clean(status) || tool_status_has_findings(status)) != 0;
}

static void print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "| %s | %s (exit %d) |\n", label, status_word(status), WEXITSTATUS(status));
    }
    else if(WIFSIGNALED(status))
    {
        p101_fprintf(env, err, stream, "| %s | trouble (signal %d) |\n", label, WTERMSIG(status));
    }
    else
    {
        p101_fprintf(env, err, stream, "| %s | trouble (status %d) |\n", label, status);
    }
}

static void print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"exit\", \"code\": %d, \"result\": \"%s\"}", label, WEXITSTATUS(status), status_word(status));
    }
    else if(WIFSIGNALED(status))
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"signal\", \"signal\": %d, \"result\": \"trouble\"}", label, WTERMSIG(status));
    }
    else
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"status\", \"status\": %d, \"result\": \"trouble\"}", label, status);
    }
}

static const char *status_word(int status)
{
    const char *word;

    word = "trouble";

    if(tool_status_is_clean(status))
    {
        word = "clean";
    }
    else if(tool_status_has_findings(status))
    {
        word = "findings";
    }

    return word;
}

static void usage(const struct p101_env *env, struct p101_error *err, const char *program_name, int exit_code, const char *message)
{
    if(message != NULL)
    {
        p101_fprintf(env, err, stderr, "%s\n\n", message);
    }

    p101_fprintf(env,
                 err,
                 stderr,
                 "Usage: %s [-h] [-v] [-o <doctor-dir>] [-s <source-path>] [-n <count>] [-A <p101-wrapper-audit>] [-O <p101-observe>] [-W <p101-error-path-walk>] [-r <p101-resource-tracker>] [-t <p101-trace>] [-p <p101-report>] -- <command> [args...]\n",
                 program_name);
    p101_fputs(env, err, "\n", stderr);
    p101_fputs(env, err, "Run a p101 program through wrapper, observation, and fault-injected error-path checks.\n", stderr);
    p101_fputs(env, err, "\nOptions:\n", stderr);
    p101_fputs(env, err, "  -h                      Show this help\n", stderr);
    p101_fputs(env, err, "  -v                      Enable p101 tracing inside p101-doctor\n", stderr);
    p101_fputs(env, err, "  -o <doctor-dir>         Output directory; default is p101-doctor-<pid>\n", stderr);
    p101_fputs(env, err, "  -s <source-path>        Source path for p101-wrapper-audit; default is .\n", stderr);
    p101_fputs(env, err, "  -n <count>              Fault cases for p101-error-path-walk; default is 16\n", stderr);
    p101_fputs(env, err, "  -A <p101-wrapper-audit> p101-wrapper-audit executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -O <p101-observe>       p101-observe executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -W <p101-error-path-walk> p101-error-path-walk executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -r <p101-resource-tracker> p101-resource-tracker executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -t <p101-trace>         p101-trace executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "  -p <p101-report>        p101-report executable; default resolves through PATH\n", stderr);
    p101_fputs(env, err, "\nExample:\n", stderr);
    p101_fprintf(env, err, stderr, "  %s -o doctor -- ./my-program config.txt\n", program_name);
    p101_exit(env, exit_code);
}
