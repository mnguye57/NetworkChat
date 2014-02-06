/******************************************************************************
 * tcp_client.c
 *
 *****************************************************************************/

#include "cclient.h"
#include "cpe464.h"

pthread_mutex_t mutex_status = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t conditionVariable = PTHREAD_COND_INITIALIZER;

void setChecksum(uint8_t *buffer,  int buffSize) {
   uint16_t checksum = 0;
   
   checksum = in_cksum((unsigned short *)buffer, buffSize);
   
   memcpy(&buffer[sizeof(uint32_t)], &checksum, sizeof(checksum));
}

void sendACK(uint32_t *thisSeqNum, uint32_t replySeqNum, struct status *stat, 
             struct message newMsg) {
   struct header hdr;
   struct ack ack;
   uint8_t buffer[sizeof(hdr) + sizeof(ack)];
   uint32_t bufSiz = 0;
   int sent = 0;
   
   hdr.seqNum = htonl((*thisSeqNum)++);
   hdr.checksum = 0;
   hdr.flag = ACK_PACKET;
   
   ack.ackSeqNum = htonl(replySeqNum);
   ack.destHandleLen = newMsg.destHandleLen;
   strcpy((char *)ack.destHandleName, (char *)newMsg.srcHandleName);
   ack.srcHandleLen = newMsg.srcHandleLen;
   strcpy((char *)ack.srcHandleName, (char *)newMsg.destHandleName);
   
   memcpy(&buffer, &hdr, sizeof(hdr));
   bufSiz += sizeof(hdr);
   memcpy(&buffer[bufSiz], &ack.ackSeqNum, sizeof(ack.ackSeqNum));
   bufSiz += sizeof(ack.ackSeqNum);
   memcpy(&buffer[bufSiz], &ack.destHandleLen, sizeof(ack.destHandleLen));
   bufSiz += sizeof(ack.destHandleLen);
   memcpy(&buffer[bufSiz], &ack.destHandleName, ack.destHandleLen);
   bufSiz += ack.destHandleLen;
   memcpy(&buffer[bufSiz], &ack.srcHandleLen, sizeof(ack.srcHandleLen));
   bufSiz += sizeof(ack.srcHandleLen);
   memcpy(&buffer[bufSiz], &ack.srcHandleName, ack.srcHandleLen);
   bufSiz += ack.srcHandleLen;
   
   setChecksum(buffer, bufSiz);
   sent =  send(stat->socket_num, buffer, bufSiz, 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
   
   *(stat->resendCount) = 0;
   
}

void getDestHandleName(uint8_t *storage, uint8_t *buffer, int *bufNdx) {
   uint8_t destHandleLen;
   
   memcpy(storage, &buffer[*bufNdx], 1);
   memcpy(&destHandleLen, &buffer[*bufNdx], 1);
   *bufNdx += 1;
   
   memcpy(&storage[1], &buffer[*bufNdx], destHandleLen);
   storage[destHandleLen + 1] = '\0';
   *bufNdx += destHandleLen;
}

void getSrcHandleName(uint8_t *storage, uint8_t *buffer, int *bufNdx) {
   uint8_t srcHandleLen;
   
   memcpy(storage, &buffer[*bufNdx], 1);
   memcpy(&srcHandleLen, &buffer[*bufNdx], 1);
   *bufNdx += 1;
   
   memcpy(&storage[1], &buffer[*bufNdx], srcHandleLen);
   storage[srcHandleLen + 1] = '\0';
   *bufNdx += srcHandleLen;
}

void lockThread(struct status *stat) {
   pthread_mutex_lock(&mutex_status);
   
   while ((int)*(stat->result) == 0) {
      pthread_cond_wait(&conditionVariable, &mutex_status);
   }
   
   pthread_mutex_unlock(&mutex_status);
}

void addNewClientToList(struct handle newClient, struct handle *clientList, 
                        int *clientCount, int *maxClientCount) {
   clientList[(*clientCount)] = newClient;
   (*clientCount)++;
   
   if ((*clientCount) >= (*maxClientCount)) {
      struct handle newClientList[(*maxClientCount) * 2];
      int i, j;
      for (i = 0, j = 0; i < (*clientCount); i++) {
            newClientList[j++] = clientList[i];
      }
      
      clientList = newClientList;
      (*maxClientCount) *= 2;
   }
}

int searchClientList(uint8_t *srcHandleName, uint32_t newSeqNum, struct handle *clientList, int *clientCount, int *maxClientCount) {
   int clientIndex;
   int result = 0;
   
   for (clientIndex = 0; clientIndex < *clientCount; clientIndex++) {
      if (strcmp((char *)srcHandleName, (char *)clientList[clientIndex].handleName) == 0) {
         result = 1;
         if (newSeqNum >= clientList[clientIndex].lastSeqNum) {
            result = 2;
            clientList[clientIndex].lastSeqNum = newSeqNum + 1;
         }
      }
   }
   
   if (result == 0) {
      struct handle newClient;
      strcpy((char*)newClient.handleName, (char*)srcHandleName);
      newClient.lastSeqNum = newSeqNum + 1;
      
      addNewClientToList(newClient, clientList, clientCount, maxClientCount);
      
      result = 2;
   }
   
   return result;
}

void nonExistantHandle (uint8_t *buffer, int bufNdx, struct status *stats) {
   uint8_t destHandleLen;
   uint8_t destHandleName[MAX_HANDLE_SIZE];
   
   memcpy(&destHandleLen, &buffer[bufNdx], 1);
   bufNdx += 1;
   
   memcpy(destHandleName, &buffer[bufNdx], destHandleLen);
   destHandleName[destHandleLen] = '\0';
   
   fprintf(stdout, "\nClient with handle %s does not exist.\n", destHandleName);
   unlock();
}

void receivedMessage (uint8_t *buffer, int message_len, int bufNdx, 
                      struct handle *clientList, int *clientCount, 
                      int *maxClientCount, struct status *stats, struct header returnHdr) {
   struct message newMsg;
   int txtLen;
   int result;
   
   getDestHandleName((uint8_t *)&newMsg, buffer, &bufNdx);
   getSrcHandleName((uint8_t *)&newMsg.srcHandleLen, buffer, &bufNdx);
   
   txtLen = message_len - bufNdx;
   memcpy(&newMsg.txtMsg, &buffer[bufNdx], txtLen);
   newMsg.txtMsg[txtLen] = '\0';
   
   result = searchClientList(newMsg.srcHandleName, returnHdr.seqNum, 
                             clientList, clientCount, maxClientCount);
   if (result == 2) {
      fprintf(stdout, "\n%s: %s\n", newMsg.srcHandleName, newMsg.txtMsg);
      fprintf(stdout, "> ");
      fflush(stdout);
   }
   
   sendACK(stats->currSeqNum, returnHdr.seqNum, stats, newMsg);
   (*(stats->currSeqNum))++;   
}

void determinePacketType (uint8_t *buffer, int message_len, struct status *stats,
                          struct handle *clientList, int *clientCount, int *maxClientCount) {
   struct header returnHdr;
   int bufNdx = 0;
   uint8_t idNumExistingHandle = 0;
   
   memcpy(&returnHdr, buffer, sizeof(returnHdr));
   returnHdr.seqNum = ntohl(returnHdr.seqNum);
   
   bufNdx += sizeof(returnHdr);
   
   if (returnHdr.flag == CONF_GOOD_HANDLE) {
      unlock();
      *(stats->initHandleAckStatus) = 0;
   }
   else if (returnHdr.flag == 3) {
      memcpy(&idNumExistingHandle, &buffer[bufNdx], sizeof(idNumExistingHandle));
      unlock();
      *(stats->quit) = 1;
   }
   else if (returnHdr.flag == C_EXIT) {
      unlock();
      *(stats->quit) = 1;
   }
   else if (returnHdr.flag == MISSING_HANDLE) {
      nonExistantHandle(buffer, bufNdx, stats);
   }
   else if (returnHdr.flag == ACK_PACKET) {
      uint32_t lastMsgSeqNum;
      
      memcpy(&lastMsgSeqNum, &buffer[bufNdx], sizeof(lastMsgSeqNum));
      
      lastMsgSeqNum = ntohl(lastMsgSeqNum);
      if (lastMsgSeqNum == stats->lastMsgSeqNum) {
         unlock();
      }
   }
   else if (returnHdr.flag == MSG_PACKET) {
      receivedMessage(buffer, message_len, bufNdx, clientList, clientCount, maxClientCount, stats, returnHdr);
   }
}

void checkSocketSet (struct status *stats, fd_set *fdvar, uint8_t *buffer, 
                    struct handle *clientList, int *clientCount, int *maxClientCount) {
   int message_len = 0, sent = 0;
   if (FD_ISSET(stats->socket_num, fdvar)) {
      
      uint16_t checksum;
      
      //socket is ready for recv, data is there
      //now get the data on the server_socket
      message_len = recv(stats->socket_num, buffer, MAX_BUFFER_SIZE, 0);
      
      checksum = in_cksum((unsigned short *)buffer, message_len);
      
      if (message_len < 0)
      {
         perror("recv call");
         exit(-1);
      }
      else if (message_len == 0) {
         exit(-1);
      }
      else if (checksum == 0){
         determinePacketType(buffer, message_len, stats, clientList, clientCount, maxClientCount);
      }
   }
   else if (*(stats->msgSent) == 0 && *(stats->resendCount) < 9) {
      (*(stats->resendCount))++;
      sent =  send(stats->socket_num, stats->send_buf, *(stats->buffSize), 0);
      if(sent < 0) {
         perror("send call");
         exit(-1);
      }
   }
   else if (*(stats->resendCount) == 9 && (*(stats->initHandleAckStatus) == 1
                                           || *(stats->quitStatus) == 1 || *(stats->msgSent) == 0)) {
      unlock();
      
      if (*(stats->quitStatus) == 1) {
         *(stats->quit) = 1;
      }
   }
   
}

void *listenMSG(void *stat) {
   uint8_t buffer[MAX_BUFFER_SIZE];
   static struct timeval timeout;
   struct status *stats = stat;
   struct handle clientList[20];
   int clientCount = 0;
   int maxClientCount = 20;
   
   sendErr_init(stats->errorPercent, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);
   
   while (*(stats->quit) == 0) {
         fd_set fdvar;
         timeout.tv_sec = 1; //set timeout to 1 second
         timeout.tv_usec = 0; //set timeout (in micro-second)
         FD_ZERO(&fdvar);
         FD_SET(stats->socket_num, &fdvar);
         
         if (select(stats->socket_num + 1, (fd_set *) &fdvar, NULL, NULL, &timeout) < 0) {
            if (*(stats->quit) == 0) {
               exit(0);
            }
            else {
               perror("select call");
               exit(-1);
            }         
         }
         
      checkSocketSet(stats, &fdvar, buffer, clientList, &clientCount, &maxClientCount);
   }
   pthread_exit(NULL);
}

void clientQuit (int socket_num, uint32_t *seqNum, struct status *stat) {
   struct header hdr;
   int sent = 0;
   
   hdr.seqNum = htonl((*seqNum)++);
   hdr.flag = C_EXIT;
   hdr.checksum = 0;
   
   setChecksum((uint8_t *)&hdr, sizeof(hdr));
   
   /* now send the data */
   sent =  send(socket_num, &hdr, sizeof(hdr), 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
   
   *(stat->resendCount) = 0;
   *(stat->msgSent) = 0;
   stat->send_buf = (uint8_t *)(&hdr);
   *(stat->buffSize) = sizeof(hdr);
   *(stat->quitStatus) = 1;
   
   lockThread(stat);
}

int createMessage (struct message *msg, uint8_t *send_buf, uint32_t *seqNum, 
                    struct status *stat, int *bufNdx, uint8_t *handleName, 
                   int *send_len, struct header *hdr) {

   char c;
   
   stat->lastMsgSeqNum = *seqNum;
   hdr->seqNum = htonl((*seqNum)++);
   hdr->flag = MSG_PACKET;
   hdr->checksum = 0;
   
   //set destination handle name
   msg->destHandleLen = 0;
   while ((msg->destHandleName[msg->destHandleLen] = fgetc(stdin)) != ' ') {
      msg->destHandleLen++;
      if (msg->destHandleLen > 100) {
         while ((c = fgetc(stdin)) != ' ') {
            
         }
         fprintf(stdout, "Destination handle length is greater than 100.\n");
         return 0;
      }
   }
   
   *send_len = 0;
   
   while ((msg->txtMsg[*send_len] = fgetc(stdin)) != '\n') {
      (*send_len)++;
      if (*send_len > 1000) {
         while ((c = fgetc(stdin)) != EOF && c != '\n') {
            
         }
         fprintf(stdout, "Message length is longer than 1000.\n");
         return 0;
      }
   }
   
   msg->txtMsg[*send_len] = '\0';
   strcpy((char *)msg->srcHandleName, (char *)handleName);
   arrayConsumption(msg->srcHandleName, msg->srcHandleLen);
   
   return 1;
}

void listenCommands(int socket_num, uint32_t *seqNum, uint8_t *handleName, struct status *stat) {
   uint8_t *send_buf = NULL;         //data buffer
   int send_len = 0;        //amount of data to send
   int sent = 0;            //actual amount of data sent
   char option[4];
   struct header hdr;
   int bufNdx = 0;
   int quit = 0;
   
   send_buf = (uint8_t *)malloc(sizeof(struct message) + sizeof(struct header));
   
   while (quit == 0) {
      /* get the data and send it   */
      printf("> ");
      
      fgets(option, 4, stdin);
      option[2] = '\0';
      if (strcmp(option, "%M") == 0 || strcmp(option, "%m") == 0) {
         struct message msg;
         
         if (createMessage(&msg, send_buf, seqNum, stat, &bufNdx, handleName, &send_len, &hdr) == 0) {
            continue;
         }
         
         //initialize data buffer for the packet
         bufNdx = 0;
         memcpy(send_buf, &hdr, sizeof(hdr));
         bufNdx += sizeof(hdr);
         memcpy(&send_buf[bufNdx], &msg, 1 + msg.destHandleLen);
         bufNdx += 1 + msg.destHandleLen;
         
         memcpy(&send_buf[bufNdx], &(msg.srcHandleLen), 1 + msg.srcHandleLen);
         bufNdx += 1 + msg.srcHandleLen;
         
         memcpy(&send_buf[bufNdx], &(msg.txtMsg), send_len);
         bufNdx += send_len;
         
         setChecksum(send_buf, bufNdx);
         /* now send the data */
         sent =  send(socket_num, send_buf, bufNdx, 0);
         if(sent < 0) {
            perror("send call");
            exit(-1);
         }
         
         *(stat->msgSent) = 0;
         stat->send_buf = send_buf;
         *(stat->buffSize) = bufNdx;
         *(stat->result) = 0;
         *(stat->resendCount) = 0;
         
         lockThread(stat); 
      }
      else if(strcmp(option, "%E") == 0 || strcmp(option, "%e") == 0) {
         clientQuit(socket_num, seqNum, stat);
         quit = 1;
      }
   }
   free(send_buf);
}

void sendInitHandleInfo(uint8_t *handleName, uint32_t *seqNum, uint8_t *send_buf, 
                       int socket_num, struct status *stat) {
   struct initPacket handle;
   struct header hdr;
   int sent = 0;
   uint32_t hostOrderHandleIdNum = 0;
   uint32_t handleSize = 0;
   
   srand(time(NULL));
   
   //Send handle info to server
   hostOrderHandleIdNum = rand();
   handle.idNum = htonl(hostOrderHandleIdNum);
   strcpy((char *)handle.srcHandleName, (char *)handleName);
   arrayConsumption(handle.srcHandleName, handle.srcHandleLen);
   hdr.seqNum = htonl((*seqNum)++);
   hdr.flag = 1;
   hdr.checksum = 0;
   
   handleSize += sizeof(handle.idNum) + sizeof(handle.srcHandleLen) + handle.srcHandleLen;
   send_buf = (uint8_t *)malloc(sizeof(handle) + sizeof(hdr) + 1);
   memcpy(send_buf, &hdr, sizeof(hdr));
   memcpy(&send_buf[sizeof(hdr)], &handle, handleSize);
   
   setChecksum(send_buf, handleSize + sizeof(hdr));
   sent =  send(socket_num, send_buf, handleSize + sizeof(hdr), 0);
   if(sent < 0) {
      perror("send call");
      exit(-1);
   }
   
   *(stat->resendCount) = 0;
   *(stat->msgSent) = 0;
   stat->send_buf = send_buf;
   *(stat->buffSize) = handleSize + sizeof(hdr);
}

int main(int argc, char * argv[]) {
   int socket_num;         //socket descriptor
   uint8_t *send_buf = NULL;         //data buffer
   uint32_t seqNum = 1;
   uint32_t buffSize = 0;
   uint8_t result = 0;
   pthread_t listenThread;
   struct status stat;
   int msgSent = 0;
   int quit = 0;
   uint32_t resendCount = 0;
   uint8_t handleName[MAX_HANDLE_SIZE];
   int initHandleAckStatus = 1;
   int quitStatus = 0;
   int myHandleNameLength = 0;
   
   /* check command line arguments  */
   if(argc!= 5)
   {
      printf("usage: %s handle-name percent-error host-name port-number \n", argv[0]);
      exit(1);
   }
   
   arrayConsumption(argv[1], myHandleNameLength);
   
   if (myHandleNameLength > 100) {
      fprintf(stdout, "Handle name is longer than 100 characters.\n");
      return 1 ;
   }
   
   stat.errorPercent = atof(argv[2]);

   sendErr_init(stat.errorPercent, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);
   
   /* set up the socket for TCP transmission  */
   socket_num = tcp_send_setup(argv[3], argv[4]);
   
   stat.currSeqNum = &seqNum;
   stat.socket_num = socket_num;
   stat.buffSize = &buffSize;
   stat.result = &result;
   stat.msgSent = &msgSent;
   stat.quit = &quit;
   stat.resendCount = &resendCount;
   stat.initHandleAckStatus = &initHandleAckStatus;
   stat.quitStatus = &quitStatus;

   strcpy((char *)handleName, argv[1]);
   sendInitHandleInfo(handleName, &seqNum, send_buf, socket_num, &stat);
   if (pthread_create(&listenThread, NULL, &listenMSG, &stat)) {
      fprintf(stdout, "Thread could not be created\n");
   }
   else {
      lockThread(&stat);
      if (quit == 0) {
         result = 0;
         listenCommands(socket_num, &seqNum, (uint8_t *)argv[1], &stat);
      }
   }
   
   close(socket_num);
   
   return 1;
}


int tcp_send_setup(char *host_name, char *port)
{
    int socket_num;
    struct sockaddr_in remote;       // socket address for remote side
    struct hostent *hp;              // address of remote host

    // create the socket
    if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
	    perror("socket call");
	    exit(-1);
    }
    

    // designate the addressing family
    remote.sin_family= AF_INET;

    // get the address of the remote host and store
    if ((hp = gethostbyname(host_name)) == NULL)
    {
       fprintf(stderr, "Error getting hostname: %s\n", host_name);
       exit(-1);
    }
    
    memcpy((char*)&remote.sin_addr, (char*)hp->h_addr, hp->h_length);

    // get the port used on the remote side and store
    remote.sin_port= htons(atoi(port));

    if(connect(socket_num, (struct sockaddr*)&remote, sizeof(struct sockaddr_in)) < 0)
    {
       perror("connect call");
       exit(-1);
    }

    return socket_num;
}


/* This function waits for a client to ask for services.  It returns
 the socket number to service the client on.    */

int tcp_listen(int client_socket, int back_log)
{
   int server_socket = 0;
   if (listen(client_socket, back_log) < 0)
   {
      perror("listen call");
      exit(-1);
   }
   
   if ((server_socket = accept(client_socket, (struct sockaddr*)0, (socklen_t *)0)) < 0)
   {
      perror("accept call");
      exit(-1);
   }
   
   return(server_socket);
}