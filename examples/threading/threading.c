#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
  struct thread_data *pThreadData = (struct thread_data*)thread_param;

  pThreadData->thread_complete_success = false;

  if(usleep((pThreadData->wait_to_obtain_ms) * 1000) != 0)
  {
    return thread_param;
  }

  if(pthread_mutex_lock(pThreadData->pMutex) != 0)
  {
    return thread_param;
  }

  if(usleep((pThreadData->wait_to_release_ms) * 1000) != 0)
  {
    return thread_param;
  }

  if(pthread_mutex_unlock(pThreadData->pMutex) != 0)
  {
    return thread_param;
  }

  pThreadData->thread_complete_success = true;

  return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
  struct thread_data *pThreadData = calloc(1, sizeof(struct thread_data));

  if(pThreadData == NULL)
  {
    return false;
  }

  pThreadData->wait_to_obtain_ms = wait_to_obtain_ms;
  pThreadData->wait_to_release_ms = wait_to_release_ms;
  pThreadData->pThread = thread;
  pThreadData->pMutex = mutex;

  if(pthread_create(thread, NULL, &threadfunc, (void*)pThreadData) != 0)
  {
    free(pThreadData);
    return false;
  } 

  return true;
}
