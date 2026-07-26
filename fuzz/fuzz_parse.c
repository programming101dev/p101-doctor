/*
 * libFuzzer harness for p101-doctor's argument parser.
 */
#include <p101_c/p101_setjmp.h>
#include <p101_c/p101_stdlib.h>
#include <p101_c/p101_string.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static jmp_buf g_fuzz_exit_jmp;

#include "../src/main.c"

_Noreturn void p101_fuzz_exit(const struct p101_env *env, int code)
{
    (void)code;
    p101_longjmp(env, g_fuzz_exit_jmp, 1);
}

#define FUZZ_MAX_ARGS 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char              *buf;
    char              *argv[FUZZ_MAX_ARGS];
    int                argc;
    char              *p;
    struct p101_error *err;
    struct p101_env   *env;
    struct arguments   args;

    buf = NULL;
    err = p101_error_create(false);
    env = p101_env_create(err, NULL);

    buf = (char *)p101_malloc(env, err, size + 1U);
    if(buf == NULL)
    {
        goto done;
    }
    p101_memcpy(env, buf, data, size);
    buf[size] = '\0';

    argv[0] = (char *)"p101-doctor";
    argc    = 1;
    p       = buf;
    while(argc < FUZZ_MAX_ARGS - 1)
    {
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\v' || *p == '\f')
        {
            p++;
        }
        if(*p == '\0')
        {
            break;
        }
        argv[argc++] = p;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '\v' && *p != '\f')
        {
            p++;
        }
        if(*p != '\0')
        {
            *p++ = '\0';
        }
    }
    argv[argc] = NULL;

#ifdef __GLIBC__
    optind = 0;
#else
    {
        extern int optreset;
        optreset = 1;
        optind   = 1;
    }
#endif

    p101_memset(env, &args, 0, sizeof(args));
    args.source_path          = DEFAULT_SOURCE_PATH;
    args.fault_count_str      = DEFAULT_FAULT_COUNT;
    args.p101_wrapper_audit   = DEFAULT_WRAPPER_AUDIT;
    args.p101_observe         = DEFAULT_OBSERVE_PATH;
    args.p101_error_path_walk = DEFAULT_ERROR_WALK_PATH;
    args.resource_tracker     = DEFAULT_TRACKER_PATH;
    args.p101_trace           = DEFAULT_TRACE_PATH;
    args.p101_report          = DEFAULT_REPORT_PATH;

    for(int i = 1; i < argc; i++)
    {
        if(p101_strcmp(env, argv[i], "--help") == 0)
        {
            goto done;
        }

        if(argv[i][0] == '-' && argv[i][1] != '-' && p101_strchr(env, argv[i] + 1, 'h') != NULL)
        {
            goto done;
        }
    }

    if(p101_setjmp(env, g_fuzz_exit_jmp) == 0)
    {
        parse_arguments(env, err, argc, argv, &args);
    }

done:
    p101_free(env, buf);
    p101_env_destroy(env);
    p101_error_destroy(err);
    return 0;
}
