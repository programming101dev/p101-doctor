#include "runner.h"
#include "constants.h"
#include "paths.h"
#include "report.h"
#include "status.h"
#include <fcntl.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_fcntl.h>
#include <p101_posix/p101_stdlib.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_wait.h>
#include <stdio.h>

static int  run_p101_wrapper_audit(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_p101_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_p101_observe(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_p101_error_path_walk(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths);
static int  run_tool_capture(const struct p101_env *env, struct p101_error *err, char *const tool_argv[], const char *stdout_path, const char *stderr_path);
static void redirect_child_output(const struct p101_env *env, struct p101_error *err, const char *stdout_path, const char *stderr_path);
static void clear_p101_observer_environment(const struct p101_env *env, struct p101_error *err);

int p101_doctor_run(const struct p101_env *env, struct p101_error *err, const struct arguments *args)
{
    struct doctor_paths  paths;
    struct doctor_result result;
    int                  ret_val;

    P101_TRACE(env);
    p101_memset(env, &paths, 0, sizeof(paths));
    p101_memset(env, &result, 0, sizeof(result));
    ret_val = EXIT_TROUBLE;

    p101_doctor_make_paths(env, err, args, &paths);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_doctor_create_dir(env, err, paths.dir);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_doctor_create_dir(env, err, paths.fault_dir);
    p101_doctor_write_command_file(env, err, paths.command, args->command_argv);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    if(!args->skip_wrapper_audit)
    {
        result.wrapper_status = run_p101_wrapper_audit(env, err, args, &paths);

        if(p101_error_has_error(err))
        {
            goto done;
        }
    }

    result.module_status = run_p101_module_map(env, err, args, &paths);

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

    p101_doctor_write_summary_file(env, err, args, &paths, &result);
    p101_doctor_write_json_file(env, err, args, &paths, &result);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    p101_printf(env, err, "p101-doctor: wrote doctor report to %s\n", paths.dir);

    if((!args->skip_wrapper_audit && !p101_doctor_status_is_acceptable(result.wrapper_status)) || !p101_doctor_status_is_acceptable(result.module_status) || !p101_doctor_status_is_acceptable(result.observe_status) ||
       !p101_doctor_status_is_acceptable(result.fault_walk_status))
    {
        ret_val = EXIT_TROUBLE;
        goto done;
    }

    if((!args->skip_wrapper_audit && p101_doctor_status_has_findings(result.wrapper_status)) || p101_doctor_status_has_findings(result.module_status) || p101_doctor_status_has_findings(result.observe_status) ||
       p101_doctor_status_has_findings(result.fault_walk_status))
    {
        ret_val = EXIT_FINDINGS;
        goto done;
    }

    ret_val = EXIT_SUCCESS;

done:
    return ret_val;
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

static int run_p101_module_map(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    char *tool_argv[MODULE_MAP_ARGS];
    char  module_path[PATH_LEN];
    char  output_path[PATH_LEN];
    char  source_path[PATH_LEN];
    char  output_option[] = "-o";
    int   ret_val;

    P101_TRACE(env);
    p101_strncpy(env, module_path, args->p101_module_map, sizeof(module_path) - 1U);
    module_path[sizeof(module_path) - 1U] = '\0';
    p101_strncpy(env, output_path, paths->module_report, sizeof(output_path) - 1U);
    output_path[sizeof(output_path) - 1U] = '\0';
    p101_strncpy(env, source_path, args->source_path, sizeof(source_path) - 1U);
    source_path[sizeof(source_path) - 1U] = '\0';

    tool_argv[0] = module_path;
    tool_argv[1] = output_option;
    tool_argv[2] = output_path;
    tool_argv[3] = source_path;
    tool_argv[4] = NULL;

    ret_val = run_tool_capture(env, err, tool_argv, paths->module_stdout, paths->module_stderr);
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
    char   observe_path[PATH_LEN];
    char   tracker_path[PATH_LEN];
    char   trace_path[PATH_LEN];
    char   report_path[PATH_LEN];
    char   fault_count[UINT_TEXT_LEN];
    char   count_option[]   = "-n";
    char   prefix_option[]  = "-l";
    char   observe_option[] = "-O";
    char   tracker_option[] = "-r";
    char   trace_option[]   = "-t";
    char   report_option[]  = "-p";
    char   separator[]      = "--";
    size_t index;

    P101_TRACE(env);
    p101_strncpy(env, walk_path, args->p101_error_path_walk, sizeof(walk_path) - 1U);
    walk_path[sizeof(walk_path) - 1U] = '\0';
    p101_strncpy(env, fault_prefix, paths->fault_prefix, sizeof(fault_prefix) - 1U);
    fault_prefix[sizeof(fault_prefix) - 1U] = '\0';
    p101_strncpy(env, observe_path, args->p101_observe, sizeof(observe_path) - 1U);
    observe_path[sizeof(observe_path) - 1U] = '\0';
    p101_strncpy(env, tracker_path, args->resource_tracker, sizeof(tracker_path) - 1U);
    tracker_path[sizeof(tracker_path) - 1U] = '\0';
    p101_strncpy(env, trace_path, args->p101_trace, sizeof(trace_path) - 1U);
    trace_path[sizeof(trace_path) - 1U] = '\0';
    p101_strncpy(env, report_path, args->p101_report, sizeof(report_path) - 1U);
    report_path[sizeof(report_path) - 1U] = '\0';
    p101_snprintf(env, err, fault_count, sizeof(fault_count), "%u", args->fault_count);

    index              = 0;
    tool_argv[index++] = walk_path;
    tool_argv[index++] = count_option;
    tool_argv[index++] = fault_count;
    tool_argv[index++] = prefix_option;
    tool_argv[index++] = fault_prefix;
    tool_argv[index++] = observe_option;
    tool_argv[index++] = observe_path;
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
            p101_exit_immediately(env, EXEC_FAILURE);
        }

        p101_execvp(env, err, tool_argv[0], tool_argv);
        p101_fprintf(env, err, stderr, "p101-doctor: exec failed for %s: %s\n", tool_argv[0], p101_error_get_message(err));
        p101_exit_immediately(env, EXEC_FAILURE);
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
