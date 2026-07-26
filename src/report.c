#include "report.h"
#include "status.h"
#include <p101_c/p101_stdio.h>
#include <stdio.h>

void p101_doctor_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
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
    if(args->skip_wrapper_audit)
    {
        p101_fputs(env, err, "Wrapper audit: `skipped`\n\n", stream);
    }
    else
    {
        p101_fprintf(env, err, stream, "Source path: `%s`\n\n", args->source_path);
    }
    p101_fprintf(env, err, stream, "Fault-injection cases requested: `%u`\n\n", args->fault_count);

    p101_fputs(env, err, "## Results\n\n", stream);
    p101_fputs(env, err, "| Step | Status |\n", stream);
    p101_fputs(env, err, "| --- | --- |\n", stream);
    if(!args->skip_wrapper_audit)
    {
        p101_doctor_print_status_markdown(env, err, stream, "p101-wrapper-audit", result->wrapper_status);
    }
    p101_doctor_print_status_markdown(env, err, stream, "p101-module-map", result->module_status);
    p101_doctor_print_status_markdown(env, err, stream, "p101-observe", result->observe_status);
    p101_doctor_print_status_markdown(env, err, stream, "p101-error-path-walk", result->fault_walk_status);

    p101_fputs(env, err, "\n## Artifacts\n\n", stream);
    p101_fputs(env, err, "- Command: [command.txt](./command.txt)\n", stream);
    if(!args->skip_wrapper_audit)
    {
        p101_fputs(env, err, "- Wrapper audit stdout: [wrapper-audit.stdout.txt](./wrapper-audit.stdout.txt)\n", stream);
        p101_fputs(env, err, "- Wrapper audit stderr: [wrapper-audit.stderr.txt](./wrapper-audit.stderr.txt)\n", stream);
    }
    p101_fputs(env, err, "- Module map report: [module-map.md](./module-map.md)\n", stream);
    p101_fputs(env, err, "- Module map stdout: [module-map.stdout.txt](./module-map.stdout.txt)\n", stream);
    p101_fputs(env, err, "- Module map stderr: [module-map.stderr.txt](./module-map.stderr.txt)\n", stream);
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
    if(!args->skip_wrapper_audit)
    {
        p101_fputs(env, err, "`p101-wrapper-audit` is the static boundary story: it reports calls that bypass available p101 wrappers.\n\n", stream);
    }
    p101_fputs(env, err, "`p101-module-map` is the static design story: it reports module shape, public API surface, include relationships, and likely split/static-scope opportunities.\n\n", stream);
    p101_fputs(env, err, "`p101-observe` is the clean/ordinary execution story: resources, calls, trace tree, and correlated findings.\n\n", stream);
    p101_fputs(env, err, "`p101-error-path-walk` is the unhappy-path story: it fails p101 calls one at a time and checks whether those error paths leak or release invalid resources.\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_json_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
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
    if(args->skip_wrapper_audit)
    {
        p101_fputs(env, err, "  \"wrapper_audit\": false,\n", stream);
    }
    else
    {
        p101_fputs(env, err, "  \"wrapper_audit\": true,\n", stream);
    }
    p101_fprintf(env, err, stream, "  \"source_path\": \"%s\",\n", args->source_path);
    p101_fprintf(env, err, stream, "  \"fault_count\": %u,\n", args->fault_count);
    p101_fprintf(env, err, stream, "  \"doctor_dir\": \"%s\",\n", paths->dir);
    p101_fprintf(env, err, stream, "  \"module_report\": \"%s\",\n", paths->module_report);
    p101_fprintf(env, err, stream, "  \"observe_dir\": \"%s\",\n", paths->observe_dir);
    p101_fprintf(env, err, stream, "  \"fault_dir\": \"%s\",\n", paths->fault_dir);
    p101_fputs(env, err, "  \"statuses\": {\n", stream);
    if(!args->skip_wrapper_audit)
    {
        p101_doctor_print_status_json(env, err, stream, "p101_wrapper_audit", result->wrapper_status);
        p101_fputs(env, err, ",\n", stream);
    }
    p101_doctor_print_status_json(env, err, stream, "p101_module_map", result->module_status);
    p101_fputs(env, err, ",\n", stream);
    p101_doctor_print_status_json(env, err, stream, "p101_observe", result->observe_status);
    p101_fputs(env, err, ",\n", stream);
    p101_doctor_print_status_json(env, err, stream, "p101_error_path_walk", result->fault_walk_status);
    p101_fputs(env, err, "\n  }\n", stream);
    p101_fputs(env, err, "}\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
