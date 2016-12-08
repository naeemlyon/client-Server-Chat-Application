/*****************************************************************/
/* Chat Server, Project   for  Network Programming               */
/*                        Muhammad Naeem                         */
/*****************************************************************/
#include <stdio.h>
#include <netinet/udp.h>
#include <unistd.h>
#include <string.h>
#include <linux/if_ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>

/**********************************/
/* Definitions                    */
/**********************************/
//Port as mentioned in Assignment
#define SERVER_PORT      3333   

//Maxline in our chat server
#define MAXLINE         160  

//Applicaiton Message type as defined in assignment
#define MESSAGE_TYPE 0x0005

/*********************************************************/
/* Messages as Per Project Description                   */
/*********************************************************/

#define BROADCAST_MSG 	  0
#define ERROR		 -1

#define QUIT    	 -2
#define CONNECT          -3
#define USER_NAME    	 -4
#define PASSWD       	 -5
#define GET_USER_LIST	 -6
#define USER_NOT_FOUND 	 -7
#define USER_FOUND	 -8
#define PASSWD_INVALID	 -9
#define ASSIGNED_ID	-10
#define WELCOME_MSG	-11
#define USER_LIST       -12 

/*******************************************************/
/* Structures and global variables used in Chat Server */
/*******************************************************/
typedef struct _ChatUsers
{
	char UserName[MAXLINE];
	char Password[MAXLINE];
}ChatUserList;

/* Structure to store the Client Information as in Assignment */
typedef struct _ClientInformation
{
  char Username[MAXLINE];
  int UserListIndex;
  int ClientID;
  struct sockaddr ClientAddress;
  int ClientStatus;
  int cli_new;
  pthread_t ThreadID;
}ClientInformation;

/*LinkList to store information of each clients threadId and address */

struct Node{
  char packet[MAXLINE];
  pthread_t tid; /* thread ID */
  struct sockaddr source;
  struct Node* next;
	};

/*Global variables and mutexes */
struct Node* list;// = NULL; /* head pointer for linked list */


//Mutex to linked list declaration for synchronization between threads
pthread_mutex_t listMutex = PTHREAD_MUTEX_INITIALIZER; 

//clients information structure
ClientInformation ClientInfo[FD_SETSIZE];


//mutex for client info link list
pthread_mutex_t clientInforMutex = PTHREAD_MUTEX_INITIALIZER; 

//pthread condition to wait for mutex unlock
pthread_cond_t listThreadCondition = PTHREAD_COND_INITIALIZER; 



/******************************************/
/* Load list of users from ChatFile.bin   */
/******************************************/
int ReadUsers(ChatUserList listUsers[])
{
  //Pointer to the file to read	
  FILE* fptr; 
  ChatUserList user; /* temporary user variable */
  size_t i=0;
  if((fptr = fopen("ChatFile.bin", "rb")) == NULL)
    {
      printf("\n Unable to Open file ChatFile.bin ... ");
      return 0;
    }
  while( fread(&user, sizeof(user), 1, fptr) == 1)
    listUsers[i++] = user;
  fclose(fptr);
  return i;
};

/*********************************************************************/
/* To Convert, short integer to two characters and save in buffer   */
/*******************************************************************/
void ShortToBuff(char buffer[], int len, int index)
{
  buffer[index] = len & 0x00ff;               /* Lower byte at lower index */
  buffer[index + 1] = (len >> 8) & 0x00ff;   /* Higher byte at higher index */
};

/******************************************************************/
/* function to convert buffer character to short variable         */
/******************************************************************/
short BuffToShort(char buffer[], int index)
{
  short length;
  length = buffer[index+1];
  length = (length << 8) | buffer[index];
  return length;
};

void PackAndSend(int connfd, char message[], short chat_message_type, struct sockaddr cliadd); 
int ProcessClient(char line[], int sockfd, int numbytes, struct sockaddr cliadd);
int AutheticateClient(char line[], int sockfd, int numbytes, struct sockaddr cliadd, ChatUserList listUsers[]);
int AuthenticateUsername(char user[], ChatUserList listUsers[]);
int AuthenticatePassword(int index, char password[], ChatUserList listUsers[]);
int GetClientID(struct sockaddr* client);
int GetClientStatus(struct sockaddr* client); 
int GetClientIndex(struct sockaddr* client); /*user index in online user list */
int NextEmptySlot(void);
int AlreadyOnline(char user[]);
int VerifyClient(struct sockaddr* cli1,struct sockaddr* cli2);
int GetDestInformation(short chat_message_type, struct sockaddr* dest);
void WriteToFile(int total_users, ChatUserList listUsers[]);
void listInsert(struct Node* head, char pack[],  pthread_t tid, struct sockaddr source); /* insert node in list */
struct Node* listHeadRemove(struct Node* head);
size_t listCount(struct Node* head);

struct Node* listHeadInsert(char pack[], pthread_t tid, struct sockaddr source);
struct Node* listSearch(struct Node* head, pthread_t tid);
void listRemove(struct Node* prev);
void FreeListMemory(void);
int GetPacket(struct Node* cursor, char line[]);
void* PseudoUDPReceiver(void *arg);
pthread_t GetThreadID(struct sockaddr *ClientAddress);
void MutexLock(pthread_mutex_t* lock_mutex);
void MutexUnlock(pthread_mutex_t* lock_mutex);
void PThreadWaitCondition(pthread_cond_t* listThreadCondition, pthread_mutex_t* lock_mutex);
void* HandleClient(void* arg);
void UDPSender(char *buf, int userlen, struct sockaddr_in* dest, int rawfd);
int CheckPacket(char line[], int numbytes);
int CheckUDP(char *ptr, int len, struct sockaddr* source, struct sockaddr* dest);

  /**********************************************/
 /* SIGINT handler which only frees the memory.*/
/**********************************************/
void sigint_handler(int sig)
{
  FreeListMemory();
  exit(0);
}

  /************************************/
 /* Main Programe Execution          */
/************************************/
int main(void)
{


	system("clear");      // to clear the screen
	printf("\n\t*******************************************************");
	printf("\n\t**           Chat Server by Asif and Khurram         **");
	printf("\n\t*******************************************************");
	printf("\n");

  int i,j,rawfd;
  int yes=1; 
  ssize_t numbytes; 
  
  char line[MAXLINE], message[MAXLINE]; 
  
  struct sigaction sa_int; 
  pthread_t p_tid;

  ChatUserList listUsers[FD_SETSIZE]; 
  struct Node* cursor, *prev;

  /* Server starts reading the Users stored in File*/

  int userCount = ReadUsers(listUsers);
 	
  // initialize the Users with initial values of the structure as defiend above
  for(i = 0; i < userCount; i++)
    {
      //Lock the client information list so that modification by other thread
      MutexLock(&clientInforMutex);
      ClientInfo[i].Username[0] = '\0'; /* user name */
      ClientInfo[i].UserListIndex = -1; /* Indext into user chatlist */
      ClientInfo[i].ClientID = -1; /* Id field*/
      bzero(&ClientInfo[i].ClientAddress, sizeof(struct sockaddr));
      ClientInfo[i].ClientStatus = 0; /* by default user is OFFline */
      ClientInfo[i].cli_new = 0;
      MutexUnlock(&clientInforMutex);
    }	


  /* Creates a raw  socket with AF_INET and UDP Protocol */
  if ((rawfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) == -1) {
    perror("socket");
    exit(1);
  }

  /* set signal handlers */
  sa_int.sa_handler = sigint_handler; /* reap all dead processes */
  sigemptyset(&sa_int.sa_mask);
  if (sigaction(SIGINT, &sa_int, NULL) == -1)
    {
      perror("sigaction");
      exit(1);
    }

  if(pthread_create(&p_tid, NULL, &PseudoUDPReceiver, NULL) != 0)
    {
      printf("\nError creating thread ... \n");
      exit(1);
    }

  /* infinite loop, checking for nodes with tid = 0 */
  for(;;)
    {
      /* lock mutex for linked list */      
      MutexLock(&listMutex);

      /*wait for signal from Pseudo-UDP-Reciever thread */
      if(listCount(list) == 0)
	PThreadWaitCondition(&listThreadCondition, &listMutex);

      /* trace list for nodes with tid = 0 */
      for(cursor = list; cursor != NULL; cursor = cursor->next)
	if(cursor->tid == 0)
	  {
	    prev = cursor;
	    numbytes = GetPacket(cursor, line);
	    line[numbytes] = '\0';
	    
	    if(CheckPacket(line, numbytes))
	      AutheticateClient(line, rawfd, numbytes, cursor->source, listUsers);
	    if(cursor == list)
	      list = listHeadRemove(list);
	    else
	      listRemove(prev);
	  }

      /* lock mutex for linked list */      
      MutexUnlock(&listMutex);
    }

}/*end function main */

/*****************************************/
/* Write users data to file ChatFile.bin */
/*****************************************/
void WriteToFile(int total_users, ChatUserList listUsers[])
{
  FILE* fptr;
  int i;
  if((fptr = fopen("ChatFile.bin", "wb")) == NULL)
    {
      printf("\n Unable to Open file ... ");
      exit(1);
    }

  for(i=0; i < total_users; i++)
      fwrite(&listUsers[i], sizeof(listUsers[i]), 1, fptr);

  fclose(fptr);
}


/*********************************************/
/* Format the Packet and as per Assignment   */
/* And Send to the Specific User             */
/*********************************************/ 
void PackAndSend(int rawfd, char message[], short chat_message_type, struct sockaddr cliadd)
{
  short mess_type = MESSAGE_TYPE, numbytes;
  short payload_length = 0, i;
  unsigned char seq = 1;
  char packet[MAXLINE];
  socklen_t clilen;
  struct sockaddr_in *dest, local;
  dest = (struct sockaddr_in*)&cliadd;

  clilen = sizeof(struct sockaddr);
  payload_length = 3 + strlen(message); /* chat mess.type + value */

  /* form the packet in TLV format, first 8 bytes are left for UDP header */
  ShortToBuff(packet, mess_type, 8); /* Message type at the start of packet */
  ShortToBuff(packet, payload_length, 10); /* place payload length after Message type */
  packet[12] = seq; /* sequence number */
  ShortToBuff(packet, chat_message_type, 13); /* place z message type before actual message string */
  
  for(i = 0; i < strlen(message); i++) /* copy the payload value at the end */
    packet[i + 15] = message[i];

  UDPSender(packet, payload_length + 4, dest, rawfd);

}

/*********************************************************/
/* this function will process the received message       */
/* from client                                           */
/*********************************************************/
int AutheticateClient(char line[], int rawfd, int numbytes, struct sockaddr cliadd, ChatUserList listUsers[])
{
 short message_type, chat_message_type, payload_length;
 int source_id, i, index, total_users;
 char temp[MAXLINE], user_list[MAXLINE];
 static int next_id=100, Online = 0;
 pthread_t tid;
int new_client;
 message_type = BuffToShort(line, 0); /* Extract message type */
 if(message_type == MESSAGE_TYPE) /* valid message type for chat applications */
   {
     chat_message_type = BuffToShort(line, 5); /* Extract message type */
     switch(chat_message_type)
       {
       case CONNECT:
		
 		/*state information for new client */
  		new_client = NextEmptySlot(); /* find the next available slot for new client in user list*/
  		MutexLock(&clientInforMutex); /* lock mutex */
  		ClientInfo[new_client].ClientAddress = cliadd;
  		ClientInfo[new_client].ClientStatus = 2; /* waiting for UserName on this socket */
  		MutexUnlock(&clientInforMutex); /* unlock mutex */

	 
	 	PackAndSend(rawfd, "Welcome to  CHAT SERVER. Please send your username and Password", \
		       WELCOME_MSG, cliadd);
	 break;
	 
       case USER_NAME:
	 if(GetClientStatus(&cliadd) == 2) /* if the server was waiting for username, go ahead */
	   {
	     if(AlreadyOnline(&line[7]))
	       { /* If user is already online, dont allow single user to login multiple times */
		 index = GetClientIndex(&cliadd);
		 MutexLock(&clientInforMutex); /* lock mutex before accessing client info */
		 ClientInfo[index].Username[0] = '\0'; /* user name */
		 ClientInfo[index].UserListIndex = -1; /* Indext into user chatlist */
		 ClientInfo[index].ClientID = -1; /* Id field*/
		 bzero(&ClientInfo[index].ClientAddress, sizeof(struct sockaddr));
		 ClientInfo[index].ClientStatus = 0; /* by default user is OFFline */
		 MutexUnlock(&clientInforMutex);
		 PackAndSend(rawfd, "User already online, use your own ID", ERROR, cliadd);
		 return -1;
	       }
	     
	     if((i = AuthenticateUsername(&line[7],listUsers)) >= 0)
	       { /* if user name is already registered */
		 index = GetClientIndex(&cliadd);
		 MutexLock(&clientInforMutex); /* lock mutex before accessing client info */
		 strcpy(ClientInfo[index].Username, &line[7]); /* save user name at fixed slot */
		 ClientInfo[index].ClientStatus = 3; /* waiting for password */
		 ClientInfo[index].ClientAddress = cliadd;
		 ClientInfo[index].UserListIndex = i; /* Indext into user chatlist */
		 MutexUnlock(&clientInforMutex); /* lock mutex before accessing client info */
		 PackAndSend(rawfd, "Waiting for your password", USER_FOUND, cliadd);
	       }
	     else
	       { 
		 /* Initialize new client information */
		 index = GetClientIndex(&cliadd);
		  MutexLock(&clientInforMutex); /* lock mutex before accessing client info */
		 strcpy(ClientInfo[index].Username, &line[7]); /* user name */
		 ClientInfo[index].ClientStatus = 3; /* waiting for password */
		 ClientInfo[index].cli_new = 1; /* declare it new user */
		 MutexUnlock(&clientInforMutex); /* lock mutex before accessing client info */
		 PackAndSend(rawfd, "You are new User. Register your  Password Please", USER_NOT_FOUND, cliadd);
	       }
	   }
	 else
	   PackAndSend(rawfd, "Invalid response, message discarded", ERROR, cliadd); /* generate error */
	 
	 break;
	 
       case PASSWD:
	 if(GetClientStatus(&cliadd) == 3) /* if the server was waiting for password, go ahead */
	   {
	     index = GetClientIndex(&cliadd);
	     MutexLock(&clientInforMutex); /* lock mutex before accessing client info */
	     if(ClientInfo[index].cli_new == 1)
	       {
		 ClientInfo[index].ClientStatus = 1; /* user online */
		 ClientInfo[index].ClientID = next_id++; /* ID */
		 ClientInfo[index].cli_new  = 0; /* reset new user flag */
		 total_users = ReadUsers(listUsers);		     
		 strcpy(listUsers[total_users].UserName, ClientInfo[index].Username);
		 strcpy(listUsers[total_users++].Password, &line[7]);
		 WriteToFile(total_users, listUsers );     
		 sprintf(temp, "%d", ClientInfo[index].ClientID); /* pack client ID */
		 PackAndSend(rawfd, temp, ASSIGNED_ID, cliadd); /* send ID to client */
		 printf("\n New user added ... Online Users: %d\n", ++Online);
		 		 
		 if(pthread_create(&tid, NULL, &HandleClient, (void *)rawfd) != 0)
		   {
		     printf("\nError creating thread ... \n");
		     exit(1);
		   }
		 
		 ClientInfo[index].ThreadID = tid;
		 
	       }
	     else
	       {
		 if(AuthenticatePassword(ClientInfo[index].UserListIndex, &line[7], listUsers) )
		   {
		     if(pthread_create(&tid, NULL, &HandleClient, (void *)rawfd) != 0)
		       {
			 printf("\nError creating thread ... \n");
			 exit(1);
		       }
		     ClientInfo[index].ClientStatus = 1; /* user online */
		     ClientInfo[index].ClientID = next_id++; /* ID */
		     ClientInfo[index].ThreadID = tid;
		     sprintf(temp, "%d", ClientInfo[index].ClientID); /* pack client ID */
		     PackAndSend(rawfd, temp, ASSIGNED_ID, cliadd); /* send ID to client */
		     printf("\n Another user logged in ... Online Users: %d\n", ++Online);		     
		   }
		 else
		   { /* invalid password, connection is being closed */
		     ClientInfo[index].Username[0] = '\0'; /* user name */
		     ClientInfo[index].UserListIndex = -1; /* Indext into user chatlist */
		     ClientInfo[index].ClientID = -1; /* Id field*/
		     bzero(&ClientInfo[index].ClientAddress, sizeof(struct sockaddr));
		     ClientInfo[index].ClientStatus = 0; /* by default user is OFFline */
		     PackAndSend(rawfd, "Invalid password, connection closed", PASSWD_INVALID, cliadd);
		   }
		 
	       }
	   }
	 MutexUnlock(&clientInforMutex); /* lock mutex before accessing client info */
	 break;
	     
       case ERROR:
	 source_id = GetClientID(&cliadd);	 
	 printf("Error string returned by %d: %s", source_id, &line[7]);
	 break;
	 
       case QUIT:
	 index = GetClientIndex(&cliadd);
	 MutexLock(&clientInforMutex); /* lock mutex forbuffer accessing client info*/
	 ClientInfo[index].Username[0] = '\0'; /* user name */
	 ClientInfo[index].UserListIndex = -1; /* Indext into user chatlist */
	 ClientInfo[index].ClientID = -1; /* Id field*/
	 bzero(&ClientInfo[index].ClientAddress, sizeof(struct sockaddr));
	 ClientInfo[index].ClientStatus = 0; /* by default user is OFFline */
	 MutexUnlock(&clientInforMutex);
	 break;
	 
       }/* end of switch statement */
   } /* end of message_id check */
 else
   PackAndSend(rawfd, "Incorrect Message Type", ERROR, cliadd);
}/* end of function */


/*********************************************************/
/* Process the Incoming Request                          */
/*                                                       */
/*********************************************************/
int ProcessClient(char line[], int rawfd, int numbytes, struct sockaddr cliadd)
{
 short message_type, chat_message_type, payload_length;
 int source_id, dest_id, i, index, sock, total_users;
 char temp[MAXLINE], user_list[MAXLINE];
 static int next_id=100, Online = 0;
 pthread_t tid;
 ChatUserList listUsers[FD_SETSIZE];

 message_type = BuffToShort(line, 0); /* Extract message type */
 if(message_type == MESSAGE_TYPE) /* valid message type for chat applications */
   {
     chat_message_type = BuffToShort(line, 5); /* Extract chat message type */
     
     if(chat_message_type > 0)
       { /* Individual message */
	 source_id = GetClientID(&cliadd); /* get ID of the client that sent the message */
	 dest_id = GetDestInformation(chat_message_type, &cliadd);
	 sprintf(temp, "%d - %d:", source_id, dest_id);
	 strcat(temp, &line[7]); /* concatenate client id with message string */
	 printf("\n%s\n", temp);
	 PackAndSend(rawfd, temp, chat_message_type, cliadd);
	 return 0;
       }
     else
       {
	 switch(chat_message_type)
	   {
	   case BROADCAST_MSG:  
	     index = GetClientIndex(&cliadd); /* get Index in information table */
	     MutexLock(&clientInforMutex); /* lock mutex */
	     if(ClientInfo[index].ClientStatus == 1)
	       {
		 sprintf(temp, "%d:", ClientInfo[index].ClientID);
		 strcat(temp, &line[7]); /* concatenate client id with message string */
		 printf("\n%s\n", temp);
		 /*send to everyone online */
		 
		 total_users = ReadUsers(listUsers);
		 for(i = 0; i < total_users; i++)
		   {
		     if(ClientInfo[i].ClientStatus == 1) 
		       {
			 cliadd = ClientInfo[i].ClientAddress;
			 PackAndSend(rawfd, temp, ClientInfo[i].ClientID, cliadd);
		       }
		   } 
	       }
	     else
	       {
		 PackAndSend(rawfd, "You need to be authenticated", ERROR, cliadd); /* generate error */
	       }
	     MutexUnlock(&clientInforMutex); /* unlock mutex */
	     break;
	    	     
	   case ERROR:
	     source_id = GetClientID(&cliadd);	 
	     printf("Error string returned by %d: %s", source_id, &line[7]);
	     break;
	     
	   case QUIT:
	     index = GetClientIndex(&cliadd);
	     MutexLock(&clientInforMutex); /* lock mutex */
	     ClientInfo[index].Username[0] = '\0'; /* user name */
	     ClientInfo[index].UserListIndex = -1; /* Indext into user chatlist */
	     ClientInfo[index].ClientID = -1; /* Id field*/
	     bzero(&ClientInfo[index].ClientAddress, sizeof(struct sockaddr));
	     ClientInfo[index].ClientStatus = 0; /* by default user is OFFline */
	     MutexUnlock(&clientInforMutex); /* unlock mutex */
	     break;

	   case GET_USER_LIST:
	     user_list[0] = '\0'; /*Initialize user list */
	     strcpy(user_list, "List of Online Users, Format --> UserID:UserName\n\n");
	     index = 0; /* Initialize byte count */
	     
	     total_users = ReadUsers(listUsers);
	     MutexLock(&clientInforMutex); /* lock mutex */
	     for(i = 0; i < total_users; i++)
	       if( ClientInfo[i].ClientStatus == 1)
		 {
		   sprintf(temp, "  %d:", ClientInfo[i].ClientID);
		   strcat(temp, ClientInfo[i].Username);
		   strcat(user_list, temp);
		   index = index + strlen(user_list); /* count total bytes in packet */
		   if(index < MAXLINE - 7)
		     continue;
		   else
		     break;
		 }
	     MutexUnlock(&clientInforMutex); /* unlock mutex */
	     index = GetClientID(&cliadd);
	     PackAndSend(rawfd, user_list, USER_LIST, cliadd); 
	     break;
	   }/* end of switch statement */
       } /* end of else */
   } /* end of message_id check */
 else
   PackAndSend(rawfd, "Incorrect Message Type", ERROR, cliadd);
}/* end of function */

/**************************************************/
/* Get CLient ID of the user using Socket Address */
/*                                                */
/**************************************************/
int GetClientID(struct sockaddr* client)
{
  int i, total_users, id;
  socklen_t len;
  ChatUserList listUsers[FD_SETSIZE];

  len = sizeof(struct sockaddr);
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if(memcmp(&ClientInfo[i].ClientAddress, client, sizeof(&ClientInfo[i].ClientAddress)) == 0)
      {
	id = ClientInfo[i].ClientID;
	break;
      }

  MutexUnlock(&clientInforMutex); /* lock mutex */
  return id;
}

/*************************************************/
/* Get the CLient index in chat list using Client*/
/* Address                                       */
/*************************************************/
int GetClientIndex(struct sockaddr* client)
{
  int i, total_users;
  socklen_t len;
  ChatUserList listUsers[FD_SETSIZE];
  len = sizeof(struct sockaddr);
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if(memcmp(&ClientInfo[i].ClientAddress, client, sizeof(&ClientInfo[i].ClientAddress)) == 0)  
      break;
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return i;
}

/*************************************************/
/* Get the Current Status of the User            */
/*************************************************/
int GetClientStatus(struct sockaddr* client)
{
  int i, total_users, st;
  socklen_t len;
  ChatUserList listUsers[FD_SETSIZE];
  len = sizeof(struct sockaddr);
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if(memcmp(&ClientInfo[i].ClientAddress, client, sizeof(&ClientInfo[i].ClientAddress)) == 0)  
      {
	st = ClientInfo[i].ClientStatus;
	break;
      }
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return st;
}

/*********************************************************/
/* Authenticate the Usernaem in the userList in ChatFile */
/*********************************************************/
int AuthenticateUsername(char user[], ChatUserList listUsers[])
{
  int i, total_users;
  total_users = ReadUsers(listUsers);
  for(i = 0; i < total_users; i++)
    if(strcmp(user,  listUsers[i].UserName) == 0)
      {
	return i;
      }
  return -1;
}

/***************************************************/
/* Authenticate the Password                       */
/***************************************************/
int AuthenticatePassword(int index, char password[], ChatUserList listUsers[])
{
  int i;
  if(strcmp(password, listUsers[index].Password) == 0)
    return 1;
  else
    return 0;
}
/**************************************************/
/* Check the Next Empty Slot                      */
/**************************************************/
int NextEmptySlot(void)
{
  int i, total_users;
  ChatUserList listUsers[FD_SETSIZE];
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if( ClientInfo[i].ClientStatus == 0) /* match the socket descriptor and return the matching id */
      break;
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return i;
}


/*************************************************/
/* CHeck if user is already Online               */  
/*************************************************/
int AlreadyOnline(char user[])
{
  int i, flag = 0;
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < FD_SETSIZE; i++)
    if((strcmp(user, ClientInfo[i].Username)) == 0) /* match user name that is alread on the list */
      {      
	flag = 1;
	break;
      }
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return flag;
}

/***********************************************/
/* Comparison FUnction for two client Address  */
/***********************************************/
int VerifyClient(struct sockaddr* cli1,struct sockaddr* cli2)
{
  int i;
  char str1[46], str2[46];
  short port;
  struct sockaddr_in *sin1 = (struct sockaddr_in *) cli1;
  struct sockaddr_in *sin2 = (struct sockaddr_in *) cli2;
  if (inet_ntop(AF_INET, &sin1->sin_addr, str1, sizeof(str1)) == NULL)
    {
      perror("inet_ntop()");
      exit(1);
    }

  if (inet_ntop(AF_INET, &sin2->sin_addr, str2, sizeof(str2)) == NULL)
    {
      perror("inet_ntop()");
      exit(1);
    }


  if(strcmp(str1, str2) == 0)
  { /* then match port number */
    if(ntohs(sin1->sin_port) == ntohs(sin2->sin_port))
      return 1; /* both match, return status */
  }
  return 0;
} /* end of for loop in AF_INET */

/*********************************************/
/* Get the destination Information           */
/*********************************************/
int GetDestInformation(short chat_message_type, struct sockaddr* dest)
{
  int i, total_users, id;
  ChatUserList listUsers[FD_SETSIZE];
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if( ClientInfo[i].ClientID == chat_message_type) /* match destination Id and id*/
      {
	*dest = ClientInfo[i].ClientAddress;
	id = ClientInfo[i].ClientID;
	break;
      }
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return id;
}

/***********************************************/
/* Insert an element in link list              */
/***********************************************/
void listInsert(struct Node* head, char pack[],  pthread_t tid, struct sockaddr source)
{
  short total_bytes = 0, i;
  struct Node* cursor, *prev;
  
  total_bytes = BuffToShort(pack, 2) + 4;

  for(cursor = head; cursor != NULL; cursor = cursor->next)
    prev = cursor;
  cursor = (struct Node*) malloc(sizeof(struct Node));
  
  /* copy packet in data area */
  for(i = 0; i < total_bytes; i++)
    cursor->packet[i] = pack[i];
  cursor->tid = tid; /* thread ID */
  cursor->source = source; /* source of packet */

  cursor->next = prev->next;
  prev->next = cursor;
  
}

/************************************************/
/* Search the particualr Thread from the list   */
/************************************************/
struct Node* listSearch(struct Node* head, pthread_t tid)
{
  struct Node* cursor;

  for(cursor = head; cursor != NULL; cursor = cursor->next)
    if(tid == cursor->tid)
      return cursor;

  return NULL;
}

/**********************************************/
/* Insert the head in link list               */
/**********************************************/
struct Node* listHeadInsert(char pack[], pthread_t tid, struct sockaddr source)
{
  short total_bytes = 0, i;
  struct Node* cursor;

  total_bytes = BuffToShort(pack, 2) + 4;
  cursor = (struct Node*) malloc(sizeof(struct Node));

 /* copy packet in data area */
  for(i = 0; i < total_bytes; i++)
    cursor->packet[i] = pack[i];
  cursor->tid = tid; /* thread ID */
  cursor->source = source; /* source of packet */

  cursor->next = NULL;
  return cursor;
}

/**********************************************/
/* Get the total count of elements in list    */
/**********************************************/
size_t listCount(struct Node* head)
{
  struct Node* cursor;
  size_t total = 0;
  
  for(cursor = head; cursor != NULL; cursor = cursor->next)
    total++;
  return total;
}
/*********************************************/
/* Removes the head from the list            */
/*********************************************/
struct Node* listHeadRemove(struct Node* head)
{
  struct Node* remove;
  remove = head;
  head = head->next;
  free(remove);
  return head;
}

/*********************************************/
/* Removes the previous  node from the list  */
/*********************************************/
void listRemove(struct Node* prev)
{
  struct Node* remove;
  remove = prev->next;
  prev->next = remove->next;
  free(remove);
}

/********************************************/
/* Frees the memory for this list           */
/********************************************/
void FreeListMemory(void)
{
  struct Node* cursor, *head;
  int i, count=0;

  head = list;
  for(cursor = head; cursor != NULL; cursor = cursor->next)
    free(cursor);
}
/***********************************************/
/*UDP receiver as per group assignement         */
/***********************************************/
void* PseudoUDPReceiver(void* arg)
{
  int i, j, sockfd;
  int yes=1; /* Hold client IDs, next available ID */
  ssize_t numbytes; /* received bytes */
  char line[MAXLINE + 28], message[MAXLINE]; /* line string, message string, log string etc.*/
  socklen_t clilen; /* holds the length of client sockaddrr */
  struct sockaddr servaddr, *s; /* client and server address structures */
  struct sigaction sa_int; /* signal handler structure */
  struct sockaddr ClientAddress;
  pthread_t tid;
  
  /* create datalink layer socket */
  if ((sockfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) == -1)
    {
      perror("socket");
      exit(1);
    }

  printf("\n Chat Server Started Successfully .... \n Waiting for User connections and Messages:\n");

  /* infinite loop to handle clients */
  for(;;)
    {
      clilen = sizeof(ClientAddress);
      if ((numbytes=recvfrom(sockfd, line, MAXLINE, 0, (struct sockaddr *)&ClientAddress, &clilen)) == -1)
	{
	  perror("recvfrom");
	  exit(1);
	}
      if(CheckUDP(line, numbytes, &ClientAddress, &servaddr) == 1)
	{      
	  MutexLock(&listMutex);
	  tid = GetThreadID(&ClientAddress);
	  if(listCount(list) == 0)
	    list = listHeadInsert(&line[28], tid, ClientAddress);
	  else
	    listInsert(list, &line[28], tid, ClientAddress);
	  
	  /* broadcast signal to all threads waiting for condition variable */
	  if(pthread_cond_broadcast(&listThreadCondition) > 0)
	    {
	      printf("\nunable to broadcast signal for condition variable\n");
	      exit(1);
	    }
	  
	  MutexUnlock(&listMutex);
	}
      else
	continue;
    } /* end for(;;) loop */ 
  return NULL;
}

/**************************************************/
/* Get the Thread ID from client information table*/
/**************************************************/
pthread_t GetThreadID(struct sockaddr *ClientAddress)
{
  int i, id = 0, total_users;
  ChatUserList listUsers[FD_SETSIZE];
  total_users = ReadUsers(listUsers);
  MutexLock(&clientInforMutex); /* lock mutex */
  for(i = 0; i < total_users; i++)
    if(memcmp(&ClientInfo[i].ClientAddress, ClientAddress, sizeof(&ClientInfo[i].ClientAddress)) == 0) 
      { 
	id = ClientInfo[i].ThreadID;
	break;
      }  
  MutexUnlock(&clientInforMutex); /* unlock mutex */
  return id;
}

/************************************************/
/* Get the Packer from the list and count       */
/************************************************/
int GetPacket(struct Node* cursor, char line[])
{
  short total_bytes, i;

  total_bytes = BuffToShort(cursor->packet, 2) + 4;
  /* copy packet in data area */
  for(i = 0; i < total_bytes; i++)
    line[i] = cursor->packet[i];  

  return total_bytes;
}

/********************************************/
/* Mutex lock with error handling           */
/********************************************/
void MutexLock(pthread_mutex_t* lock_mutex)
{
  int n;
  if((n = pthread_mutex_lock(lock_mutex) == 0))
    return;
  errno = n;
  perror("pthread_mutex_lock"); 
}

/********************************************/
/* Mutex Unlock with error handling         */
/********************************************/
void MutexUnlock(pthread_mutex_t* lock_mutex)
{
  int n;
  if((n = pthread_mutex_unlock(lock_mutex) == 0))
    return;
  errno = n;
  perror("pthread_mutex_unlock");
}

/********************************************/
/* Wait Condition                           */
/********************************************/
void PThreadWaitCondition(pthread_cond_t* sig_cond, pthread_mutex_t* lock_mutex)
{
  int n;
  if(pthread_cond_wait(sig_cond, lock_mutex) == 0)
    return;
  errno = n;
  perror("pthread_cond_wait");
}

/************************************************/
/* Handle CLients thread                        */
/************************************************/
void* HandleClient(void* arg)
{
  pthread_t tid;
  char line[MAXLINE];
  struct Node* cursor, *prev;
  int numbytes, rawfd;

  tid = pthread_self();
  rawfd = (int)arg;
  while(1)
    {
      /* lock mutex for linked list */      
      MutexLock(&listMutex);
      /*wait for signal from Pseudo-UDP-Reciever thread */
      PThreadWaitCondition(&listThreadCondition, &listMutex);

      for(cursor = list; cursor != NULL; cursor = cursor->next)
	  if(cursor->tid == tid)
	    {
	      prev = cursor;
	      numbytes = GetPacket(cursor, line);
	      line[numbytes] = '\0';
	      if(CheckPacket(line, numbytes))
		ProcessClient(line, rawfd, numbytes, cursor->source);
	      if(cursor == list)
		list = listHeadRemove(list);
	      else
		listRemove(prev);
	    }
      /* unlock mutex */
      MutexUnlock(&listMutex);
    } /*end of while loop */
}

/*******************************************************/
/* Create UDP packets as per specs and sends to client */
/*******************************************************/
void UDPSender(char *buf, int userlen, struct sockaddr_in* dest, int rawfd)
{
  struct udphdr *udp;
  struct sockaddr_in local;  
  int numbytes;

  local.sin_family = AF_INET;         // host byte order
  local.sin_port = htons(SERVER_PORT);     // short, network byte order
  local.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
  bzero(&(local.sin_zero), 8);        // zero the rest of the struct

  /* 4Fill in and checksum UDP wheader */
  udp = (struct udphdr *)buf;
  /* 8add 8 to userlen for pseudo-header length */
  udp->len = htons((u_short) (sizeof(struct udphdr) + userlen));
  udp->source = local.sin_port;
  udp->dest = dest->sin_port;
  udp->check = 0;
  userlen = userlen + sizeof(struct udphdr);//  + sizeof(struct udphdr);
  if ((numbytes=sendto(rawfd, buf, userlen, 0, (struct sockaddr *)dest, sizeof(struct sockaddr))) == -1)
    {
      perror("recvfrom");
      exit(1);
    }
}

/**************************************/
/* verifies packet ...................*/
/**************************************/
int CheckPacket(char line[], int numbytes)
{
  if((BuffToShort(line, 0) == MESSAGE_TYPE) && (BuffToShort(line, 2) == numbytes - 4))
    return 1;
  else
    return 0;
    
}

/*****************************************************/
/* Check the Packet header                           */
/*****************************************************/
int CheckUDP(char *ptr, int len, struct sockaddr* source, struct sockaddr* dest)
{
  int	hlen;
  struct ip *ip;
  struct udphdr	*ui;
  short buff[10];
  struct sockaddr_in* server, *client;

  server = (struct sockaddr_in*)dest;
  client = (struct sockaddr_in*)source;

  
  if (len < sizeof(struct ip) + sizeof(struct udphdr))
      return 0;
  /* *INDENT-ON* */

  /* 4minimal verification of IP header */
  ip = (struct ip *) ptr;
  ui = (struct udphdr *) (ptr+20);
  if (ip->ip_v != IPVERSION)
      return 0;
  hlen = ip->ip_hl << 2;
  /* *INDENT-OFF* */
  if (hlen < sizeof(struct ip))
      return 0;

  if (len < hlen + sizeof(struct udphdr))
      return 0;
  
  if (ip->ip_p != IPPROTO_UDP)
      return 0;

  if(ntohs(ui->dest) !=  SERVER_PORT)       
    return 0;

  client->sin_port = ui->source;
  server->sin_port = ui->dest;
    
  client->sin_addr.s_addr = ip->ip_src.s_addr; 
  server->sin_addr.s_addr = ip->ip_dst.s_addr;

  client->sin_family = AF_INET;
  server->sin_family = AF_INET;  
  return 1;
}

