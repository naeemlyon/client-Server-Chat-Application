/*****************************************************************/
/* Chat_client            Project        Network programming     */
/*                                                               */
/* Client connects to chat server and then uses select() to poll */
/* stdin and socket descriptor. if socket becomes readable, it   */
/* reads data and displays on screen. else if stdin becomes      */
/* readable, reads data and sends to chat server.                */
/*                       Muhammad Naeem                          */
/*****************************************************************/

/**********************************/
/* Headerfiles                   */
/********************************/
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
/**********************************/
/* Definitions                   */
/********************************/
#define SERV_PORT      3333    /* the port clients will be connecting to */
#define MAXLINE         160

/**********************************/
/* Message types                 */
/********************************/

     /* General*/
#define BROADCAST_MSG 	  0
#define ERROR		 -1
#define QUIT    	 -2
    /* Client to Server*/
#define CONNECT          -3
#define USER_NAME    	 -4
#define PASSWD       	 -5
#define GET_USER_LIST	 -6
#define USER_NOT_FOUND 	 -7
   /* Server to Client */
#define USER_FOUND	 -8
#define PASSWD_INVALID	 -9
#define ASSIGNED_ID	-10
#define WELCOME_MSG	-11
#define USER_LIST       -12
#define MESSAGE_TYPE     0x0005
 /* Server socket address structure */
struct sockaddr_in servaddr;                                                       
/***********************************/
/* FUNCTION Definitions           */
/*********************************/
/*********************************************************************/
/* To Convert, short integer to two characters and save in buffer   */
/*******************************************************************/
void ShortToBuff(char packet[], int len, int index)
{
  packet[index] = len & 0x00ff;               /* Lower byte at lower index */
  packet[index + 1] = (len >> 8) & 0x00ff;   /* Higher byte at higher index */
};

/*******************************************************************/
/* To convert buffer character to short variable                  */
/*****************************************************************/
int BuffToShort(char data[], int index)
{
  int length;
  length = data[index+1];
  length = (length << 8) | data[index];
  return length;
};

/**********************************************************************************/
/* Make packet according to given format and send it to specified peer           */
/*   Message Formate   |MESSAGE TYPE| |PAYLOAD LENGTH| |PAYLOAD VALUE|          */
/*   PayloadFormate     |Seq No| |Chat Msg Type| |Message|                     */
/*                                                                            */
/*****************************************************************************/ 
void PackAndSend(int sockfd, char message[], short chat_message_type)
{
  short mess_type = MESSAGE_TYPE, numbytes;
  short payload_length = 0, i;
  unsigned char seq = 1;
  char packet[MAXLINE];
  socklen_t clilen;
  clilen = sizeof(struct sockaddr);
  payload_length = 3 + strlen(message);           /* chat mess.type + value */
  /* Form the packet format */
  ShortToBuff(packet, mess_type, 0);            /* Message type at the start of packet */
  ShortToBuff(packet, payload_length, 2);      /* place payload length after Message type */
  packet[4] = seq;                            /* sequence number */
  ShortToBuff(packet, chat_message_type, 5); /* place chat message type before actual message string */

  for(i = 0; i < strlen(message); i++)     /* copy the payload value at the end */
    packet[i + 7] = message[i];

  if ((numbytes=sendto(sockfd, packet, payload_length + 4, 0, (struct sockaddr *)&servaddr, clilen)) == -1)
    {
      perror("sendto");
      exit(1);
    }

};
/**********************************************/
/* Display menu function and handle options  */
/********************************************/
void GetOption(int sockfd)
{
  char id[5], message[MAXLINE];
  int client_id, option;
  printf("\n\t\tSpecial Options.");
  printf("\n\t1. Get User List from server");
  printf("\n\t2. Send Message to specified user ");
  printf("\n                              ");
  printf("\n\t\tEnter Your Option:");
  while(1)
    {
      scanf("%d", &option);
      switch(option)
	{
	case 1:
	  PackAndSend(sockfd, "", GET_USER_LIST);
	  return;
	case 2:
	  printf("\n Enter user ID:\n");
	  scanf("%d", &client_id);
	  printf("\n Enter your message:");
	  gets(message);
	  gets(message);
	  PackAndSend(sockfd, message, client_id);
	  printf("\nMessage sent to %d\n", client_id);
	  return;
	default:
	  break;
	}
    }
};
/*********************************************************/
/* Server Message processing according to Message type  */
/*******************************************************/
void ProcessServerMessage(char message[], int total, int sockfd)
{
  int message_type, chat_message_type, payload_length;
  char passwd[MAXLINE];
  message_type = BuffToShort(message, 0);
  if(message_type == MESSAGE_TYPE)
    {
      chat_message_type = BuffToShort(message, 5);
      switch(chat_message_type)
	{
	case ERROR:
	  printf("\nError returned by server: %s\n", &message[7]);
	  break;
	case USER_NOT_FOUND:
	  printf("\n%s\n", &message[7]);
	  printf("\nPassword:");
	  gets(message);
	  PackAndSend(sockfd, message, PASSWD);
	  break;
	case USER_FOUND: 
	  printf("\n%s\n", &message[7]);
	  printf("\nPassword:");
	  gets(passwd);
	  PackAndSend(sockfd, passwd, PASSWD);
	  break;
	case PASSWD_INVALID:
	  printf("\n%s\n", &message[7]);
	  close(sockfd); /* connection closed */
	  exit(0);
	case ASSIGNED_ID:     /* After Login*/
	  printf("\n Your ID: %s\n", &message[7]);
	  printf("\nConnected to chat server.");
	  printf("\nEverything you type and press 'Enter' will be broadcasted.");
	  printf("\nTo send message to a specified user, use special options by pressing '/'\n");
	  printf("\nThanks for using our software, Its all about Network Programming Course \n");
	  printf("\n===================================================================\n");
	  break;
	case WELCOME_MSG:
	  printf("\n%s\n", &message[7]);
	  printf("\nEnter Your User Name:");
	  gets(message);
	  PackAndSend(sockfd, message, USER_NAME);
	  break;
	case QUIT:
	  printf("\n QUIT_CHAT message recieved. connection will be closed now.");
	  close(sockfd);                              /* connection closed */
	  exit(0);
	case USER_LIST:
	  printf("\n%s\n", &message[7]);
	  gets(message);
	  break;
	default:
	  printf("\n%s\n", &message[7]);
	}
    }
  else
    PackAndSend(sockfd, "Incorrect Message Type", ERROR);
};

/*********************************************/
/*This function uses select() call to poll  */
/* standard input and socket descriptor.   */
/* and handles them when become readable. */
/*****************************************/
void StartChatting(FILE *fp, int sockfd)
{
  int maxfdp1, stdineof;
  fd_set rset;
  char sendline[MAXLINE], recvline[MAXLINE];
  int numbytes;
  socklen_t clilen;

  stdineof = 0;
  FD_ZERO(&rset);
  for ( ; ; )
    {
      /* Initialize fd_set for select() call */
      if (stdineof == 0)
	FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
    
      /* find the maximum descriptor */
      if(fileno(fp) > sockfd)
	maxfdp1 = fileno(fp) + 1;
      else      
	maxfdp1 = sockfd + 1;
      /* wait for any event to happen */     
      if (select(maxfdp1, &rset, NULL, NULL, NULL) == -1)
	{
	  perror("select");
	  exit(1);
	}
      
      if (FD_ISSET(sockfd, &rset)) 
	{	
     /* socket is readable, read and display the message */
	  clilen = sizeof(struct sockaddr);
	  if ((numbytes=recvfrom(sockfd, recvline, MAXLINE, 0, (struct sockaddr *)&servaddr, &clilen)) == -1)
	    {
	      perror("recvfrom");
	      exit(1);
	    }
	  recvline[numbytes] = '\0';
	  ProcessServerMessage(recvline, numbytes, sockfd);	      
	}
      
      if (FD_ISSET(fileno(fp), &rset))
	{  /* input is readable */
	  if (fgets(sendline, MAXLINE, fp) == NULL)
	    {
	      stdineof = 1;
	      shutdown(sockfd, SHUT_WR);	/* send FIN */
	      FD_CLR(fileno(fp), &rset);
	      continue;
	    }

	  if(sendline[0] == '/')
	      GetOption(sockfd);
	  else
	    PackAndSend(sockfd, sendline, BROADCAST_MSG);
	}
    }
};


/**************************************/
/*        main() starts              */
/************************************/
int main(int argc, char **argv)
{
  int sockfd, numbytes;

  char message[MAXLINE];
/********* Client consol display ******************************************/
         
	system("clear");      // to clear the screen
	printf("\n\t*******************************************************");
	printf("\n\t**           Chat Client                             **");
	printf("\n\t*******************************************************");
	printf("\n");

  /* Input Argument checks*/
  if (argc != 2){
            fprintf(stderr,"usage: client hostname\n");
            exit(1);
        }
  
  /*UDP Socket Creation */
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror("socket");
      exit(1);
    }
  
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERV_PORT);
  inet_pton(AF_INET, argv[1], &servaddr.sin_addr);


  /* Connect to server */
  message[0] = '\0';
  PackAndSend(sockfd, message, CONNECT);
  /* Chat start */
  StartChatting(stdin, sockfd);		
  exit(0);
}




