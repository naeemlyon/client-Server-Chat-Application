   /*************************************************************************/
  /*                              Muhammad Naeem                           */
 /*************************************************************************/

This is a group assignment that covers all functionality as mentioned in the group Assignment. 
The Server will use the DATA link Sockets to receive Datagram and to send the resposne the server is using UDP rawsockets as mentioned in description. 
We have introduced one additional Chat Message Type "USER_LIST" i.e -12 from Server to Client which is being used to inform client that user list is in the payload.
For each client there will be a separate thread along with main thread receiving incoming packet on port 3333.

A make file i.e MakeFile is also available which will generate the ChatServer.o and ChatClient.o but simple shell script can be used to compile and run the client and server.
Use the below command for make file
>>>make -f MakeFile

ChatServer does not take any argument and starts listening on port 3333.
ChatCLient will take one argumen i.e Hostname which is localhost by default

Assumptions :
1)  ChatServer.c requires are binary file ChatFile.bin which is being used by server to load the users. Without this file chatserver will not run properly. ChatFile.bin will contain username and password of 160 chracters each.
2)  Top run chat server user must be logged in as ROOT user
3)  Make sure to run on instance of ChatServer otherwise client will behave strange


  /*********************************************************************/
 /*    HOW TO RUN CHATSERVER APPLICATION                              */
/*********************************************************************/

To run the chatsever application perform the following simple steps

1) Switch to root user
2) Change Directory to the folder where ChatServer.c is present
3) Run the script Compile.sh than following output will displayed
   	*******************************************************
	**           Chat Server by Asif and Khurram         **
	*******************************************************

        Chat Server Started Successfully .... 
        Waiting for User connections and Messages:

4) Open another Terminal and run the client application using following command
      >>>./ChatClient localhost
   Than following output will be displayed
 
    	   *******************************************************
	   **           Chat Client                             **
	   *******************************************************

           Welcome to  CHAT SERVER. Please send your username and Password

           Enter Your User Name:

  

5) Enter the username and password and enjoy chatting


        *******************************************************
	   **           Individual tasks                          **
	   *******************************************************
1)  Muhammad Asif
        (i)   Client 
              a. Complete
        (ii)  Server 
        			a.UDP packet Sending on RAW socket 
        			b.User information table
        (iii) Readme file

2)  Khurram Shehzad	   
        (i)  Server
        			a. Threads to handle each client and pseudo-UDP layer
        			b. Mutexes acros shared linked list
        			c. Signal wait and broadcasting with linked list mutex
        			d. Data link sockets receive the UDP packets 
        (ii) MakeFile
