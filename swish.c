#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Task 4: Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // You should adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    int echo = 0;
    if (argc > 1 && strcmp(argv[1], "--echo") == 0) {
        echo = 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        if (echo) {
            printf("%s", cmd);
        }
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // TODO Task 1: Print the shell's current working directory
            // Use the getcwd() system call

            char dir[CMD_LEN]; //create buffer for the directory

            if(getcwd(dir, sizeof(dir)) != NULL){ //get current working directory
                printf("%s\n", dir);
            }else{ //error
                perror("getcwd");
            }
        }

        else if (strcmp(first_token, "cd") == 0) {
            // TODO Task 1: Change the shell's current working directory
            // Use the chdir() system call
            // If the user supplied an argument (token at index 1), change to that directory
            // Otherwise, change to the home directory by default
            // This is available in the HOME environment variable (use getenv())

            char curdir[CMD_LEN];
            char file[CMD_LEN];
            memset(file, 0, sizeof(file));

            if(strvec_get(&tokens, 1) != NULL){
                if(getcwd(curdir, sizeof(curdir)) == NULL) perror("getcwd");
                else{
                    if(strvec_get(&tokens, 1)[0] != '/'){
                        strcat(file, "/");
                        strcat(file, strvec_get(&tokens, 1));
                        strcat(curdir, file);
                    }else if(strvec_get(&tokens, 1)[0] == '/'){
                        memset(curdir, 0, sizeof(curdir));
                        strcpy(curdir, strvec_get(&tokens, 1));
                    }
                    if(chdir(curdir) == -1){
                        perror("chdir");
                    }
                }
            }else{
                strcpy(file, getenv("HOME"));
                if(file == NULL){
                    printf("invalid file");
                }else{
                    if(chdir(file) == -1){
                        perror("chdir");
                    }
                }
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Task 5: Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == JOB_BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Task 5: Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == 1) {
                printf("Failed to resume job in foreground\n");
            }
            
        }

        // Task 6: Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == 1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Task 6: Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == 1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Task 6: Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == 1) {
                printf("Failed to wait for all background jobs\n");
            }
        }
        else {

            // TASK 6: If the last token input by the user is "&", start the current
            // command in the background.
            // 1. Determine if the last token is "&". If present, use strvec_take() to remove
            //    the "&" from the token list.
            const char *last_token = strvec_get(&tokens, tokens.length - 1);
            if (last_token == NULL) {
                fprintf(stderr, "%s", "failed to check last token");
            } else if (*last_token == '&') { // run process in background

                // TASK 6:
                // 2. Modify the code for the parent (shell) process: Don't use tcsetpgrp() or
                //    use waitpid() to interact with the newly spawned child process.
                strvec_take(&tokens, tokens.length);  // no error return, no need to check
                pid_t pid = fork();
                if (pid == 0) {  // child process
                    if(run_command(&tokens) == 1) { 
                        return 1;
                    }

                // TASK 6:
                // 3. Add a new entry to the jobs list with the child's pid, program name,
                //    and status JOB_BACKGROUND.
                } else if (pid > 0) {  // parent process
                    int status = JOB_BACKGROUND;
                    if (job_list_add(&jobs, pid, first_token, status) == 1) {
                        fprintf(stderr, "%s", "failed to add job");
                    };
                } else {  // fork failed
                    fprintf(stderr, "%s", "failed to fork child");
                }
            } else {  // run as normal, in foreground

                // TASK 2: fork(), then run_command() in child, waitpid() in parent
                pid_t pid = fork();
                if (pid == 0) {  // child process
                    // if run_command returns 1, just return 1
                    if (run_command(&tokens) == 1) {
                        return 1;
                    }

                // TASK 4: Set the child process as the target of signals sent to the terminal
                // via the keyboard.
                // To do this, call 'tcsetpgrp(STDIN_FILENO, <child_pid>)', where child_pid is the
                // child's process ID just returned by fork(). Do this in the parent process.
                } else if (pid > 0) { // parent process
                    int status = 0;
                    if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
                        perror("failed to set foreground");
                        return 1;
                    } 

                    // TASK 5: Handle the issue of foreground/background terminal process groups.
                    // Do this by taking the following steps in the shell (parent) process:
                    // 1. Modify your call to waitpid(): Wait specifically for the child just forked, and
                    //    use WUNTRACED as your third argument to detect if it has stopped from a signal
                    if (waitpid(pid, &status, WUNTRACED) == -1) {
                        perror("failed to wait");
                        return 1;
                    }

                    // TASK 5:
                    // 2. After waitpid() has returned, call tcsetpgrp(STDIN_FILENO, <pid>) where pid is
                    //    the process ID of the shell process (use getpid() to obtain it)
                    if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
                        perror("failed to set foreground");
                        return 1;
                    }

                    // TASK 5:
                    // 3. If the child status was stopped by a signal, add it to 'jobs', the
                    //    the terminal's jobs list.
                    // You can detect if this has occurred using WIFSTOPPED on the status
                    // variable set by waitpid()
                    if(WIFSTOPPED(status)){
                        if (job_list_add(&jobs, pid, first_token, status) == 1) {
                            fprintf(stderr, "%s", "failed to add job");
                        }
                    }
                }else{ // an error occured in fork()
                    fprintf(stderr, "%s", "failed to fork child");
                }
            }
        }
        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }
    return 0;
}
