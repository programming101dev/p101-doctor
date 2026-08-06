#include "report.h"
#include "constants.h"
#include "status.h"
#include <errno.h>
#include <inttypes.h>
#include <p101_c/p101_stdio.h>
#include <p101_c/p101_string.h>
#include <p101_filesystem/p101_dirent.h>
#include <p101_filesystem/p101_fnmatch.h>
#include <p101_filesystem/p101_ftw.h>
#include <p101_filesystem/p101_glob.h>
#include <p101_filesystem/p101_libgen.h>
#include <p101_filesystem/p101_stdio.h>
#include <p101_filesystem/p101_stdlib.h>
#include <p101_filesystem/p101_unistd.h>
#include <p101_filesystem/sys/p101_stat.h>
#include <p101_filesystem/sys/p101_statvfs.h>
#include <p101_record/record.h>
#include <p101_tool_event/receipt.h>
#include <stdio.h>

static void        write_grade_line(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status);
static const char *grade_for_status(int status);
static void        write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text);
static bool        doctor_result_has_findings(const struct arguments *args, const struct doctor_result *result);
static bool        doctor_result_has_trouble(const struct arguments *args, const struct doctor_result *result);
static size_t      doctor_result_completed_checks(const struct arguments *args, const struct doctor_result *result);
static void        write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, const char *path);

static bool doctor_result_has_findings(const struct arguments *args, const struct doctor_result *result)
{
    return ((!args->skip_source_contracts && (p101_doctor_status_has_findings(result->wrapper_status) || p101_doctor_status_has_findings(result->error_contract_status))) || p101_doctor_status_has_findings(result->module_status)) != 0;
}

static bool doctor_result_has_trouble(const struct arguments *args, const struct doctor_result *result)
{
    return ((!args->skip_source_contracts && (!p101_doctor_status_is_acceptable(result->wrapper_status) || !p101_doctor_status_is_acceptable(result->error_contract_status))) || !p101_doctor_status_is_acceptable(result->module_status)) != 0;
}

static size_t doctor_result_completed_checks(const struct arguments *args, const struct doctor_result *result)
{
    size_t completed;

    completed = 0U;
    if(p101_doctor_status_is_acceptable(result->module_status))
    {
        completed++;
    }
    if(!args->skip_source_contracts)
    {
        if(p101_doctor_status_is_acceptable(result->wrapper_status))
        {
            completed++;
        }
        if(p101_doctor_status_is_acceptable(result->error_contract_status))
        {
            completed++;
        }
    }
    return completed;
}

static void write_artifact_fingerprint(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, const char *path)
{
    struct p101_tool_event_fingerprint fingerprint;

    if(p101_access(env, P101_ERROR_OPTIONAL, path, F_OK) != 0)    // P101_ERROR_OPTIONAL rationale: missing artifacts are explicit receipt evidence.
    {
        p101_fprintf(env, err, stream, "artifact.%s=missing path=%s\n", label, path);
        return;
    }
    if(p101_tool_event_fingerprint_file(err, path, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint) == 0)
    {
        p101_fprintf(env, err, stream, "artifact.%s=present path=%s bytes=%zu records=%zu fnv1a64=%016" PRIx64 " final_newline=%d\n", label, path, fingerprint.bytes, fingerprint.records, fingerprint.fnv1a64, fingerprint.final_newline);
    }
}

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

static void write_json_string(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *text)
{
    P101_TRACE_SCOPE(env);
    if(p101_record_write_json_string(stream, text == NULL ? "" : text) != 0)
    {
        P101_ERROR_RAISE_ERRNO(err, errno == 0 ? EIO : errno);
    }
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
    p101_fputs(env, err, "## Quick grade\n\n", stream);
    if(!args->skip_source_contracts)
    {
        write_grade_line(env, err, stream, "Wrapper usage", result->wrapper_status);
        write_grade_line(env, err, stream, "Error contracts", result->error_contract_status);
    }
    write_grade_line(env, err, stream, "Module shape", result->module_status);
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
    p101_fputs(env, err, "- Machine-readable doctor index: [doctor.json](./doctor.json)\n", stream);
    p101_fputs(env, err, "- Bounded artifact fingerprints: [receipt.txt](./receipt.txt)\n", stream);
    p101_fputs(env, err, "- Verifiable run receipt: [tool-receipt.json](./tool-receipt.json)\n", stream);
    p101_fputs(env, err, "- Run manifest: [manifest.txt](./manifest.txt)\n", stream);

    p101_fputs(env, err, "\n## How to read this\n\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_fputs(env, err, "`p101-wrapper-audit` is the static boundary story: it reports calls that bypass available p101 wrappers.\n\n", stream);
        p101_fputs(env, err, "`p101-error-contract` is the static error-handling story: it reports p101 calls used before a visible env/error contract.\n\n", stream);
    }
    p101_fputs(env, err, "`p101-module-map` is the static design story: it reports module shape, public API surface, include relationships, and likely split/static-scope opportunities.\n\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_evidence_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths)
{
    FILE *stream;

    P101_TRACE_SCOPE(env);
    stream = p101_fopen(env, err, paths->receipt, "w");
    if(stream == NULL)
    {
        goto done;
    }
    p101_fputs(env, err, "schema=p101-doctor-evidence-receipt-v1\n", stream);
    write_artifact_fingerprint(env, err, stream, "command", paths->command);
    write_artifact_fingerprint(env, err, stream, "manifest", paths->manifest);
    if(!args->skip_source_contracts)
    {
        write_artifact_fingerprint(env, err, stream, "wrapper-stdout", paths->wrapper_stdout);
        write_artifact_fingerprint(env, err, stream, "wrapper-stderr", paths->wrapper_stderr);
        write_artifact_fingerprint(env, err, stream, "source-facts", paths->facts);
        write_artifact_fingerprint(env, err, stream, "source-inputs", paths->input_manifest);
        write_artifact_fingerprint(env, err, stream, "error-contract-stdout", paths->error_contract_stdout);
        write_artifact_fingerprint(env, err, stream, "error-contract-stderr", paths->error_contract_stderr);
    }
    write_artifact_fingerprint(env, err, stream, "module-map-stdout", paths->module_stdout);
    write_artifact_fingerprint(env, err, stream, "module-map-stderr", paths->module_stderr);
    write_artifact_fingerprint(env, err, stream, "module-map-report", paths->module_report);
    write_artifact_fingerprint(env, err, stream, "module-map-json", paths->module_json);
    write_artifact_fingerprint(env, err, stream, "summary", paths->summary);
    write_artifact_fingerprint(env, err, stream, "doctor-json", paths->json);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}

void p101_doctor_write_receipt_file(const struct p101_env *env, struct p101_error *err, const struct arguments *args, const struct doctor_paths *paths, const struct doctor_result *result)
{
    struct p101_tool_event_fingerprint fingerprint;
    struct p101_tool_run_receipt       receipt;
    FILE                              *stream;

    P101_TRACE_SCOPE(env);
    stream = NULL;
    if(p101_tool_event_fingerprint_file(err, paths->receipt, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_BYTES, P101_TOOL_EVENT_RECEIPT_DEFAULT_MAX_RECORDS, &fingerprint) != 0)
    {
        goto done;
    }
    receipt = (struct p101_tool_run_receipt){
        .tool_name        = "p101-doctor",
        .tool_version     = "1.0.0",
        .input_schema     = "p101-doctor-evidence-receipt-v1",
        .input_identity   = paths->receipt,
        .policy_schema    = "p101-doctor-policy-v1",
        .policy_identity  = "full-source-contracts",
        .run_identity     = paths->dir,
        .failed_stage     = "",
        .first_diagnostic = "",
        .checks_attempted = 3U,
        .checks_completed = doctor_result_completed_checks(args, result),
        .does_not_prove   = "runtime correctness, complete wrapper instrumentation, third-party code, or unsupported compiler behavior",
    };
    if(args->skip_source_contracts)
    {
        receipt.policy_identity  = "module-map-only";
        receipt.checks_attempted = 1U;
    }
    if(doctor_result_has_trouble(args, result))
    {
        receipt.outcome          = P101_TOOL_OUTCOME_TOOL_ERROR;
        receipt.failure_reason   = P101_TOOL_FAILURE_TOOL_ERROR;
        receipt.failed_stage     = "subtool";
        receipt.first_diagnostic = "at least one doctor subtool did not produce an acceptable result";
    }
    else if(doctor_result_has_findings(args, result))
    {
        receipt.outcome          = P101_TOOL_OUTCOME_FINDINGS;
        receipt.failure_reason   = P101_TOOL_FAILURE_FINDINGS_PRESENT;
        receipt.failed_stage     = "source-contract";
        receipt.first_diagnostic = "at least one doctor subtool reported findings";
    }
    else
    {
        receipt.outcome        = P101_TOOL_OUTCOME_CLEAN;
        receipt.failure_reason = P101_TOOL_FAILURE_NONE;
    }
    stream = p101_fopen(env, err, paths->tool_receipt, "w");
    if(stream != NULL)
    {
        (void)p101_tool_run_receipt_write_json(err, stream, &receipt, &fingerprint);
    }

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

    p101_fputs(env, err, "{\n  \"schema\": \"p101-doctor-v3\",\n", stream);
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
    p101_fputs(env, err, "  \"statuses\": {\n", stream);
    if(!args->skip_source_contracts)
    {
        p101_doctor_print_status_json(env, err, stream, "p101_wrapper_audit", result->wrapper_status);
        p101_fputs(env, err, ",\n", stream);
        p101_doctor_print_status_json(env, err, stream, "p101_error_contract", result->error_contract_status);
        p101_fputs(env, err, ",\n", stream);
    }
    p101_doctor_print_status_json(env, err, stream, "p101_module_map", result->module_status);
    p101_fputs(env, err, "\n  }\n", stream);
    p101_fputs(env, err, "}\n", stream);

done:
    if(stream != NULL)
    {
        p101_fclose(env, err, stream);
    }
}
