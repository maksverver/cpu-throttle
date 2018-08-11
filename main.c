#include <assert.h>
#include <cpufreq.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "number-to-string.h"
#include "parse-frequency.h"

// Total number of CPUs configured.
static int ncpus;

// Policy for each CPU, at startup. Memory is never freed.
static struct cpufreq_policy **old_policies;

bool Initialize() {
    if (geteuid() != 0) {
       fprintf(stderr, "root privileges missing (check that the binary is owned by root and has setuid bit set))\n");
       return false;
    }

    ncpus = get_nprocs();
    if (ncpus < 1) {
        fprintf(stderr, "Could not determine number of CPUs\n");
        return false;
    }

    old_policies = calloc(ncpus, sizeof(*old_policies));
    for (int i = 0; i < ncpus; ++i) {
        struct cpufreq_policy *p = cpufreq_get_policy(i);
        if (!p) {
            fprintf(stderr, "CPU %d: could not retrieve current policy\n", i);
            return false;
        }
        old_policies[i] = p;
    }

    return true;
}

void PrintPolicy(FILE *fp, int cpu, const struct cpufreq_policy *p) {
    char min_buf[NUMBER_BUF_SIZE];
    char max_buf[NUMBER_BUF_SIZE];
    fprintf(fp, "CPU %d: governor=%s min=%s max=%s\n",
            cpu, p->governor,
            NumberToString(p->min, &min_buf),
            NumberToString(p->max, &max_buf));
}

void ResetFrequencies() {
    fprintf(stderr, "Resetting CPU frequency scaling policies...\n");
    for (int i = 0; i < ncpus; ++i) {
        struct cpufreq_policy *p = old_policies[i];
        old_policies[i] = NULL;
        if (!p) {
            fprintf(stderr, "CPU %d: missing policy\n", i);
        } else {
            PrintPolicy(stderr, i, p);
            if (cpufreq_set_policy(i, p) != 0) {
                fprintf(stderr, "CPU %d: failed to reset policy\n", i);
            }
        }
    }
}

bool SetMaxFrequencies(unsigned long target_freq) {
    char target_freq_buf[NUMBER_BUF_SIZE];
    char *target_freq_str = NumberToString(target_freq, &target_freq_buf);
    fprintf(stderr, "Setting maximum frequency to %s kHz...\n", target_freq_str);
    int succeeded = 0;
    for (int i = 0; i < ncpus; ++i) {
        unsigned long min_freq = 0, max_freq = 0;
        if (cpufreq_get_hardware_limits(i, &min_freq, &max_freq) != 0) {
            fprintf(stderr, "CPU %d: could not determine hardware frequency limits\n", i);
        } else if (target_freq < min_freq) {
            char min_freq_buf[NUMBER_BUF_SIZE];
            fprintf(stderr, "CPU %d: target frequency (%s kHz) is below hardware minimum (%s kHz)\n",
                    i, target_freq_str, NumberToString(min_freq, &min_freq_buf));
        } else if (target_freq > max_freq) {
            char max_freq_buf[NUMBER_BUF_SIZE];
            fprintf(stderr, "CPU %d: target frequency (%s kHz) is above hardware maximum (%s kHz)\n",
                    i, target_freq_str, NumberToString(max_freq, &max_freq_buf));
        } else if (cpufreq_modify_policy_max(i, target_freq) != 0) {
            fprintf(stderr, "Failed to set maximum frequency of CPU %d\n", i);
        } else {
            ++succeeded;
        }
        // Read back and print the actual frequency, which may differ from what was requested.
        struct cpufreq_policy *p = cpufreq_get_policy(i);
        PrintPolicy(stderr, i, p);
        cpufreq_put_policy(p);
    }
    return succeeded == ncpus;
}

void IgnoreSignal(int signum) {
    // Strictly speaking, fprintf() is not safe to call in a signal handler,
    // but I'll take my chances.
    fprintf(stderr, "INTERRUPTED: Received signal %d\n", signum);
}

// Executes the given command in a child process, waits for it to terminate,
// and returns its exit status code. If anything goes wrong, EXIT_FAILURE is
// returned instead.
int RunCommand(char *command_argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        // In child process.
        // Drop root privileges and exec command.
        if (setuid(getuid()) != 0) {
            perror("setuid");
        } else if (setgid(getgid()) != 0) {
            perror("setgid");
        } else {
            execvp(command_argv[0], command_argv);
            perror(command_argv[0]);
        }
        _exit(1);
    } else {
        // In parent process.
        // Set up signal handlers so that if we are killed, waitpid()
        // is interrupted and we can exit cleanly.
        signal(SIGHUP, &IgnoreSignal);
        signal(SIGINT, &IgnoreSignal);
        signal(SIGTERM, &IgnoreSignal);
        // Now wait for child to exit and return its status.
        int wstatus = 0;
        if (waitpid(pid, &wstatus, 0) != pid) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
        if (!WIFEXITED(wstatus)) {
            fprintf(stderr, "Child process did not exit normally\n");
            return EXIT_FAILURE;
        }
        return WEXITSTATUS(wstatus);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: cpu-throttle [freq] <command> <args...>\n\n"
                "Frequency is an integer, with an optional m(Hz) or g(Hz) suffix.\n"
                "Without a suffix, the value is interpreted in kHz by default.\n");
        return EXIT_FAILURE;
    }

    unsigned long target_freq = 0;
    if (!ParseFrequency(argv[1], &target_freq)) {
        fprintf(stderr, "Could not parse frequency argument (%s)\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (!Initialize()) {
        return EXIT_FAILURE;
    }
    atexit(&ResetFrequencies);
    if (!SetMaxFrequencies(target_freq)) {
        return EXIT_FAILURE;
    }
    return RunCommand(&argv[2]);
}
