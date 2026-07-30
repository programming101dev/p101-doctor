#include "report.h"
#include "constants.h"
#include "status.h"
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <stdbool.h>
#include <stdio.h>

enum
{
    JSON_CONTROL_BYTE_LIMIT  = 0x20U,
    JSON_NON_ASCII_BYTE_BASE = 0x80U
};

static void        write_grade_line(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static const char *grade_for_status(int status);
static void        write_observe_detail(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct doctor_paths *paths);
static bool        read_observe_resource_line(const struct p101_env *env, struct p101_error *err, const struct doctor_paths *paths, char line[MSG_LEN]);
static void        trim_newline(const struct p101_env *env, char *line);
static void        write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);

static const char *grade_for_status(int status)
{
    const char *grade;

    grade = "trouble";

    if(p101_doctor_status_is_clean(status))
    {
        grade = "good";
    }
    else if(p101_doctor_status_has_findings(status))
    {
        grade = "needs work";
    }

    return grade;
}

static void write_grade_line(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    p101_fprintf(env, err, stream, "- %s: `%s` (%s)\n", label, grade_for_status(status), p101_doctor_status_word(status));
}

static void write_observe_detail(const struct p101_env *env, struct p101_error *err, FILE *stream, const struct doctor_paths *paths)
{
    char line[MSG_LEN];

    if(read_observe_resource_line(env, err, paths, line))
    {
        p101_fprintf(env, err, stream, "- Observed resource counts: `%s`\n", line);
    }
    else
    {
        p101_fputs(env, err, "- Observed resource counts: unavailable; see observe artifacts.\n", stream);
    }
}

static bool read_observe_resource_line(const struct p101_env *env, struct p101_error *err, const struct doctor_paths *paths, char line[MSG_LEN])
{
    FILE *stream;
    char  path[PATH_LEN];
    bool  found;

    stream = NULL;
    found  = false;
    p101_snprintf(env, err, path, sizeof(path), "%s/summary.txt", paths->observe_dir);

    if(p101_error_has_error(err))
    {
        goto done;
    }

    stream = p101_fopen(env, err, path, "r");

    if(stream == NULL)
    {
        p101_error_reset(err);
        goto done;
    }

    while(p101_error_has_no_error(err) && p101_fgets(env, err, line, MSG_LEN, stream) != NULL)
    {
        if(p101_strncmp(env, line, "resources:", sizeof("resources:") - 1U) == 0)
        {
            trim_newline(env, line);
            found = true;
            break;
        }
    }

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }

    return found;
}

static void trim_newline(const struct p101_env *env, char *line)
{
    size_t length;

    length = p101_strlen(env, line);
    while(length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r'))
    {
        line[length - 1U] = '\0';
        length--;
    }
}

static void write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    const unsigned char *cursor;
    size_t               length;

    p101_fputc(env, err, '\"', stream);

    if(text == NULL)
    {
        goto done;
    }

    cursor = (const unsigned char *)text;
    length = p101_strlen(env, text);
    for(size_t index = 0U; index < length && p101_error_has_no_error(err); index++)
    {
        unsigned char current;

        current = cursor[index];
        if(current == '\"' || current == '\\')
        {
            p101_fputc(env, err, '\\', stream);
            p101_fputc(env, err, (int)current, stream);
        }
        else if(current == '\n')
        {
            p101_fputs(env, err, "\\n", stream);
        }
        else if(current == '\r')
        {
            p101_fputs(env, err, "\\r", stream);
        }
        else if(current == '\t')
        {
            p101_fputs(env, err, "\\t", stream);
        }
        else if(current < JSON_CONTROL_BYTE_LIMIT || current >= JSON_NON_ASCII_BYTE_BASE)
        {
            p101_fprintf(env, err, stream, "\\u%04x", (unsigned)current);
        }
        else
        {
            p101_fputc(env, err, (int)current, stream);
        }
    }

done:
    p101_fputc(env, err, '\"', stream);
}

void p101_doctor_write_summary_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->summary, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "# p101 doctor\n\n", stream);
    p101_fprintf(env, err, stream, "Command: `%s`\n\n", args->command_argv[0]);
    if(args->skip_source_contracts)
    {
        p101_fputs(env, err, "Source contracts: `skipped`\n\n", stream);
    }
    else
    {
        p101_fputs(env, err, "Source paths:", stream);

        for(int i = 0; i < args->source_count; i++)
        {
            p101_fprintf(env, err, stream, " `%s`", args->source_paths[i]);
        }

        p101_fputs(env, err, "\n\n", stream);
    }
    p101_fprintf(env, err, stream, "Fault-injection cases requested: `%u`\n\n", args->fault_count);

    p101_fputs(env, err, "## Quick grade\n\n", stream);
    if(!args->skip_source_contracts)
    {
        write_grade_line(env, err, stream, "Wrapper usage", result->wrapper_status);
        write_grade_line(env, err, stream, "Error contracts", result->error_contract_status);
    }
    write_grade_line(env, err, stream, "Module shape", result->module_status);
    write_grade_line(env, err, stream, "Runtime resources", result->observe_status);
    write_grade_line(env, err, stream, "Error paths", result->fault_walk_status);
    p101_fputs(env, err, "\n", stream);
    write_observe_detail(env, err, stream, paths);
    p101_fputs(env, err, "\n", stream);

    p101_fputs(env, err, "## Results\n\n", stream);
    p101_fputs(env, err, "| Step | Status |\n", stream);
    p101_fputs(env, err, "| --- | --- |\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_doctor_print_status_markdown(env, err, stream, "p101-wrapper-audit", result->wrapper_status);
        p101_doctor_print_status_markdown(env, err, stream, "p101-error-contract", result->error_contract_status);
    }
    p101_doctor_print_status_markdown(env, err, stream, "p101-module-map", result->module_status);
    p101_doctor_print_status_markdown(env, err, stream, "p101-observe", result->observe_status);
    p101_doctor_print_status_markdown(env, err, stream, "p101-error-path-walk", result->fault_walk_status);

    p101_fputs(env, err, "\n## Artifacts\n\n", stream);
    p101_fputs(env, err, "- Command: [command.txt](./command.txt)\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_fputs(env, err, "- Wrapper audit stdout: [wrapper-audit.stdout.txt](./wrapper-audit.stdout.txt)\n", stream);
        p101_fputs(env, err, "- Wrapper audit stderr: [wrapper-audit.stderr.txt](./wrapper-audit.stderr.txt)\n", stream);
        p101_fputs(env, err, "- Shared source facts: [source-facts.tsv](./source-facts.tsv)\n", stream);
        p101_fputs(env, err, "- Admitted source inputs: [source-inputs.json](./source-inputs.json)\n", stream);
        p101_fputs(env, err, "- Error contract stdout: [error-contract.stdout.txt](./error-contract.stdout.txt)\n", stream);
        p101_fputs(env, err, "- Error contract stderr: [error-contract.stderr.txt](./error-contract.stderr.txt)\n", stream);
    }
    p101_fputs(env, err, "- Module map report: [module-map.md](./module-map.md)\n", stream);
    p101_fputs(env, err, "- Module map JSON findings: [module-map.json](./module-map.json)\n", stream);
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
    p101_fputs(env, err, "- Run manifest: [manifest.txt](./manifest.txt)\n", stream);

    p101_fputs(env, err, "\n## How to read this\n\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_fputs(env, err, "`p101-wrapper-audit` is the static boundary story: it reports calls that bypass available p101 wrappers.\n\n", stream);
        p101_fputs(env, err, "`p101-error-contract` is the static error-handling story: it reports p101 calls used before a visible env/error contract.\n\n", stream);
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

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->json, "w");

    if(stream == NULL)
    {
        goto done;
    }

    p101_fputs(env, err, "{\n  \"schema\": \"p101-doctor-v2\",\n", stream);
    p101_fputs(env, err, "  \"command\": ", stream);
    write_json_string(env, err, stream, args->command_argv[0]);
    p101_fputs(env, err, ",\n", stream);
    if(args->skip_source_contracts)
    {
        p101_fputs(env, err, "  \"source_contracts\": false,\n", stream);
    }
    else
    {
        p101_fputs(env, err, "  \"source_contracts\": true,\n", stream);
    }
    p101_fputs(env, err, "  \"source_paths\": [", stream);
    for(int i = 0; i < args->source_count; i++)
    {
        if(i > 0)
        {
            p101_fputs(env, err, ", ", stream);
        }
        write_json_string(env, err, stream, args->source_paths[i]);
    }
    p101_fputs(env, err, "],\n", stream);
    p101_fprintf(env, err, stream, "  \"fault_count\": %u,\n", args->fault_count);
    p101_fputs(env, err, "  \"doctor_dir\": ", stream);
    write_json_string(env, err, stream, paths->dir);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"manifest\": ", stream);
    write_json_string(env, err, stream, paths->manifest);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"source_facts\": ", stream);
    write_json_string(env, err, stream, paths->facts);
    p101_fputs(env, err, ",\n  \"source_inputs\": ", stream);
    write_json_string(env, err, stream, paths->input_manifest);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"module_report\": ", stream);
    write_json_string(env, err, stream, paths->module_report);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"module_findings\": ", stream);
    write_json_string(env, err, stream, paths->module_json);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"observe_dir\": ", stream);
    write_json_string(env, err, stream, paths->observe_dir);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"fault_dir\": ", stream);
    write_json_string(env, err, stream, paths->fault_dir);
    p101_fputs(env, err, ",\n", stream);
    p101_fputs(env, err, "  \"statuses\": {\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_doctor_print_status_json(env, err, stream, "p101_wrapper_audit", result->wrapper_status);
        p101_fputs(env, err, ",\n", stream);
        p101_doctor_print_status_json(env, err, stream, "p101_error_contract", result->error_contract_status);
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
