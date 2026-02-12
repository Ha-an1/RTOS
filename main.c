/*
 * main.c - Entry Point and CLI
 *
 * Provides a command-line interface to run individual test scenarios
 * or all tests at once.
 *
 * Usage:
 *   rtos_scheduler [1-8|all]
 *
 * Author: RTOS Project
 * Date:   2026-02-13
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Test function declarations (defined in tests.c) ──────────────── */
extern void test_basic_priority(void);
extern void test_preemption(void);
extern void test_priority_inversion_with_pi(void);
extern void test_priority_inversion_without_pi(void);
extern void test_transitive_pi(void);
extern void test_rms(void);
extern void test_semaphore(void);
extern void test_deadline_miss(void);

/* ── Usage ────────────────────────────────────────────────────────── */

static void print_usage(const char *prog)
{
    printf("\n");
    printf("================================================================\n");
    printf("  RTOS Task Scheduler — Priority Inheritance Demo\n");
    printf("================================================================\n");
    printf("\n");
    printf("Usage: %s [scenario]\n\n", prog);
    printf("  Scenarios:\n");
    printf("    1   - Basic Priority Scheduling\n");
    printf("    2   - Preemption\n");
    printf("    3   - Priority Inversion WITH PI  (the killer demo)\n");
    printf("    4   - Priority Inversion WITHOUT PI (comparison)\n");
    printf("    5   - Transitive Priority Inheritance\n");
    printf("    6   - Rate Monotonic Scheduling\n");
    printf("    7   - Semaphore Producer-Consumer\n");
    printf("    8   - Deadline Miss Detection\n");
    printf("    all - Run all scenarios\n");
    printf("\n");
    printf("  Example:\n");
    printf("    %s 3      # Run the priority inheritance demo\n", prog);
    printf("    %s all    # Run everything\n", prog);
    printf("\n");
}

static void run_all(void)
{
    test_basic_priority();
    test_preemption();
    test_priority_inversion_with_pi();
    test_priority_inversion_without_pi();
    test_transitive_pi();
    test_rms();
    test_semaphore();
    test_deadline_miss();
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    const char *arg = argv[1];

    if (strcmp(arg, "all") == 0) {
        run_all();
    } else if (strcmp(arg, "1") == 0) {
        test_basic_priority();
    } else if (strcmp(arg, "2") == 0) {
        test_preemption();
    } else if (strcmp(arg, "3") == 0) {
        test_priority_inversion_with_pi();
    } else if (strcmp(arg, "4") == 0) {
        test_priority_inversion_without_pi();
    } else if (strcmp(arg, "5") == 0) {
        test_transitive_pi();
    } else if (strcmp(arg, "6") == 0) {
        test_rms();
    } else if (strcmp(arg, "7") == 0) {
        test_semaphore();
    } else if (strcmp(arg, "8") == 0) {
        test_deadline_miss();
    } else {
        fprintf(stderr, "Unknown scenario: %s\n", arg);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
