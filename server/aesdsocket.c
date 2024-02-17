#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const static char *kPort = "9000";
const static char *kSocketData = "/var/tmp/aesdsocketdata";
const static int kBufferSize = 100000;

static int sfd = 0;
static int cfd = 0;
static int fd = 0;

static void signal_handler(int signo)
{
  syslog(LOG_INFO, "Caught signal, exiting.");
  shutdown(sfd, SHUT_RDWR);
  close(sfd);
  close(fd);
  unlink(kSocketData);
  close(cfd);
  closelog();
  exit(0);
}


int main(int argc, char *argv[]) 
{
  struct addrinfo hints;
  struct addrinfo *result;
  struct sockaddr_in addr;

  openlog(argv[0], LOG_PID, LOG_USER);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_INET;     
  hints.ai_socktype = SOCK_STREAM; 
  hints.ai_flags = AI_PASSIVE;   
  hints.ai_protocol = 0;        
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  int status = getaddrinfo(NULL, kPort, &hints, &result);

  if(status != 0)
  {
    syslog(LOG_ERR, "getaddrinfo: [%s]\n", gai_strerror(status));
    closelog();
    return -1;
  }

  sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

  if(sfd == -1)
  {
    syslog(LOG_ERR, "socket() failed with errno [%d]\n", errno);
    freeaddrinfo(result);
    closelog();
    return -1;
  }

  const int opt = 1;
  if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
  {
    syslog(LOG_ERR, "setsockopt() failed with errno [%d]\n", errno);
    freeaddrinfo(result);
    closelog();
    return -1;
  }

  if(bind(sfd, result->ai_addr, result->ai_addrlen) != 0)
  {
    syslog(LOG_ERR, "bind() failed with errno [%d]\n", errno);
    freeaddrinfo(result);
    closelog();
    return -1;
  }

  if(argc == 2 && (!strcmp(argv[1],"-d")))
  {
    if(daemon(0,0) < 0)
    {
      syslog(LOG_ERR, "daemon() failed with errno [%d]\n", errno);
    }
  }

  freeaddrinfo(result);

  listen(sfd, 10);

  if(signal(SIGINT, signal_handler) == SIG_ERR)
  {
    syslog(LOG_ERR,"SIGINT");
    closelog();
  }
  if(signal(SIGTERM, signal_handler) == SIG_ERR)
  {
    syslog(LOG_ERR,"SIGTERM");
    closelog();
  }

  fd = open(kSocketData, O_RDWR | O_CREAT | O_APPEND, 0777);

  if(fd < 0)
  {
    syslog(LOG_ERR, "open() failed with errno [%d]\n", errno);
    closelog();
  } 
  
  while(1)
  {
    char ipaddress[INET_ADDRSTRLEN];
    bzero(ipaddress, INET_ADDRSTRLEN);

    char buffer[kBufferSize];
    bzero(buffer, kBufferSize);
    ssize_t bytesRecieved = 0;
    ssize_t bytesSent = 0;

    socklen_t addrlen = sizeof(addr);
    cfd = accept(sfd, (struct sockaddr *)&addr, &addrlen);

    if(cfd == -1)
    {
      syslog(LOG_ERR, "accept() failed with errno [%d]\n", errno);
      raise(SIGTERM);
      return -1;
    }

    if(inet_ntop(AF_INET, &(addr.sin_addr), ipaddress, INET_ADDRSTRLEN) == NULL)
    {
      syslog(LOG_ERR, "inet_ntop() failed with errno [%d]\n", errno);
      raise(SIGTERM);
      return -1;
    }

    syslog(LOG_DEBUG, "Accepted connection from [%s]\n", ipaddress);

    bytesRecieved = recv(cfd, buffer, kBufferSize, 0);

    if(bytesRecieved == -1)
    {
      syslog(LOG_ERR, "recv() failed with errno [%d]\n", errno);
      raise(SIGTERM);
      return -1;
    }

    if(write(fd, buffer, bytesRecieved) == -1)
    {
      syslog(LOG_ERR, "write() failed with errno [%d]\n", errno);
      raise(SIGTERM);
      return -1;
    }

    if(lseek(fd, 0, SEEK_SET) == -1)
    {
      syslog(LOG_ERR, "lseek() failed with errno [%d]\n", errno);
      raise(SIGTERM);
      return -1;
    }

    bzero(buffer, kBufferSize);
    while(1)
    {
      bytesSent = read(fd, buffer, kBufferSize); 

      if(bytesSent == -1)
      {
        syslog(LOG_ERR, "read() failed with errno [%d]\n", errno);
        raise(SIGTERM);
        return -1;
      }

      bytesSent = send(cfd, buffer, bytesSent, 0);

      if(bytesSent == -1)
      {
        syslog(LOG_ERR, "send() failed with errno [%d]\n", errno);
        raise(SIGTERM);
        return -1;
      }

      break;
    }

    syslog(LOG_DEBUG, "Closed connection from [%s]\n", ipaddress);
  }

  return 0;
}
