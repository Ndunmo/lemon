#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <asm/unistd_64.h>

#ifdef __amd64__
#define eax rax
#define orig_eax orig_rax
#else
#endif

#define offsetof(a, b) __builtin_offsetof(a, b)
#define reg_offset(name) offsetof(struct user, regs.name)

typedef struct flags {
    int max_attempts;
    int e_flag;
    char *on_error;
    int ptrace_on;
    int r_flag;
    char *on_restart;
    char **args;
} command_options;

void getFlags(int argc, char *argv[], command_options *options);

int start(command_options *commands);
int exec_child(command_options *commands);
int hold_child(pid_t child, command_options *commands);

int run_trace(pid_t child);
int watch_signals(pid_t child);

int get_reg(pid_t child, int name);
int wait_for_syscall(pid_t child);

void on_restart(command_options *commands);
void on_error(command_options *commands);

int main(int argc, char *argv[]) {

    if(argc < 2) {
        fprintf(stderr, "Usage: %s [-a <number of attemps>|-e <onerror> '<args>'|-r <onrestart> '<args>'] <program> <args>\n", argv[0]);
        exit(1);
    }


    int i;
    command_options *commands = malloc(sizeof(*commands));

    getFlags(argc, argv, commands);

    for(i = 0; i < commands->max_attempts; i++) {
        start(commands);
        on_restart(commands);
    }

    return 0;
}

int start(command_options *commands) {
    pid_t child = fork();

    if (child == 0) {
        return exec_child(commands);
    } else {
        return hold_child(child, commands);
    }
}

/*
 *  Executes the child program
 */

int exec_child(command_options *options) {
    //Sends sets up child for PTRACE
    if(options->ptrace_on)  {
        ptrace(PTRACE_TRACEME);
        kill(getpid(), SIGSTOP);
    }

    return execvp(options->args[0], options->args);
}

/*
 * Keeps the child process running until an exit signal is sent
 *
 */

int hold_child(pid_t child, command_options *options) {

    while(1) {
        if(options->ptrace_on) {
            run_trace(child);
        }

        if(watch_signals(child) != 0) break;
    }

    return 0;
}

/*
 *  Waits for an exit or kill signal
 *
 */

int watch_signals(pid_t child) {

    int status;
    while(1) {
        waitpid(child, &status, 0);
        if(WIFEXITED(status)) {
            printf("Exited normally\n");
            return 1;
        }

        if(WIFSIGNALED(status)) {
            printf("Killed by a signal\n");
            return 2;
        }
    }
}

/*
 * Keeps track of the linux system calls
 *
 */

int run_trace(pid_t child) {

    int status, syscall_rax;

    waitpid(child, &status, 0);

    ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);

    while(1) {
        if(wait_for_syscall(child) != 0) return 1;

        syscall_rax = get_reg(child, reg_offset(orig_eax));
        fprintf(stderr, "%d\n", syscall_rax);

        if(wait_for_syscall(child) != 0) return 1;

        //TODO

    }

    return 0;
}

/*
 * Allows stopped child to continue executing until the next entry to or exit from a syscall
 */

int wait_for_syscall(pid_t child) {

    int status;

    while(1) {
        ptrace(PTRACE_SYSCALL, child, 0, 0);

        waitpid(child, &status, 0);

        //Waits for syscall stop
        if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
            return 0;
        }

        //program finished
        if(WIFEXITED(status)) {
            return 1;
        }
    }
}

void on_restart(command_options *options) {
    if(options->r_flag)
        system(options->on_restart);
}

void on_error(command_options *options) {
    if(options->e_flag)
        system(options->on_error);
}

int get_reg(pid_t child, int off) {
    long val = ptrace(PTRACE_PEEKUSER, child, off);
    assert(errno == 0);

    return val;
}


void getFlags(int argc, char *argv[], command_options *options) {
    int c;
    int i;
    int left;

    while ((c = getopt (argc, argv, "a:e:r:")) != -1) {
        switch (c) {
            case 'a':
                options->max_attempts = atoi(optarg);
                break;
            case 'e':
                options->e_flag = 1;
                options->on_error = optarg;
                break;
            case 'r':
                options->r_flag = 1;
                options->on_restart = optarg;
                break;
            default:
                printf("usage: mod -a:bcp:s:t ['cmd']\n");
                exit(1);
        }
    }

    left = argc - optind;

    options->args = malloc(sizeof(char *) * left);

    for(i = 0; i < left; i++)
        options->args[i] = argv[optind + i];
    options->args[left] = NULL;
}
