#include <assert.h>
#include <ctype.h>
#include <cpufreq.h>
#include <limits.h>
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

// Size of the character buffer passed to NumberToString().
#define NUMBER_BUF_SIZE (sizeof(unsigned long) * CHAR_BIT / 2)

// Converts an unsigned long value to its decimal string representation, with
// groups of three digits separated by commas. For example, 12345 is rendered
// as "12,345".
char *NumberToString(unsigned long value, char (*buf)[NUMBER_BUF_SIZE]) {
    char *p = *buf + NUMBER_BUF_SIZE;
    *--p = '\0';
    if (value == 0) {
        *--p = '0';
    } else {
        for (int n = 0; value > 0; ++n) {
            if (n == 3) {
                *--p = ',';
                n = 0;
            }
            *--p = '0' + value%10;
            value /= 10;
        }
    }
    return p;
}

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

// Parses a CPU frequency string.
//
// Valid strings look like: "42 mHz", "100,000", or "1.2G".
//
// Formally, a frequency string consists of a decimal number and a suffix
// denoting the scale of the result.
//
// The number and suffix may be separated by whitespace. A number may contain
// a single decimal point. Commas may be used to group digits inside a number.
//
// The suffix is case-insensitive and denotes the scale of the result:
// k(Hz), m(Hz) and g(Hz) are valid options. If no suffix is provided, kHz is
// assumed by default. If the string could be parsed, then this function returns
// true, and the frequency in kHz is stored in *result. Otherwise, the function
// returns false and *result is unchanged.
bool ParseFreq(const char *str, unsigned long *result) {
    // Logically, the result is defined as val * multi / scale.
    unsigned long val = 0;
    unsigned long scale = 0;
    unsigned long multi = 1;
    const char *p = str;
    while (isspace(*p)) ++p;
    if (*p < '0' && *p > '9') {
        // Must start with a digit.
        return false;
    }
    for ( ; *p; ++p) {
        if (*p == ',') {
            // Grouping comma ignored.
        } else if (*p == '.') {
            if (scale != 0) {
                // Can't contain more than one decimal point.
                return false;
            }
            scale = 1;
        } else if (*p >= '0' && *p <= '9') {
            int add = *p - '0';
            if (val > (ULONG_MAX - add)/10) {
                return false;
            }
            val = 10*val + add;
            if (scale > ULONG_MAX/10) {
                return false;
            }
            scale *= 10;
        } else {
            break;
        }
    }
    if (scale == 0) {
        scale = 1;
    }
    while (*p && isspace(*p)) ++p;
    if (*p) {
        char ch = tolower(*p++);
        if (ch == 'k') {
            multi = 1;
        } else if (ch == 'm') {
            multi = 1000;
        } else if (ch == 'g') {
            multi = 1000000;
        } else {
            return false;
        }
        if (*p && (tolower(*p++) != 'h' || tolower(*p++) != 'z')) {
            return false;
        }
    }
    while (*p && isspace(*p)) ++p;
    if (*p) {
        return false;
    }
    if (multi > scale) {
        multi /= scale;
        if (val > ULONG_MAX/multi) {
            return false;
        }
        *result = val * multi;
        return true;
    } else {
        scale /= multi;
        *result = val / scale;
        return true;
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
