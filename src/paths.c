#include "paths.h"
#include "constants.h"
#include "errors.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_posix/p101_unistd.h>
#include <p101_posix/sys/p101_stat.h>
#include <stdio.h>

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name);

void p101_doctor_make_paths(const struct p101_env *env, struct p101_error *err, const struct arguments *args, struct doctor_paths *paths)
{
    P101_TRACE_SCOPE(env);

    if(args->doctor_dir == NULL)
    {
        p101_snprintf(env, err, paths->dir, sizeof(paths->dir), "%s-%ld", DEFAULT_DOCTOR_PREFIX, (long)p101_getpid(env));
    }
    else
    {
        p101_strncpy(env, paths->dir, args->doctor_dir, sizeof(paths->dir) - 1U);
        paths->dir[sizeof(paths->dir) - 1U] = '\0';
    }

    join_path(env, err, paths->command, paths->dir, "command.txt");
    join_path(env, err, paths->wrapper_stdout, paths->dir, "wrapper-audit.stdout.txt");
    join_path(env, err, paths->wrapper_stderr, paths->dir, "wrapper-audit.stderr.txt");
    join_path(env, err, paths->facts, paths->dir, "source-facts.tsv");
    join_path(env, err, paths->input_manifest, paths->dir, "source-inputs.json");
    join_path(env, err, paths->error_contract_stdout, paths->dir, "error-contract.stdout.txt");
    join_path(env, err, paths->error_contract_stderr, paths->dir, "error-contract.stderr.txt");
    join_path(env, err, paths->module_stdout, paths->dir, "module-map.stdout.txt");
    join_path(env, err, paths->module_stderr, paths->dir, "module-map.stderr.txt");
    join_path(env, err, paths->module_report, paths->dir, "module-map.md");
    join_path(env, err, paths->module_json, paths->dir, "module-map.json");
    join_path(env, err, paths->observe_dir, paths->dir, "observe");
    join_path(env, err, paths->observe_stdout, paths->dir, "observe.stdout.txt");
    join_path(env, err, paths->observe_stderr, paths->dir, "observe.stderr.txt");
    join_path(env, err, paths->fault_dir, paths->dir, "fault-walk");
    join_path(env, err, paths->fault_stdout, paths->dir, "error-path-walk.stdout.txt");
    join_path(env, err, paths->fault_stderr, paths->dir, "error-path-walk.stderr.txt");
    join_path(env, err, paths->summary, paths->dir, "summary.md");
    join_path(env, err, paths->json, paths->dir, "doctor.json");
    join_path(env, err, paths->manifest, paths->dir, "manifest.txt");
    join_path(env, err, paths->fault_prefix, paths->fault_dir, "case");
}

void p101_doctor_create_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE_SCOPE(env);
    p101_mkdir(env, err, path, DEFAULT_DIR_MODE);
}

void p101_doctor_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
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

void p101_doctor_write_manifest_file(const struct p101_env *env, struct p101_error *err, const char *path, const struct arguments *args, const struct doctor_paths *paths)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, path, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "p101-doctor manifest\n", stream);
    p101_fprintf(env, err, stream, "doctor_dir=%s\n", paths->dir);
    p101_fputs(env, err, "source_paths=", stream);
    for(int i = 0; i < args->source_count; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0) ? "" : " ", args->source_paths[i]);
    }
    p101_fputc(env, err, '\n', stream);
    p101_fprintf(env, err, stream, "fault_count=%u\n", args->fault_count);
    p101_fprintf(env, err, stream, "compile_database=%s\n", args->compile_db_path == NULL ? "auto" : args->compile_db_path);
    p101_fprintf(env, err, stream, "source_facts=%s\n", paths->facts);
    p101_fprintf(env, err, stream, "source_inputs=%s\n", paths->input_manifest);
    p101_fprintf(env, err, stream, "p101_wrapper_audit=%s\n", args->p101_wrapper_audit);
    p101_fprintf(env, err, stream, "p101_error_contract=%s\n", args->p101_error_contract);
    p101_fprintf(env, err, stream, "p101_module_map=%s\n", args->p101_module_map);
    p101_fprintf(env, err, stream, "p101_run=%s\n", args->p101_run);
    p101_fprintf(env, err, stream, "p101_observe=%s\n", args->p101_observe);
    p101_fprintf(env, err, stream, "p101_analyze=%s\n", args->p101_analyze);
    p101_fprintf(env, err, stream, "p101_event_model=%s\n", args->event_model);
    p101_fprintf(env, err, stream, "p101_error_path_walk=%s\n", args->p101_error_path_walk);
    p101_fprintf(env, err, stream, "p101_resource_tracker=%s\n", args->resource_tracker);
    p101_fprintf(env, err, stream, "p101_sync_check=%s\n", args->p101_sync_check);
    p101_fprintf(env, err, stream, "p101_trace=%s\n", args->p101_trace);
    p101_fprintf(env, err, stream, "p101_report=%s\n", args->p101_report);
    p101_fputs(env, err, "command=", stream);

    for(size_t i = 0; args->command_argv[i] != NULL; i++)
    {
        p101_fprintf(env, err, stream, "%s%s", (i == 0U) ? "" : " ", args->command_argv[i]);
    }

    p101_fputc(env, err, '\n', stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

static void join_path(const struct p101_env *env, struct p101_error *err, char destination[PATH_LEN], const char *dir, const char *name)
{
    int written;

    P101_TRACE_SCOPE(env);
    written = p101_snprintf(env, err, destination, PATH_LEN, "%s/%s", dir, name);

    if(written >= PATH_LEN)
    {
        P101_ERROR_RAISE_USER(err, "A doctor path is too long.", ERR_USAGE);
    }
}
