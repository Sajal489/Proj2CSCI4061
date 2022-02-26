#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

// Pseudoterminals in canonical mode cut off lines at 4096 chars
#define LINE_LEN 4097
#define POLL_TIMEOUT 10

// Return 0 on success, 1 on error or if EOF reached
int drain_output(int fd) {
    char buf[LINE_LEN + 1];
    // Read at least once
    int nbytes = read(fd, buf, LINE_LEN);
    if (nbytes == -1) {
        perror("read");
        return 1;
    } else if (nbytes == 0) {
        return 1;
    } else {
        buf[nbytes] = '\0';
        printf("%s", buf);
    }

    // Continue reading until it would block
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLHUP;

    int ready_fds = 0;
    while ((ready_fds = poll(&pfd, 1, POLL_TIMEOUT)) > 0) {
        if (pfd.revents & POLLIN) {
            nbytes = read(fd, buf, LINE_LEN);
            if (nbytes == -1) {
                perror("read");
                return 1;
            } else if (nbytes == 0) {
                return 1;
            } else {
                buf[nbytes] = '\0';
                printf("%s", buf);
            }
        } else { // POLLHUP
            return 1;
        }
    }
    if (ready_fds == -1) {
        perror("poll");
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    int pty_fd;
    pid_t child_pid = forkpty(&pty_fd, NULL, NULL, NULL);
    if (child_pid == -1) {
        perror("forkpty");
        return 0;
    } else if (child_pid == 0) {
        if (execlp("./swish", "./swish", "--echo", NULL) == -1) {
            perror("exec");
            return 1;
        }
    } else {
        struct termios term_attr;
        if (tcgetattr(pty_fd, &term_attr) == -1) {
            perror("tcgetattr");
            return 1;
        }
        term_attr.c_lflag = (term_attr.c_lflag & ~ECHO) | ISIG;
        if (tcsetattr(pty_fd, TCSANOW, &term_attr) == -1) {
            perror("tcsetattr");
            close(pty_fd);
            return 1;
        }

        char buf[LINE_LEN];
        while (fgets(buf, LINE_LEN, stdin) != NULL) {
            char *payload = buf;
            int nbytes = strlen(buf);
            int should_echo = 0;

            // >> is interpreted as an echo directive but isn't passed to terminal directly
            if (strncmp(">> ", buf, strlen(">> ")) == 0) {
                should_echo = 1;
                payload = buf + strlen(">> ");
                nbytes = strlen(payload);

                term_attr.c_lflag |= ECHO;
                if (tcsetattr(pty_fd, TCSANOW, &term_attr) == -1) {
                    perror("tcsetattr");
                    close(pty_fd);
                    return 1;
                }
            }

            // Check if we need to pass special control chars to terminal
            if (strcmp("^C\n", payload) == 0) {
                payload = (char *) &term_attr.c_cc[VINTR];
                nbytes = 1;
            } else if (strcmp("^Z\n", payload) == 0) {
                payload = (char *) &term_attr.c_cc[VSUSP];
                nbytes = 1;
            } else if (strcmp("^D\n", payload) == 0) {
                payload = (char *) &term_attr.c_cc[VEOF];
                nbytes = 1;
            }

            if (write(pty_fd, payload, nbytes) < nbytes) {
                perror("write");
                close(pty_fd);
                return 1;
            }

            if (drain_output(pty_fd) != 0) {
                break;
            }

            // Restore pseudoterminal to non-echo mode if needed
            if (should_echo) {
                term_attr.c_lflag &= ~ECHO;
                if (tcsetattr(pty_fd, TCSANOW, &term_attr) == -1) {
                    perror("tcsetattr");
                    close(pty_fd);
                    return 1;
                }
            }
        }
        close(pty_fd);
    }

    return 0;
}
