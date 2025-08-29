#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    if(!thread_param) {
        ERROR_LOG("no params provided");
    }

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    struct thread_data* data = (struct thread_data*)thread_param;
    usleep(1000 * data->wait_to_obtain_ms);
    int rc = pthread_mutex_lock(data->mutex);
    if(rc) {
        ERROR_LOG("failed to lock mutex (%s)", strerror(errno));
        data->thread_complete_success = false;
    } else {
        usleep(1000 * data->wait_to_release_ms);
        rc = pthread_mutex_unlock(data->mutex);
        if(rc) {
            ERROR_LOG("failed to unlock mutex (%s)", strerror(errno));
            data->thread_complete_success = false;
        } else {
            data->thread_complete_success = true;
        }
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    if(thread == NULL) {
        ERROR_LOG("no thread provided");
        return false;
    }

     if(mutex == NULL) {
        ERROR_LOG("no mutex provided");
        return false;
    }

    if(wait_to_obtain_ms <= 0 || wait_to_release_ms <= 0) {
        ERROR_LOG("wait time must be positive integer");
        return false;
    }

    struct thread_data* data = calloc(1, sizeof(struct thread_data));
    if(data== NULL) {
        ERROR_LOG("failed to allocated memory");
        return false;
    }

    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;

    int rc = pthread_create(thread, NULL, threadfunc, data);
    if(rc) {
        free(data);
        ERROR_LOG("failed to create thread (%s)", strerror(errno));
        return false;
    }

    return true;
}
