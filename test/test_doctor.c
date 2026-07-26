#include "unity.h"
#include "paths.h"
#include "status.h"
#include <p101_c/p101_string.h>
#include <p101_env/env.h>
#include <p101_error/error.h>
#include <stdbool.h>
#include <stdlib.h>

static struct p101_error *error;
static struct p101_env   *env;

void setUp(void)
{
    error = p101_error_create(false);
    env   = p101_env_create(error, NULL);
}

void tearDown(void)
{
    p101_env_destroy(env);
    p101_error_destroy(error);
}

static void test_status_classification(void)
{
    TEST_ASSERT_TRUE(p101_doctor_status_is_clean(0));
    TEST_ASSERT_TRUE(p101_doctor_status_has_findings(256));
    TEST_ASSERT_TRUE(p101_doctor_status_is_acceptable(0));
    TEST_ASSERT_TRUE(p101_doctor_status_is_acceptable(256));
    TEST_ASSERT_FALSE(p101_doctor_status_is_acceptable(512));
}

static void test_make_doctor_paths_uses_requested_directory(void)
{
    struct arguments   args;
    struct doctor_paths paths;

    p101_memset(env, &args, 0, sizeof(args));
    p101_memset(env, &paths, 0, sizeof(paths));
    args.doctor_dir = "/tmp/p101-doctor-test";

    p101_doctor_make_paths(env, error, &args, &paths);

    TEST_ASSERT_FALSE(p101_error_has_error(error));
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test", paths.dir);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/wrapper-audit.stdout.txt", paths.wrapper_stdout);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/wrapper-audit.stderr.txt", paths.wrapper_stderr);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/module-map.stdout.txt", paths.module_stdout);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/module-map.stderr.txt", paths.module_stderr);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/module-map.md", paths.module_report);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/observe", paths.observe_dir);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/fault-walk/case", paths.fault_prefix);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/summary.md", paths.summary);
    TEST_ASSERT_EQUAL_STRING("/tmp/p101-doctor-test/manifest.txt", paths.manifest);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_status_classification);
    RUN_TEST(test_make_doctor_paths_uses_requested_directory);
    return UNITY_END();
}
