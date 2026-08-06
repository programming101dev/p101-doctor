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
    int  p101_expression_result_5;
    bool p101_call_result_6;
    bool p101_call_result_7;
    p101_call_result_6 = p101_doctor_status_is_clean(status);
    if(p101_call_result_6)
    {
        p101_expression_result_5 = 1;
    }
    else
    {
        p101_call_result_7 = p101_doctor_status_has_findings(status);
        if(p101_call_result_7)
        {
            p101_expression_result_5 = 1;
        }
        else
        {
            p101_expression_result_5 = 0;
        }
    }
    return p101_expression_result_5 != 0;
}

const char *p101_doctor_status_word(int status)
{
    bool        p101_call_result_4;
    bool        p101_call_result_1;
    const char *word;

    word = "trouble";

    p101_call_result_1 = p101_doctor_status_is_clean(status);
    if(p101_call_result_1)
    {
        word = "clean";
    }
    else
    {
        p101_call_result_4 = p101_doctor_status_has_findings(status);
        if(p101_call_result_4)
        {
            word = "findings";
        }
    }

    return word;
}

void p101_doctor_print_status_markdown(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    const char *p101_call_result_2;
    if(WIFEXITED(status))
    {
        p101_call_result_2 = p101_doctor_status_word(status);
        p101_fprintf(env, err, stream, "| %s | %s (exit %d) |\n", label, p101_call_result_2, WEXITSTATUS(status));
    }
    else if(WIFSTOPPED(status))
    {
        p101_fprintf(env, err, stream, "| %s | trouble (status %d) |\n", label, status);
    }
    else
    {
        p101_fprintf(env, err, stream, "| %s | trouble (signal %d) |\n", label, WTERMSIG(status));
    }
}

void p101_doctor_print_status_json(const struct p101_env *env, struct p101_error *err, FILE *stream, const char *label, int status)
{
    const char *p101_call_result_3;
    if(WIFEXITED(status))
    {
        p101_call_result_3 = p101_doctor_status_word(status);
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"exit\", \"code\": %d, \"result\": \"%s\"}", label, WEXITSTATUS(status), p101_call_result_3);
    }
    else if(WIFSTOPPED(status))
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"status\", \"status\": %d, \"result\": \"trouble\"}", label, status);
    }
    else
    {
        p101_fprintf(env, err, stream, "    \"%s\": {\"kind\": \"signal\", \"signal\": %d, \"result\": \"trouble\"}", label, WTERMSIG(status));
    }
}
