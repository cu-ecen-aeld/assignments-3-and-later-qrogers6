#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "queue.h"
#include "../aesd-char-driver/aesd_ioctl.h"
#include <sys/ioctl.h>
#include <sys/stat.h>

#define USE_AESD_CHAR_DEVICE 1

const static int kPort = 9000;
#ifdef USE_AESD_CHAR_DEVICE
const static char *kSocketData = "/dev/aesdchar";
const char kIOCtrlStr[] = "AESDCHAR_IOCSEEKTO:";
#else
const static char *kSocketData = "/var/tmp/aesdsocketdata";
#endif
const static int kBufferStartLength = 128;

static int sfd = 0;
static int fd = 0;

typedef struct socketThreadData
{
  pthread_mutex_t *mutexExit;
  int cfd;
  bool complete;
  bool exit;
}socketThreadData_t;

typedef struct slistData
{
  pthread_t *thread;
  socketThreadData_t *threadData;
  struct in_addr clientAddr;
  SLIST_ENTRY(slistData) entries;
}slistData_t;

volatile sig_atomic_t gracefullyExit = false;

pthread_mutex_t mutex;

void freeBuffers(char *recvBuffer, char *sendBuffer)
{
  if(recvBuffer != NULL)
  {
    free(recvBuffer);
  }

  if(sendBuffer != NULL)
  {
    free(sendBuffer);
  }
}

void *process(void *threadParam)
{
  socketThreadData_t *threadData = (socketThreadData_t *)threadParam;

  char *recvBuffer = NULL;
  char *sendBuffer = NULL;
  char *tempBuffer = NULL;
  ssize_t bytesRecv = 0;
  ssize_t bytesSent = 0;
  ssize_t bytesRead = 0;
  int bytesProcessed = 0;
  int bufferSize = 0;
  int bufferIndex = 0;
  bool ioctlCmdRecv = false;
  char *ioctlCmdStr = NULL;
  struct aesd_seekto seekto;
  off_t offset;

  while(1)
  {
    recvBuffer = calloc(kBufferStartLength, sizeof(char));

    if(recvBuffer == NULL)
    {
      syslog(LOG_ERR, "calloc() failed with errno [%d]\n", errno);
      freeBuffers(recvBuffer, sendBuffer);
      threadData->complete = true;
      return threadData;
    }

    bufferSize = kBufferStartLength;
    bufferIndex = 0;

    while(1)
    {
      if(threadData->exit)
      {
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        pthread_mutex_unlock(threadData->mutexExit);
        return threadData;
      }

      bytesRecv = recv(threadData->cfd, recvBuffer + bufferIndex, (bufferSize - bufferIndex - 1), 0);

      if(bytesRecv == -1)
      {
        syslog(LOG_ERR, "recv() failed with errno [%d]\n", errno);
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        return threadData;
      }
      else if(bytesRecv == (bufferSize - bufferIndex - 1))
      {
        bufferSize += kBufferStartLength;
        bufferIndex = bufferSize - kBufferStartLength - 1;
        tempBuffer = (char *)realloc(recvBuffer, bufferSize);

        if(tempBuffer == NULL)
        {
          syslog(LOG_ERR, "realloc() failed with errno [%d]\n", errno);
          freeBuffers(recvBuffer, sendBuffer);
          threadData->complete = true;
          return threadData;
        }

        recvBuffer = tempBuffer;
      }
      else
      {
        bufferIndex += bytesRecv;
      }

      *(recvBuffer + (bufferSize - 1)) = 0;

      if(strchr(recvBuffer, '\n') != NULL)
      {
        break;
      }
    }

    pthread_mutex_lock(&mutex);

    fd = open(kSocketData, O_RDWR | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);

    if(strncmp(recvBuffer, kIOCtrlStr, strlen(kIOCtrlStr)) == 0)
    {
      ioctlCmdRecv = true;
      ioctlCmdStr = recvBuffer += strlen(kIOCtrlStr);
      seekto.write_cmd = atoi(ioctlCmdStr);
      ioctlCmdStr = strchr(ioctlCmdStr, ',');
      ioctlCmdStr++;
      seekto.write_cmd_offset = atoi(ioctlCmdStr);

      if(ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto) != 0)
      {
        syslog(LOG_ERR, "ioctl() failed with errno [%d]\n", errno);
        close(fd);
        pthread_mutex_unlock(&mutex);
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        return threadData;
      }
    }
    else
    {
      bytesSent = write(fd, recvBuffer, strlen(recvBuffer));

      if(bytesSent == -1)
      {
        syslog(LOG_ERR, "write() failed with errno [%d]\n", errno);
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        return threadData;
      }
    }

    if(ioctlCmdRecv)
    {
      offset = lseek(fd, 0, SEEK_CUR);
    }
    else
    {
      offset = 0;
    }

    bytesProcessed = lseek(fd, 0, SEEK_END);

    if(bytesProcessed == -1)
    {
      syslog(LOG_ERR, "lseek() failed with errno [%d]\n", errno);
      close(fd);
      pthread_mutex_unlock(&mutex);
      freeBuffers(recvBuffer, sendBuffer);
      threadData->complete = true;
      return threadData;
    }

    lseek(fd, offset, SEEK_SET);

    sendBuffer = calloc(bytesProcessed, sizeof(char));

    if(sendBuffer == NULL)
    {
      syslog(LOG_ERR, "calloc() failed with errno [%d]\n", errno);
      freeBuffers(recvBuffer, sendBuffer);
      threadData->complete = true;
      return threadData;
    }
    
    while(1)
    {
      bytesRead = read(fd, sendBuffer, bytesProcessed);

      if(bytesRead == -1)
      {
        syslog(LOG_ERR, "read() failed with errno [%d]\n", errno);
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        return threadData;
      }

      if(bytesRead == 0)
      {
        break;
      }

      bytesSent = send(threadData->cfd, sendBuffer, bytesRead, 0);

      if(bytesSent == -1)
      {
        syslog(LOG_ERR, "send() failed with errno [%d]\n", errno);
        freeBuffers(recvBuffer, sendBuffer);
        threadData->complete = true;
        return threadData;
      }
    }

    close(fd);
    pthread_mutex_unlock(&mutex);
  }
}

void cleanup()
{
  if(sfd != -1)
  {
    close(sfd);
  }

  if(fd != -1)
  {
    close(fd);
  }
#ifndef USE_AESD_CHAR_DEVICE
  remove(kSocketData);
#endif
  closelog();
}

#ifndef USE_AESD_CHAR_DEVICE
void appendTimestamp(int signo)
{
  time_t t = time(NULL);
  struct tm *localTime= localtime(&t);
  char dateTime[64] = {0};

  strftime(dateTime, sizeof(dateTime), "timestamp: %Y%m%d%H%M%S", localTime);
  strcat(dateTime, "\n");

  pthread_mutex_lock(&mutex);

  if(write(fd, dateTime, strlen(dateTime)) == -1)
  {
    syslog(LOG_ERR, "write() failed with errno [%d]\n", errno);
  }
  pthread_mutex_unlock(&mutex);
}
#endif

static void signalHandler(int signo)
{
  syslog(LOG_DEBUG, "Caught signal, exiting");
  gracefullyExit = true;
}

int main(int argc, char *argv[])
{
  int cfd;
  struct sockaddr_in addr;
  struct sockaddr_in my_addr;
  char ipaddress[INET_ADDRSTRLEN];

  memset(&my_addr, 0, sizeof(my_addr));

  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  my_addr.sin_port = htons(kPort);

  slistData_t *node = NULL;
  slistData_t *tempNode = NULL;
  socketThreadData_t *threadData = NULL;

#ifndef USE_AESD_CHAR_DEVICE
  pthread_mutex_init(&mutex, NULL);
#endif

  openlog(argv[0], LOG_PID, LOG_USER);

  SLIST_HEAD(slisthead, slistData) head;
  SLIST_INIT(&head);

  sfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sfd == -1)
  {
    syslog(LOG_ERR, "socket() failed with errno [%d]\n", errno);
    cleanup();
    exit(EXIT_FAILURE);
  }

  const int opt = 1;
  if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
  {
    syslog(LOG_ERR, "setsockopt() reusability failed with errno [%d]\n", errno);
    cleanup();
    exit(EXIT_FAILURE);
  }

  fcntl(sfd, F_SETFL, O_NONBLOCK);

  if((bind(sfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in))) < 0)
  {
    syslog(LOG_ERR, "bind() failed with errno [%d]\n", errno);
    cleanup();
    exit(EXIT_FAILURE);
  }

  if(argc == 2 && (!strcmp(argv[1], "-d")))
  {
    if(daemon(0,0) < 0)
    {
      syslog(LOG_ERR, "daemon() failed with errno [%d]\n", errno);
    }
  }

  if(signal(SIGINT, signalHandler) == SIG_ERR)
  {
    syslog(LOG_ERR, "SIGINT");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if(signal(SIGTERM, signalHandler) == SIG_ERR)
  {
    syslog(LOG_ERR, "SIGTERM");
    cleanup();
    exit(EXIT_FAILURE);
  }

#ifndef USE_AESD_CHAR_DEVICE
  if(signal(SIGALRM, appendTimestamp) == SIG_ERR)
  {
    syslog(LOG_ERR, "SIGALRM");
    cleanup();
    exit(EXIT_FAILURE);
  }

  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 1;
  timer.it_interval.tv_sec = 10;
  timer.it_interval.tv_usec = 0;
  if(setitimer(ITIMER_REAL, &timer, NULL) == -1)
  {
    syslog(LOG_ERR, "setitimer() failed with errno [%d]\n", errno);
    cleanup();
    exit(EXIT_FAILURE);
  }
#endif

  if((listen(sfd, 10)) != 0)
  {
    syslog(LOG_ERR, "listen() failed with errno [%d]\n", errno);
    cleanup();
    exit(EXIT_FAILURE);
  }

  socklen_t addrlen = sizeof(addr);

  while(!gracefullyExit)
  {
    cfd = accept(sfd, (struct sockaddr *)&addr, &addrlen);

    if(cfd == -1)
    {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
        continue;
      }

      syslog(LOG_ERR, "accept() failed with errno [%d]\n", errno);
      cleanup();
      exit(EXIT_FAILURE);
    }

    if(inet_ntop(AF_INET, &(addr.sin_addr), ipaddress, INET_ADDRSTRLEN) == NULL)
    {
      syslog(LOG_ERR, "inet_ntop() failed with errno [%d]\n", errno);
      cleanup();
      exit(EXIT_FAILURE);
    }

    syslog(LOG_DEBUG, "Accepted connection from %s", ipaddress);

    threadData = (socketThreadData_t *)malloc(sizeof(socketThreadData_t));
    threadData->cfd = cfd;
    threadData->mutexExit = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(threadData->mutexExit, NULL);
    threadData->complete = false;
    threadData->exit = false;

    node = (slistData_t *)malloc(sizeof(slistData_t));
    node->thread = (pthread_t *)malloc(sizeof(pthread_t));
    node->threadData = threadData;
    node->clientAddr = addr.sin_addr;
    SLIST_INSERT_HEAD(&head, node, entries);

    pthread_create(node->thread, NULL, process, (void *)threadData);

    SLIST_FOREACH_SAFE(node, &head, entries, tempNode)
    {
      if(node->threadData->complete)
      {
        pthread_join(*(node->thread), NULL);
        inet_ntop(AF_INET, &(node->clientAddr), ipaddress, INET_ADDRSTRLEN);
        syslog(LOG_DEBUG, "Closed connection from %s", ipaddress);
        close(node->threadData->cfd);
        SLIST_REMOVE(&head, node, slistData, entries);
        free(node->threadData->mutexExit);
        free(node->threadData);
        free(node->thread);
        free(node);
      }
    }
  }

  SLIST_FOREACH(node, &head, entries)
  {
    pthread_mutex_lock(node->threadData->mutexExit);
    node->threadData->exit = true;
    pthread_mutex_unlock(node->threadData->mutexExit);
  }

  SLIST_FOREACH_SAFE(node, &head, entries, tempNode)
  {
    pthread_join(*(node->thread), NULL);
    inet_ntop(AF_INET, &(node->clientAddr), ipaddress, INET_ADDRSTRLEN);
    syslog(LOG_DEBUG, "Closed connection from %s", ipaddress);
    close(node->threadData->cfd);
    SLIST_REMOVE(&head, node, slistData, entries);
    free(node->threadData->mutexExit);
    free(node->threadData);
    free(node->thread);
    free(node);
  }

  cleanup();
  exit(EXIT_SUCCESS);
}
