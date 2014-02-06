/*
 *  server.h
 *  Server
 *
 *  Created by Minh Nguyen on 10/18/12.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "networks.h"

#define setRecMsg(x, y) {\
memcpy(&(x), &(y)[sizeof(hdr)], sizeof((x).destHandleLen));\
bufNdx += sizeof(hdr) + sizeof((x).destHandleLen);\
\
memcpy(&((x).destHandleName), &(y)[bufNdx], (x).destHandleLen);\
bufNdx += (x).destHandleLen;\
\
memcpy(&((x).srcHandleLen), &(y)[bufNdx], sizeof((x).srcHandleLen));\
bufNdx += sizeof(recMsg.srcHandleLen);\
\
memcpy(&((x).srcHandleName), &(y)[bufNdx], (x).srcHandleLen);\
bufNdx += (x).srcHandleLen;\
\
txtLen = message_len - bufNdx;\
memcpy(&((x).txtMsg), &(y)[bufNdx], txtLen);\
bufNdx += txtLen;\
\
(x).destHandleName[(x).destHandleLen] = '\0';\
(x).srcHandleName[(x).srcHandleLen] = '\0';\
(x).txtMsg[txtLen] = '\0';\
}

//Initializes and starts the server
void serverInit(struct handleList *clientList, int *clientCount, 
                int *maxClientCount, int *client_socket, int server_socket);

//Checks all connected sockets for active events
int getReadyData (int *client_socket, int server_socket, fd_set *fdvar, 
                 struct handleList *clientList, int *clientCount, int *maxClientCount);

//Adds a new client to the list of clients
void addNewClientToList (struct handleList newClient, struct handleList *clientList, 
                        int *clientCount, int *maxClientCount);

//Sends a ACK from server to client, not client to client i.e: Handle registration, Client exits
void sendServerACK (int seqNum, int client_socket, int flag);

//Sends a pre-created message(packet) to a given client
void sendMSG (struct handleList *clientList, int clientCount, uint8_t *msg, uint8_t *destinationClientName, uint32_t messageLength);

//Calculates the checksum and places it in the checksum field in the header
void setChecksum (uint8_t *buffer, int buffSize);

//Handles the receival of a new handle
//Checks if the handle already exists, if an ACK needs to be resent,
//or adds the good handle to the Client List
void receiveHandle (uint8_t *buffer, struct header hdr, int clientIndex, 
                   struct handleList *clientList, int clientCount, fd_set *fdvar);

//Refuse the connection when client already exists
void refuseConnection (struct header hdr, struct initPacket handle, 
                      struct handleList *clientList, int clientIndex, 
                      int clientCount, fd_set *fdvar);

//Handles a newly received message packet from a client
void receiveMessage (uint8_t *buffer, int message_len, struct header hdr, int clientIndex, 
                    struct handleList *clientList, int clientCount, fd_set *fdvar);

//Handles a newly received ACK packet
void receiveACK (uint8_t *buffer, int message_len, struct handleList *clientList, 
                int clientCount);

//Determine the packet type 
//Calls other functions to handle them according to their type
void determinePacketType (uint8_t *buffer, int message_len, int clientIndex, 
                    struct handleList *clientList, int clientCount, fd_set *fdvar);