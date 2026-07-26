#include "status.h"
#include "constants.h"
#include <p101_c/p101_stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

bool p101_doctor_status_is_clean(int status)
{
    return (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) != 0;
}

bool p101_doctor_status_has_findings(int status)
{
    return (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FINDINGS) != 0;
}

bool p101_doctor_status_is_acceptable(int status)
{
    return (p101_doctor_status_is_clean(status) || p101_doctor_status_has_findings(status)) != 0;
}

const char *p101_doctor_status_word(int status)
{
    const char *word;

    word = "trouble";

    if(p101_doctor_status_is_clean(status))
    {
        word = "clean";
    }
    else if(p101_doctor_status_has_findings(status))
    {
        word = "findings";
    }

    return word;
}

void p101_doctor_print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "| %s | %s (exit %d) |\n", label, p101_doctor_status_word(status), WEXITSTATUS(status));
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

void p101_doctor_print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    if(WIFEXITED(status))
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"exit\", \"code\": %d, \"result\": \"%s\"}", label, WEXITSTATUS(status), p101_doctor_status_word(status));
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
