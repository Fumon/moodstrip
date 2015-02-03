#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <err.h>

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
#define MAXPACKETS 10
#define BUFFER_SIZE MAXPACKETS

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

void PushQueue()
{
    end = (end + 1) % BUFFER_SIZE;

    if (active < BUFFER_SIZE)
    {
        active++;
    } else {
        /* Overwriting the oldest. Move start to next-oldest */
        start = (start + 1) % BUFFER_SIZE;
    }
}

struct item *RetrieveFromQueue(void)
{
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

int main() {
  struct sockaddr_in servAddr;
  struct sockaddr_in clientAddr;
  int servSock;
  int clientSock;
  int serialPort;
  fd_set reads;
  int avail;

  int timer = setup_timer();

  // Init serial port
  if((serialPort = serialport_init("/dev/ttyACM0", 115200)) < 0) {
    fprintf(stderr, "Error opening serial port\n");
    exit(1);
  }

  // Clear and set servAddr
  memset((void*)&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(SRVPORT);

  if((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    fprintf(stderr, "Encountered error opening socket\n");
    err(1, NULL);
    exit(1);
  }


  if(bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0) {
    fprintf(stderr, "Couldn't bind\n");
    err(1, NULL);
    exit(1);
  }

  if(listen(servSock, MAXPENDING) <0) {
    fprintf(stderr, "Cannot listen\n");
    exit(1);
  }

  printf("listening\n");
  uint8_t echoBuffer[RECVBUFFLEN];        /* Buffer for echo string */
  ssize_t recvMsgSize = 0;                    /* Size of received message */
  uint8_t partialBuf[RECVBUFFLEN];
  int  partialBufLen = 0;
  int parseret = 0;
  size_t parseplace = 0;
  uint8_t itembuf[PACKETLENGTH];
  unsigned long long missed;

  char sendBuf[10];

  for(;parseret >= 0;) {
    partialBufLen = 0;
    unsigned int clientLen = sizeof(clientAddr);

    if((clientSock = accept(servSock, (struct sockaddr *) &clientAddr, &clientLen)) < 0) {
      fprintf(stderr, "Couldn't accept\n");
      exit(1);
    }

    printf("Handling client\n");

    // CLEAR BUFFER
    free_packetbuffer();
    init_queue();

    /* Receive message from client */
    if((recvMsgSize = recv(clientSock, (void*)echoBuffer, sizeof(echoBuffer), 0)) < 0) {
      fprintf(stderr, "recv failed\n");
      exit(1);
    }

    /* Send received string and receive again until end of transmission */
    while(recvMsgSize > 0)      /* zero indicates end of transmission */
    {
      parseplace = 0;
      // While current recvbuffer has content
      do {
        // Parse
        if((parseret = parse_packet(&parseplace, echoBuffer, recvMsgSize, partialBuf,
            &partialBufLen, itembuf)) < 0) {
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

      if(parseret < 0)
        break;

#ifdef DEBUG
      fprintf(stderr, "%d packets in queue\n", active);
#endif
      // Send number of currently queued packets
      int sendlen = sprintf(sendBuf, "%d", active);
      if(send(clientSock, sendBuf, sendlen + 1, 0) != (sendlen + 1)) {
        exit(1);
      }

      for(;;) {
        // Select
        FD_ZERO(&reads);
        FD_SET(clientSock, &reads);
        FD_SET(timer, &reads);
        avail = select(FD_SETSIZE, &reads, (fd_set *) 0, (fd_set *) 0, NULL);

        if(avail) {
          if(FD_ISSET(timer, &reads) && active) {
            // Clear timer
            read(timer, &missed, sizeof(missed));

            while(missed-- && active) {
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
          }

          if(FD_ISSET(clientSock, &reads)) {
            /* See if there is more data to receive */
            if((recvMsgSize = recv(clientSock, (void*)echoBuffer, sizeof(echoBuffer), 0)) < 0) {
              err(1, "Send Failed");
              exit(1);
            }
            break;
          }
        }
      }
    }

    free_packetbuffer();
    close(clientSock);    /* Close client socket */
    fprintf(stderr, "Socket closed\n");
  }
  free_packetbuffer();
  fprintf(stderr, "Shutting down\n");
  return 0;
}
