#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // TODO Task 0: Tokenize string s
    // Assume each token is separated by a single space (" ")
    // Use the strtok() function to accomplish this
    // Add each token to the 'tokens' parameter (a string vector)
    // Return 0 on success, 1 on error

    char *token = strtok(s, " ");

    strvec_add(tokens, token);

    while((token = strtok(NULL, " ")) != NULL){
        if(strvec_add(tokens, token) == -1)return 1;
    }

    return 0;
}

int run_command(strvec_t *tokens) {
    // TODO Task 2: Execute the specified program (token 0) with the
    // specified command-line arguments
    // THIS FUNCTION SHOULD BE CALLED FROM A CHILD OF THE MAIN SHELL PROCESS
    // Hint: Build a string array from the 'tokens' vector and pass this into execvp()
    // Another Hint: You have a guarantee of the longest possible needed array, so you
    // won't have to use malloc.
    
    // initialize string array and either loop thru, or copy from vector
    // dont forget null char at end
    // run execvp with first array el as cmd, and then the whole array as args
    // if exec fails, perror("exec") and return 1. otherwise nothing
    char *child_argv[MAX_ARGS];
    const char *first_token = strvec_get(tokens, 0);
    if (first_token == NULL) { return 1; }
    int i = 0;
    char *arg = strvec_get(tokens, i);
    while (arg != NULL) {
        child_argv[i] = arg;
        i++;
        arg = strvec_get(tokens, i);
    }
    child_argv[i] = NULL;
    // TODO Task 3: Extend this function to perform output redirection before exec()'ing
    // Check for '<' (redirect input), '>' (redirect output), '>>' (redirect and append output)
    // entries inside of 'tokens' (the strvec_find() function will do this for you)
    // Open the necessary file for reading (<), writing (>), or appending (>>)
    // Use dup2() to redirect stdin (<), stdout (> or >>)
    // DO NOT pass redirection operators and file names to exec()'d program
    // E.g., "ls -l > out.txt" should be exec()'d with strings "ls", "-l", NULL

    // strvec_find() for < > and >>
    int index = -1;
    if ((index = strvec_find(tokens, "<")) != -1) {
        int in_fd = open(child_argv[index+1], O_RDONLY); 
        if (in_fd == -1) {
            perror("Failed to open input file");
            return 1;
        }
        dup2(in_fd, STDIN_FILENO);
        // if (close(in_fd) == -1 ) {
        //     perror("failed to close file");
        //     return 1;
        // }
        child_argv[index] = NULL;
        child_argv[index+1] = NULL;
    }
    if ((index = strvec_find(tokens, ">")) != -1) {
        int out_fd = open(child_argv[index+1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR|S_IWUSR); // not sure what other flags we need to use
        if (out_fd == -1) {
            perror("failed to open file");
            return 1;
        }
        dup2(out_fd, STDOUT_FILENO); // need to error check these 
        child_argv[index] = NULL;
        child_argv[index+1] = NULL;

    }
    else if ((index = strvec_find(tokens, ">>")) != -1) {
        int out_fd = open(child_argv[index+1], O_WRONLY | O_CREAT |O_APPEND, S_IRUSR|S_IWUSR);
        if (out_fd == -1) {
            perror("failed to open file");
            return 1;
        }
        dup2(out_fd, STDOUT_FILENO);
        child_argv[index] = NULL;
        child_argv[index+1] = NULL;
    }
    // TODO Task 4: You need to do two items of setup before exec()'ing
    // 1. Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults.
    // The code in main() within swish.c sets these handlers to the SIG_IGN value.
    // Adapt this code to use sigaction() to set the handlers to the SIG_DFL value.
    // 2. Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID
    struct sigaction sac;
    sac.sa_handler = SIG_DFL;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    pid_t curpid = getpid();
    setpgid(curpid, curpid);

    if (execvp(first_token, child_argv) == -1) {
        perror("exec");
        return 1;
    }
    // Not reachable after a successful exec(), but retain here to keep compiler happy
    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // TODO Task 5: Implement the ability to resume stopped jobs in the foreground
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    //    Feel free to use sscanf() or atoi() to convert this string to an int
    // 2. Call tcsetpgrp(STDIN_FILENO, <job_pid>) where job_pid is the job's process ID
    // 3. Send the process the SIGCONT signal with the kill() system call
    // 4. Use the same waitpid() logic as in main -- dont' forget WUNTRACED
    // 5. If the job has terminated (not stopped), remove it from the 'jobs' list
    // 6. Call tcsetpgrp(STDIN_FILENO, <shell_pid>). shell_pid is the *current*
    //    process's pid, since we call this function from the main shell process

    for(int i = 1; i < tokens->length; i++){
        job_t *curjob = job_list_get(jobs, atoi(strvec_get(tokens, i)));
        if(curjob == NULL){
            fprintf(stderr, "Job index out of bounds\n");
            return 1;
        }
        tcsetpgrp(STDIN_FILENO, curjob->pid);
        if(kill(curjob->pid, SIGCONT) == -1){
            perror("kill failed");
        }
        int status = 0;
        waitpid(-1, &status, WUNTRACED); 
        tcsetpgrp(STDIN_FILENO, getpid());
         if(WIFEXITED(status) || WIFSIGNALED(status)){
            job_list_remove(jobs, atoi(strvec_get(tokens, i)));
        }
        tcsetpgrp(STDIN_FILENO, getpid());
    }

    // TODO Task 6: Implement the ability to resume stopped jobs in the background.
    // This really just means omitting some of the steps used to resume a job in the foreground:
    // 1. DO NOT call tcsetpgrp() to manipulate foreground/background terminal process group
    // 2. DO NOT call waitpid() to wait on the job
    // 3. Make sure to modify the 'status' field of the relevant job list entry to JOB_BACKGROUND
    //    (as it was JOB_STOPPED before this)

    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // TODO Task 6: Wait for a specific job to stop or terminate
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Make sure the job's status is JOB_BACKGROUND (no sense waiting for a stopped job)
    // 3. Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    // 4. If the process terminates (is not stopped by a signal) remove it from the jobs list

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    // TODO Task 6: Wait for all background jobs to stop or terminate
    // 1. Iterate throught the jobs list, ignoring any stopped jobs
    // 2. For a background job, call waitpid() with WUNTRACED.
    // 3. If the job has stopped (check with WIFSTOPPED), change its
    //    status to JOB_STOPPED. If the job has terminated, do nothing until the
    //    next step (don't attempt to remove it while iterating through the list).
    // 4. Remove all background jobs (which have all just terminated) from jobs list.
    //    Use the job_list_remove_by_status() function.

    return 0;
}
