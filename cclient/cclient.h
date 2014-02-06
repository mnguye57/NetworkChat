/*
 *  cclient.h
 *  cclient
 *
 *  Created by Minh Nguyen on 10/18/12.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "networks.h"

//count the used bytes of an array x and store into y
#define arrayConsumption(x, y) {\
for((y) = 0; (x)[(y)] != 0; (y)++);\
}

#define unlock() {\
*(stats->result) = 1;\
pthread_cond_signal(&conditionVariable);\
*(stats->msgSent) = 1;\
}

struct status {
   uint8_t *send_buf;
   uint32_t *currSeqNum;
   uint32_t lastMsgSeqNum;
   uint32_t *buffSize;
   int socket_num;
   uint8_t *result;
   uint32_t *resendCount;
   int *initHandleAckStatus;
   int *msgSent;
   int *quit;
   int *quitStatus;
   double errorPercent;
};

struct handle {
   uint8_t handleName[MAX_HANDLE_SIZE];
   uint32_t lastSeqNum;
};

//listen to commands
void listenCommands (int socket_num, uint32_t *seqNum, uint8_t *handleName, 
                    struct status *stat);

//Send the handle information to the server
//returns 1 if error connecting to server, else return 0
void sendInitHandleInfo (uint8_t *handleName, uint32_t *seqNum, uint8_t *send_buf, 
                        int socket_num, struct status *stat);

//Listens for incoming messages and ACKs
void *listenMSG (void *stat);

//Checks the set of socket numbers for new events
void checkSocketSet (struct status *stats, fd_set *fdvar, uint8_t *buffer, 
                    struct handle *clientList, int *clientCount, int *maxClientCount);

//Lock pthread until ACK is received
void lockThread (struct status *stat);

//Sends an ACK for a received message
void sendACK (uint32_t *thisSeqNum, uint32_t replySeqNum, struct status *stat,
             struct message newMsg);

//Gets the destination handle name from buffer
void getDestHandleName (uint8_t *storage, uint8_t *buffer, int *bufNdx);

//Gets the source handle name from buffer
void getSrcHandleName (uint8_t *storage, uint8_t *buffer, int *bufNdx);

//Calculates the checksum and places it in the checksum field in the header
void setChecksum (uint8_t *buffer, int buffSize);

//Adds a new client to the list of clients
void addNewClientToList (struct handle newClient, struct handle *clientList, 
                        int *clientCount, int *maxClientCount);

//Checks if a client is in the table and compares the sequence number of the client
//with the sequence number of the newly received packet, and adds the client if
//it does not exist in the list
int searchClientList (uint8_t *srcHandleName, uint32_t newSeqNum, 
                     struct handle *clientList, int *clientCount, int *maxClientCount);

//Determines the type of packet received
void determinePacketType (uint8_t *buffer, int message_len, struct status *stat, 
                          struct handle *clientList, int *clientCount, int *maxClientCount);

//Handle a newly received packet
void receivedMessage (uint8_t *buffer, int message_len, int bufNdx, 
                      struct handle *clientList, int *clientCount, 
                      int *maxClientCount, struct status *stats, struct header returnHdr);

//Handles the event of a nonexistant handle name for a message
void nonExistantHandle (uint8_t *buffer, int bufNdx, struct status *stats);

//Handles the quitting of the client
void clientQuit (int socket_num, uint32_t *seqNum, struct status *stat);

//Creates a message packet to be sent
int createMessage (struct message *msg, uint8_t *send_buf, uint32_t *seqNum, 
                   struct status *stat, int *bufNdx, uint8_t *handleName, 
                   int *send_len, struct header *hdr);