/******************************************************************************
 * tcp_server.c
 *
 * CPE 464 - Program 1
 *****************************************************************************/

#include "server.h"
#include "cpe464.h"

void setChecksum (uint8_t *buffer,  int buffSize) {
   uint16_t checksum = 0;
   
   checksum = in_cksum((unsigned short *)buffer, buffSize);
   
   memcpy(&buffer[sizeof(uint32_t)], &checksum, sizeof(checksum));
}

void sendServerACK (int seqNum, int client_socket, int flag) {
   struct header ackHdr;
   int sent = 0;
   
   ackHdr.seqNum = htonl(seqNum);
   ackHdr.checksum = 0;
   ackHdr.flag = flag;
   
   setChecksum((uint8_t *)&ackHdr, sizeof(ackHdr));
   /* now send the data */
   sent =  send(client_socket, &ackHdr, sizeof(ackHdr), 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
}

void sendMSG (struct handleList *clientList, int clientCount, uint8_t *msg, uint8_t *destinationClientName, uint32_t messageLength) {
   int sent, clientIndex;
   int destinationSocket;
   
   for (clientIndex = 0; clientIndex < clientCount; clientIndex++) {
      if (strcmp((char *)destinationClientName, (char *)clientList[clientIndex].handleName) == 0) {
         destinationSocket = clientList[clientIndex].clientSocket;
      }
   }
   
   /* now send the data */
   sent =  send(destinationSocket, msg, messageLength, 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
}

void refuseConnection (struct header hdr, struct initPacket handle, 
                      struct handleList *clientList, int clientIndex, 
                      int clientCount, fd_set *fdvar) {
   uint8_t refuseConnectBuffer[sizeof(struct header) + sizeof(handle.idNum)];
   struct header returnHdr;
   int sent = 0;
   
   //reply packet,
   //Format: normal header, random number associated with the already existing handle
   returnHdr.seqNum = htonl(hdr.seqNum);
   returnHdr.checksum = 0;
   returnHdr.flag = 3;
   
   memcpy(refuseConnectBuffer, &returnHdr, sizeof(returnHdr));
   memcpy(&refuseConnectBuffer[sizeof(returnHdr)], &handle.idNum, sizeof(handle.idNum));
   
   setChecksum(refuseConnectBuffer, sizeof(returnHdr) + sizeof(handle.idNum));
   /* now send the data */
   sent =  send(clientList[clientIndex].clientSocket, &refuseConnectBuffer, 
                sizeof(returnHdr) + sizeof(handle.idNum), 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
   
   FD_CLR(clientList[clientIndex].clientSocket, fdvar);
   close(clientList[clientIndex].clientSocket);
   clientList[clientIndex].active = 0;
}

void determinePacketType (uint8_t *buffer, int message_len, int clientIndex, 
                         struct handleList *clientList, int clientCount, fd_set *fdvar) {
   struct header hdr;
   
   memcpy(&hdr, buffer, sizeof(hdr));
   hdr.seqNum = ntohl(hdr.seqNum);
   
   if (hdr.flag == RECV_HANDLE) {
      //Handle a newly connected client
      receiveHandle(buffer, hdr, clientIndex, clientList, clientCount, fdvar);
   }
   else if (hdr.flag == MSG_PACKET) {
      //Handle a newly received message, route it
      receiveMessage(buffer, message_len, hdr, clientIndex, clientList, clientCount, fdvar);
   }
   else if (hdr.flag == ACK_PACKET) {
      //Handle a newly received ACK, route it
      receiveACK(buffer, message_len, clientList, clientCount);
   }
   else if (hdr.flag == C_EXIT) {
      //flag 8 Client exit, send ACK
      sendServerACK(hdr.seqNum, clientList[clientIndex].clientSocket, C_EXIT);
   }
}

void receiveHandle (uint8_t *buffer, struct header hdr, int clientIndex, 
                   struct handleList *clientList, int clientCount, fd_set *fdvar) {
   struct initPacket handle;
   int i, handleState = 0;
   
   memcpy(&handle, &buffer[sizeof(struct header)], sizeof(handle));
   
   handle.idNum = ntohl(handle.idNum);
   
   handle.srcHandleName[handle.srcHandleLen] = '\0';
   
   for (i = 0; i < clientCount && !handleState; i++) {
      if (clientList[i].active &&
          strcmp((char *)handle.srcHandleName, (char *)clientList[i].handleName) == 0) {
         handleState = 1;
         if (clientList[i].idNum == handle.idNum) {
            handleState = 2;
         }
      }
   }
   
   if (handleState == 1) {
      //refuse connection client already exists
      refuseConnection(hdr, handle, clientList, clientIndex, clientCount, fdvar);
   }
   else if (handleState == 2) {
      //resend ACK
      sendServerACK(hdr.seqNum, clientList[clientIndex].clientSocket, CONF_GOOD_HANDLE);
   }
   else {
      //handle is good
      strcpy((char *)clientList[clientIndex].handleName, (char *)handle.srcHandleName);
      clientList[clientIndex].idNum = handle.idNum;
      clientList[clientIndex].active = 1;
      
      sendServerACK(hdr.seqNum, clientList[clientIndex].clientSocket, CONF_GOOD_HANDLE);
   }
   
}

void receiveMessage (uint8_t *buffer, int message_len, struct header hdr, int clientIndex, 
                    struct handleList *clientList, int clientCount, fd_set *fdvar) {
   struct message recMsg;
   int bufNdx = 0, existHandle = 0, i, txtLen, sent = 0;
   
   setRecMsg(recMsg, buffer);
   
   //flag 7 Check nonexistent handle name
   for (i = 0; i < clientCount; i++) {
      if (strcmp((char *)recMsg.destHandleName, (char *)clientList[i].handleName) == 0) {
         existHandle = 1;
      }
   }
   
   if (!existHandle) {
      struct header returnHdr;
      uint8_t missingHandleReplyBuffer[sizeof(returnHdr) + recMsg.destHandleLen + 1];
      int bufSiz = 0;
      
      returnHdr.seqNum = htonl(hdr.seqNum);
      returnHdr.checksum = 0;
      returnHdr.flag = MISSING_HANDLE;
      
      memcpy(&missingHandleReplyBuffer, &returnHdr, sizeof(returnHdr));
      bufSiz += sizeof(returnHdr);
      memcpy(&missingHandleReplyBuffer[bufSiz], &recMsg.destHandleLen, 1);
      bufSiz += 1;
      memcpy(&missingHandleReplyBuffer[bufSiz], &recMsg.destHandleName, recMsg.destHandleLen);
      bufSiz += recMsg.destHandleLen;
            
      setChecksum(missingHandleReplyBuffer, bufSiz);
      /* now send the data */
      sent =  send(clientList[clientIndex].clientSocket, &missingHandleReplyBuffer, bufSiz, 0);
      if(sent < 0) {
         perror("send call");
         exit(-1);
      }
   }
   else {
      sendMSG(clientList, clientCount, buffer, recMsg.destHandleName, message_len);
   }
}

void receiveACK (uint8_t *buffer, int message_len, struct handleList *clientList, 
                int clientCount) {
   struct ack ack;
   int bufSiz = sizeof(struct header);
   
   memcpy(&ack.ackSeqNum, &buffer[bufSiz], sizeof(ack.ackSeqNum));
   bufSiz += sizeof(ack.ackSeqNum);
   ack.ackSeqNum = ntohl(ack.ackSeqNum);
   
   memcpy(&ack.destHandleLen, &buffer[bufSiz], sizeof(ack.destHandleLen));
   bufSiz += sizeof(ack.destHandleLen);
   
   memcpy(&ack.destHandleName, &buffer[bufSiz], ack.destHandleLen);
   
   ack.destHandleName[ack.destHandleLen] = '\0';
   
   sendMSG(clientList, clientCount, buffer, ack.destHandleName, message_len);
}

int getReadyData (int *client_socket, int server_socket, fd_set *fdvar, struct handleList *clientList, int *clientCount, int *maxClientCount) {
   int message_len = 0;     //length of the received message
   int flag = 0;
   uint8_t buffer[MAX_BUFFER_SIZE];
   int clientIndex;
   
   for (clientIndex = 0; clientIndex < (*clientCount); clientIndex++) {
      if (clientList[clientIndex].active && FD_ISSET(clientList[clientIndex].clientSocket, fdvar)) {
         if (clientList[clientIndex].clientSocket == server_socket) {
            struct handleList newHandle;
            
            *client_socket = tcp_listen(server_socket, 5);
            FD_SET(*client_socket, fdvar);
            
            newHandle.idNum = 0;
            newHandle.clientSocket = *client_socket;
            newHandle.active = 1;
            addNewClientToList(newHandle, clientList, clientCount, maxClientCount);
         }
         else {
            flag = 1;
            uint16_t checksum;
            
            //get data onr eady socket
            message_len = recv(clientList[clientIndex].clientSocket, buffer, MAX_BUFFER_SIZE, 0);
            
            checksum = in_cksum((unsigned short *)buffer, message_len);
            
            if (message_len < 0)
            {
               perror("recv call");
               exit(-1);
            }
            else if (message_len == 0) {
               FD_CLR(clientList[clientIndex].clientSocket, fdvar);
               close(clientList[clientIndex].clientSocket);
               clientList[clientIndex].active = 0;
            }
            else if (checksum == 0) {
               determinePacketType(buffer, message_len, clientIndex, clientList, *clientCount, fdvar);
            }

         }
      }
   }
   
   return flag;
}

void addNewClientToList(struct handleList newClient, struct handleList *clientList, int *clientCount, int *maxClientCount) {
   clientList[(*clientCount)] = newClient;
   (*clientCount)++;
   
   if ((*clientCount) >= (*maxClientCount)) {
      struct handleList newClientList[(*maxClientCount) * 2];
      int i, j;
      for (i = 0, j = 0; i < (*clientCount); i++) {
         if (clientList[i].active == 1) {
            newClientList[j++] = clientList[i];
         }
      }
      
      clientList = newClientList;
      (*maxClientCount) *= 2;
   }
}

void serverInit(struct handleList *clientList, int *clientCount, 
                int *maxClientCount, int *client_socket, int server_socket) {
   struct handleList newHandle, serverHandle;
   int flag = 0;
   static struct timeval timeout;
   
   strcpy((char *)serverHandle.handleName, "Server");
   serverHandle.idNum = 0;
   serverHandle.clientSocket = server_socket;
   serverHandle.active = 1;
   addNewClientToList(serverHandle, clientList, clientCount, maxClientCount);
   
   newHandle.idNum = 0;
   newHandle.clientSocket = *client_socket;
   newHandle.active = 1;
   addNewClientToList(newHandle, clientList, clientCount, maxClientCount);
   
   while (1) {
      flag = 0;
      
      while (flag == 0) {
         fd_set fdvar;
         timeout.tv_sec = 1; //set timeout to 1 second
         timeout.tv_usec = 0; //set timeout (in micro-second)
         FD_ZERO(&fdvar);
         int i;
         for (i = 0; i < *clientCount; i++) {
            if (clientList[i].active) {
               FD_SET(clientList[i].clientSocket, &fdvar);
            }
         }
         
         if (select((*client_socket) + 1, (fd_set *) &fdvar, NULL, NULL, &timeout) < 0) {
            errno = 9; //bad file descriptor
         }
         
         flag = getReadyData(client_socket, server_socket, &fdvar, clientList, 
                             clientCount, maxClientCount);
      }
   }
}

int main(int argc, char *argv[])
{
   int server_socket = 0;   //socket descriptor for the server socket
   int client_socket = 0;   //socket descriptor for the client socket
   char *buf;              //buffer for receiving from client
   int buffer_size = 1024;  //packet size variable
   struct handleList clientList[20];
   int clientCount = 0;
   int maxClientCount = 20;
   double errorPercent;
   
   /* check command line arguments  */
   if(argc!= 2)
   {
      printf("usage: %s percent-error\n", argv[0]);
      exit(1);
   }
   
   printf("sockaddr: %u sockaddr_in %u\n", sizeof(struct sockaddr), sizeof(struct sockaddr_in));
   
   //create packet buffer
   buf=  (char *) malloc(buffer_size);
   
   errorPercent = atof(argv[1]);
   
   sendErr_init(errorPercent, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);

   //create the server socket
   server_socket = tcp_recv_setup();
   
   //look for a client to server
   client_socket = tcp_listen(server_socket, 5);

   serverInit(clientList, &clientCount, &maxClientCount, &client_socket, server_socket);
   
   /* close the sockets */
   close(server_socket);
   
   return 1;
}

/* This function sets the server socket.  It lets the system
   determine the port number.  The function returns the server
   socket number and prints the port number to the screen.  */

int tcp_recv_setup()
{
    int server_socket= 0;
    struct sockaddr_in local;      /* socket address for local side  */
    socklen_t len= sizeof(local);  /* length of local address        */

    /* create the socket  */
    server_socket= socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0)
    {
       perror("socket call");
       exit(1);
    }

    local.sin_family= AF_INET;         //internet family
    local.sin_addr.s_addr= INADDR_ANY; //wild card machine address
    local.sin_port= htons(0);                 //let system choose the port

    /* bind the name (address) to a port */
    if (bind(server_socket, (struct sockaddr *) &local, sizeof(local)) < 0)
    {
       perror("bind call");
       exit(-1);
    }
    
    //get the port name and print it out
    if (getsockname(server_socket, (struct sockaddr*)&local, &len) < 0)
    {
       perror("getsockname call");
       exit(-1);
    }

    fprintf(stdout, "socket has port %d \n", ntohs(local.sin_port));
	        
    return server_socket;
}

/* This function waits for a client to ask for services.  It returns
   the socket number to service the client on.    */

int tcp_listen(int server_socket, int back_log)
{
  int client_socket = 0;
  if (listen(server_socket, back_log) < 0)
  {
     perror("listen call");
     exit(-1);
  }
  
  if ((client_socket = accept(server_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0)
  {
     perror("accept call");
     exit(-1);
  }
  
  return(client_socket);
}

