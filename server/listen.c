#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

#include <signal.h>
#include <sys/signalfd.h>

#include <sys/epoll.h>

#include <sys/timerfd.h>

#include "arduino-serial-lib.h"

#define nLEDS 92
#define DEBUG

#define PACKETLENGTH ((nLEDS*3) + 1)

#define MAGICBYTE 0x0F
#define COMMANDBYTE 0x84

#define SRVPORT 9996
#define MAXPENDING 1

#define RECVBUFFLEN (PACKETLENGTH)
#define MAXPACKETS 15
#define BUFFER_SIZE MAXPACKETS

#define MAX_EVENTS 3

struct item {
  uint8_t buf[PACKETLENGTH];
};

struct item *buffer[BUFFER_SIZE];
int start = 0;
int end = 0;
int active = 0;

void init_queue() {
  int i;
  for(i = 0; i < BUFFER_SIZE; ++i) {
    // Make a new item
    struct item *newitem;
    if((newitem = (struct item*)malloc(sizeof(struct item))) == NULL) {
      err(1, "Problem allocating memory: ");
      exit(1);
    }
    buffer[i] = newitem;
  }
  
}

void PushQueue() {
    end = (end + 1) % BUFFER_SIZE;

    if (active < BUFFER_SIZE)
    {
        active++;
    } else {
        /* Overwriting the oldest. Move start to next-oldest */
        start = (start + 1) % BUFFER_SIZE;
    }
}

struct item *RetrieveFromQueue(void) {
    struct item *p;

    if (!active) { return NULL; }

    p = buffer[start];
    start = (start + 1) % BUFFER_SIZE;

    active--;
    return p;
}

void free_packetbuffer() {
    struct item *tempitem;
    while((tempitem = RetrieveFromQueue()) != NULL) {
      free((void*)tempitem);
    }
}


// TODO: Error codes
int parse_packet(size_t *parseplace, uint8_t *recvBuf, const size_t recvlen, uint8_t *partial, int *partialLength, uint8_t *output) {
  // NOTES: Partial will always begin with MAGICBYTE if it contains anything
  uint8_t *parseloc = &(recvBuf[*parseplace]);
  size_t parselen = recvlen - *parseplace;
  uint8_t *mbyteloc;
  // Find next magicbyte
  if(parseloc[0] == MAGICBYTE) {
    if(*partialLength > 0) {
      // This means we've fallen out of sync
      fprintf(stderr, "Out of sync: magic byte at beginning but partialLength > 0\n");
      return -1;
    }

    if(parselen >= PACKETLENGTH) {
      // Copy item buffer data
      memcpy(output, parseloc, PACKETLENGTH);

      if(parselen == PACKETLENGTH) {
        return 2;
      }
      // Update counts
      *parseplace = *parseplace + PACKETLENGTH;
      return 1;
    } else if(parselen < PACKETLENGTH) {
      // Partial
      memcpy(partial, parseloc, parselen);
      // Update counts
      *parseplace = 0;
      *partialLength = parselen;

      return 0;
    }
  } else if(*partialLength == 0) {
    // This means we've fallen out of sync
    fprintf(stderr, "Out of sync: magic byte at loc > 0 but nothing in partial buffer\n");
    return -1;
  } else {
    mbyteloc = (uint8_t *)memchr(parseloc, MAGICBYTE, parselen);
    if(mbyteloc == NULL) {
      // Partial packet

      // Copy memory
      memcpy(partial + *partialLength, parseloc, parselen);
      // Update counts
      *partialLength += parselen;

      if(*partialLength == PACKETLENGTH) {
        memcpy(output, partial, PACKETLENGTH);
        *partialLength = 0;
        return 1;
      } else {
        return 0;
      }
    } else {
      // Beginning of buffer must complete a partial packet
      size_t partlen = mbyteloc - parseloc;

      // Check that we have a full packet when combining with partial
      if((*partialLength + partlen) < PACKETLENGTH) {
        // We've lost bytes
        fprintf(stderr, "Out of sync: Not enough bytes between partial and beginning of parse\n");
        return -1;
      }

      // Copy memory
      memcpy(output, partial, *partialLength);
      memcpy(output+(*partialLength), parseloc, partlen);

      // Update locations
      *parseplace += partlen;
      *partialLength = 0;
      return 1;
    }
  }
  return 0;
}

int setup_timer() {
  int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

  struct timespec interval = {
    .tv_sec = 0,
    .tv_nsec = 40000000
  };

  struct timespec expiration = {
    .tv_sec = 0,
    .tv_nsec = 40000000
  };

  struct itimerspec ispec = {
    .it_interval = interval,
    .it_value = expiration
  };

  timerfd_settime(fd, 0, &ispec, NULL);

  return fd;
}

int setup_server_socket() {
  struct sockaddr_in servAddr;
  int servSock;
  // Clear and set servAddr
  memset((void*)&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(SRVPORT);

  if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    err(1, "Encountered error opening socket");
    return -1;
  }


  if(bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
    err(1, "Couldn't bind");
    return -1;
  }

  if(listen(servSock, MAXPENDING) <0) {
    err(1, "Couldn't Listen");
    return -1;
  }

  fprintf(stdout, "Listening\n");
  return servSock;
}

void send_queue_length(int clientSock) {
      char sendBuf[10];
      // Send number of currently queued packets
      int sendlen = sprintf(sendBuf, "%d", active);
      if(send(clientSock, sendBuf, sendlen + 1, 0) != (sendlen + 1)) {
        exit(1);
      }
}

void write_to_serial(int serialPort) {
  // Retrieve next packet
  struct item *d = RetrieveFromQueue();
  // Replace magicbyte
  d->buf[0] = COMMANDBYTE;

  // Write to arduino
  int n = write(serialPort, d->buf, PACKETLENGTH);
  if(n != PACKETLENGTH) {
    err(1, "Could not write complete string");
    exit(1);
  }
}

void makeSocketNonBlocking(int fd) {
  int flags;
 
  flags = fcntl(fd, F_GETFL, NULL);

  if(-1 == flags) {
    perror("fcntl F_GETFL failed");
    exit(1);
  }

  flags |= O_NONBLOCK;

  if(-1 == fcntl(fd, F_SETFL, flags)) {
    perror("fcntl F_SETFL failed");
    exit(1);
  }       
}

int main() {
  struct sockaddr_in clientAddr;
  int servSock;
  int clientSock = -1;
  int close_conn = 0;
  ssize_t recvLen = 0;
  uint8_t recvBuf[RECVBUFFLEN];

  int  partialBufLen = 0;
  int parseret = 0;
  size_t parseplace = 0;
  uint8_t partialBuf[RECVBUFFLEN];
  uint8_t itembuf[PACKETLENGTH];

  int serialPort;

  sigset_t sigset;
  int killSig;
  int kill = 0;

  // Epoll Vars
  int epollfd;
  int nfds, n;
  struct epoll_event ev, events[MAX_EVENTS];

  unsigned int missed = 0;
  int timer = setup_timer();

  // Initialize socket
  if((servSock = setup_server_socket()) < 0) {
    exit(1);
  }

  // Init serial port
  if((serialPort = serialport_init("/dev/ttyACM0", 115200)) < 0) {
    fprintf(stderr, "Error opening serial port\n");
    exit(1);
  }

  // Init signal handler
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGINT);
  if(sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
    perror("Couldn't mask signals");
    exit(1);
  }

  if((killSig = signalfd(-1, &sigset, 0)) < 0) {
    perror("Couldn't create signal fd");
    exit(1);
  }
    
  // Epoll
  if((epollfd = epoll_create1(0)) < 0) {
    perror("epoll_create1");
    exit(1);
  }

  // Add server socket
  ev.events = EPOLLIN;
  ev.data.fd = servSock; // For context
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, servSock, &ev) < 0) {
    perror("epoll_ctl: servSock");
  }

  // Add Timer
  ev.events = EPOLLIN;
  ev.data.fd = timer;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, timer, &ev) < 0) {
    perror("epoll_ctl: timer");
  }

  // Add serial
  ev.events = EPOLLOUT;
  ev.data.fd = serialPort;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, serialPort, &ev) < 0) {
    perror("epoll_ctl: serialPort");
  }

  // Add signal
  ev.events = EPOLLIN;
  ev.data.fd = killSig;
  if(epoll_ctl(epollfd, EPOLL_CTL_ADD, killSig, &ev) < 0) {
    perror("epoll_ctl: timer");
  }

  for(;kill == 0;) {
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if(nfds == -1) {
      perror("epoll_wait");
      exit(1);
    }

    for(n = 0; n < nfds; ++n) {
      int curfd = events[n].data.fd;
      if(curfd == servSock) {
        // New Connection
        unsigned int clientLen = sizeof(clientAddr);

        if((clientSock = accept4(servSock,
                (struct sockaddr *) &clientAddr, &clientLen, SOCK_NONBLOCK | SOCK_CLOEXEC)) < 0) {
          perror("Accept");
          exit(1);
        }
        printf("Connection Accepted\n");

        // Add to events
        ev.events = EPOLLIN;
        ev.data.fd = clientSock;
        if(epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSock, &ev) < 0) {
          perror("epoll_ctl: Adding Client");
        }
        
        // Now prepare for new data
        free_packetbuffer();
        init_queue();
        close_conn = 0;
        // Reset partial buffer
        partialBufLen = 0;
      } else if(curfd == timer) {
        // Time to push to serial
        // Add number of events
        unsigned int mtmp; // TODO: Try changing this to a smaller type
        read(timer, &mtmp, sizeof(mtmp));
        missed += mtmp;
      } else if(curfd == serialPort) {
        // Check if we're to write yet
        if(active) { // TODO: Maybe just framedrop?
          do {
            write_to_serial(serialPort);
          } while(active > 0);
        }
      } else if(curfd == killSig) {
        // Got a kill signal
        kill = 1;
      } else if(curfd == clientSock) {
        // Receive data
        parseplace = 0;
        do {
          recvLen = read(clientSock, (void *)recvBuf, sizeof(recvBuf));
          if(recvLen < 0) {
            if(errno != EWOULDBLOCK) {
              perror("recv() failed");
              // Close connection
              close_conn = 1;
            }
            break;
          }

          if((int)recvLen == 0) {
            // Close connection
            fprintf(stderr, "No data\n");
            close_conn = 1;
            break;
          }

          // Data
          do {
            // Parse
            if((parseret = parse_packet(&parseplace, recvBuf, recvLen,
                    partialBuf, &partialBufLen, itembuf)) < 0) {
              fprintf(stderr, "Parse failed\n");
              break;
            }

            // If we have a packet,
            if(parseret >= 1) {
              // Copy buffer contents
              memcpy(&(buffer[end]->buf), &itembuf, PACKETLENGTH);
              // Push it into the queue
              PushQueue();
            }

            if(parseret == 2) {
              break;
            }
          } while(parseret > 0);

          if(parseret == -1) {
            close_conn = 1;
            break;
          }
        } while(1);
        send_queue_length(clientSock);
        fprintf(stderr, "Packets: %d\n", active);

        if(close_conn) {
          printf("Connection closed\n");
          epoll_ctl(epollfd, EPOLL_CTL_DEL, clientSock, NULL);
          close(clientSock);
        }
      }
    }
  }
  free_packetbuffer();
  fprintf(stderr, "Shutting down\n");
  // Close stuff
  close(clientSock);
  shutdown(servSock, SHUT_RDWR);
  close(servSock);
  close(timer);
  close(serialPort);
  return 0;
}
