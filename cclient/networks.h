#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define MAX_HANDLE_SIZE 101
#define MAX_TXT_SIZE 1024
#define MAX_BUFFER_SIZE 1233

//flags
#define RECV_HANDLE 1
#define SEND_HANDLE 1
#define CONF_GOOD_HANDLE 2
#define ERR_INIT_PACKET 3
#define MSG_PACKET 6
#define MISSING_HANDLE 7
#define C_EXIT 8
#define ACK_PACKET 255

//7 bytes
struct header {
   uint32_t seqNum;
   uint16_t checksum;
   uint8_t flag;
}__attribute__((packed));

//1226 bytes
struct message {
   uint8_t destHandleLen;
   uint8_t destHandleName[MAX_HANDLE_SIZE]; //100 bytes
   uint8_t srcHandleLen;
   uint8_t srcHandleName[MAX_HANDLE_SIZE]; //100 bytes
   uint8_t txtMsg[MAX_TXT_SIZE]; //1024 bytes
}__attribute__((packed));

//105 bytes
struct initPacket {
   uint32_t idNum;
   uint8_t srcHandleLen;
   uint8_t srcHandleName[MAX_HANDLE_SIZE];
}__attribute__((packed));

struct ack {
   uint32_t ackSeqNum;
   uint8_t destHandleLen;
   uint8_t destHandleName[MAX_HANDLE_SIZE]; //100 bytes
   uint8_t srcHandleLen;
   uint8_t srcHandleName[MAX_HANDLE_SIZE]; //100 bytes
}__attribute__((packed));

struct handleList {
   uint8_t handleName[MAX_HANDLE_SIZE];
   uint32_t idNum;
   uint32_t clientSocket;
   uint8_t active;
};

// for the server side
int tcp_recv_setup();
int tcp_listen(int server_socket, int back_log);

// for the client side
int tcp_send_setup(char *host_name, char *port);
