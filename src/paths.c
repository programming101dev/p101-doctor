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
    P101_TRACE(env);

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
    join_path(env, err, paths->module_stdout, paths->dir, "module-map.stdout.txt");
    join_path(env, err, paths->module_stderr, paths->dir, "module-map.stderr.txt");
    join_path(env, err, paths->module_report, paths->dir, "module-map.md");
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

void p101_doctor_create_dir(const struct p101_env *env, struct p101_error *err, const char *path)
{
    P101_TRACE(env);
    p101_mkdir(env, err, path, DEFAULT_DIR_MODE);
}

void p101_doctor_write_command_file(const struct p101_env *env, struct p101_error *err, const char *path, char *const *const command_argv)
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
