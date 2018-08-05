#include <assert.h>
#include <ctype.h>
#include <cpufreq.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>

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

void ResetFrequencies() {
    fprintf(stderr, "Resetting CPU frequency scaling policies...\n");
    for (int i = 0; i < ncpus; ++i) {
        struct cpufreq_policy *p = old_policies[i];
        old_policies[i] = NULL;
        if (!p) {
            fprintf(stderr, "CPU %d: missing policy\n", i);
        } else {
            fprintf(stderr, "CPU %d: governor=%s min=%lu max=%lu\n",
                i, p->governor, p->min, p->max);
            if (cpufreq_set_policy(i, p) != 0) {
                fprintf(stderr, "CPU %d: failed to reset policy\n", i);
            }
        }
    }
}

bool SetMaxFrequencies(unsigned long target_freq) {
    fprintf(stderr, "Setting maximum frequency to %lu kHz...\n", target_freq);
    int succeeded = 0;
    for (int i = 0; i < ncpus; ++i) {
        unsigned long min_freq = 0, max_freq = 0;
        if (cpufreq_get_hardware_limits(i, &min_freq, &max_freq) != 0) {
            fprintf(stderr, "CPU %d: could not determine hardware frequency limits\n", i);
        } else if (target_freq < min_freq) {
            fprintf(stderr, "CPU %d: target frequency (%lu kHz) is below hardware minimum (%lu kHz)\n",
                    i, target_freq, min_freq);
        } else if (target_freq > max_freq) {
            fprintf(stderr, "CPU %d: target frequency (%lu kHz) is above hardware maximum (%lu kHz)\n",
                    i, target_freq, max_freq);
        } else if (cpufreq_modify_policy_max(i, target_freq) != 0) {
            fprintf(stderr, "Failed to set maximum frequency of CPU %d\n", i);
        } else {
            ++succeeded;
        }
        // Read back and print the actual frequency, which may differ from what was requested.
        struct cpufreq_policy *p = cpufreq_get_policy(i);
        fprintf(stderr, "CPU %d: governor=%s min=%lu max=%lu\n",
                i, p->governor, p->min, p->max);
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

// Parses a CPU frequency string.
//
// A valid string consists of an integer, optional followed by whitespace, and
// then an optional unit suffix, which may be "k", "kHz", "m", "mHz", "g", or
// "gHz". Case is ignored. If no suffix is provided, kHz is assumed by default.
//
// If the string was valid, this function returns true, and the frequency is
// converted to kHz and stored in *result. Otherwise, the function returns false
// and *result is unchanged.
bool ParseFreq(const char *str, unsigned long *result) {
    char *end = NULL;
    unsigned long freq = 0;
    freq = strtoul(str, &end, 10);
    if (end == str) return false;
    while (*end && isspace(*end)) ++end;
    if (!*end || strcasecmp(end, "k") == 0 || strcasecmp(end, "kHz") == 0) {
        *result = freq;
        return true;
    }
    if (strcasecmp(end, "M") == 0 || strcasecmp(end, "MHz") == 0) {
        *result = freq * 1000;
        return true;
    }
    if (strcasecmp(end, "G") == 0 || strcasecmp(end, "GHz") == 0) {
        *result = freq * 1000000;
        return true;
    }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: cpu-throttle [freq] <command> <args...>\n\n"
                "Frequency is an integer, with an optional m(Hz) or g(Hz) suffix.\n"
                "Without a suffix, the value is interpreted in kHz by default.\n");
        return EXIT_FAILURE;
    }

    unsigned long target_freq = 0;
    if (!ParseFreq(argv[1], &target_freq)) {
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
