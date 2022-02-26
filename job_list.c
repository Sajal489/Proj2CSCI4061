#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "job_list.h"

void job_list_init(job_list_t *list) {
    list->head = NULL;
    list->length = 0;
}

void job_list_free(job_list_t *list) {
    job_t *current = list->head;
    while (current != NULL) {
        job_t *temp = current;
        current = current->next;
        free(temp);
    }
    list->head = NULL;
    list->length = 0;
}

int job_list_add(job_list_t *list, pid_t pid, const char *name, int status) {
    if (list->head == NULL) {
        if ((list->head = malloc(sizeof(job_t))) == NULL) {
            return 1;
        }
        strncpy(list->head->name, name, NAME_LEN);
        list->head->pid = pid;
        list->head->status = status;
        list->head->next = NULL;
        list->length = 1;
        return 0;
    }

    job_t *current = list->head;
    while (current->next != NULL) {
        current = current->next;
    }
    if ((current->next = malloc(sizeof(job_t))) == NULL) {
        return 1;
    }
    strncpy(current->next->name, name, NAME_LEN);
    current->next->next = NULL;
    current->next->pid = pid;
    current->next->status = status;
    list->length++;
    return 0;
}

int job_list_remove(job_list_t *list, unsigned idx) {
    if (idx >= list->length) {
        return 1;
    }
    if (idx == 0) {
        job_t *temp = list->head;
        list->head = list->head->next;
        free(temp);
        return 0;
    }

    job_t *current = list->head;
    for (int i = 0; i < idx - 1; i++) {
        current = current->next;
    }
    job_t *temp = current->next;
    current->next = current->next->next;
    free(temp);
    return 0;
}

int job_list_remove_by_pid(job_list_t *list, pid_t pid) {
    if (list->head != NULL && list->head->pid == pid) {
        job_t *temp = list->head;
        list->head = list->head->next;
        free(temp);
        return 0;
    }

    job_t *current = list->head;
    while (current->next != NULL) {
        if (current->next->pid == pid) {
            job_t *temp = current->next;
            current->next = current->next->next;
            free(temp);
            return 0;
        }
    }

    return 1;
}

void job_list_remove_by_status(job_list_t *list, int status) {
    while (list->head != NULL && list->head->status == status) {
        job_t *temp = list->head;
        list->head = list->head->next;
        free(temp);
    }

    if (list ->head != NULL) { // Could have removed all nodes in loop above
        job_t *current = list->head;
        while (current->next != NULL) {
            if (current->next->status == status) {
                job_t *temp = current->next;
                current->next = current->next->next;
                free(temp);
            } else {
                current = current->next;
            }
        }
    }
}

job_t *job_list_get(job_list_t *list, unsigned idx) {
    job_t *current = list->head;
    for (int i = 0; i < idx; i++) {
        if (current == NULL) {
            return NULL;
        }
        current = current->next;
    }
    return current;
}
