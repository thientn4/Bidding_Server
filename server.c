/////////////FOR SOCKET
    #include <getopt.h>
    #include <netdb.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>

    #define BUFFER_SIZE 1024
    #define SA struct sockaddr

#include "linkedlist.h"
#include "auction.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

void printInstructions(){
    printf("./bin/zbid_server [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME.\n\n");
    printf("-h                  Displays this help menu, and returns EXIT_SUCCESS.\n");
    printf("-j N                Number of job threads. If option not specified, default to 2.\n");
    printf("-t M                M seconds between time ticks. If option not specified, default is to wait on input from stdin to indicate a tick.\n");
    printf("PORT_NUMBER         Port number to listen on.\n");
    printf("AUCTION_FILENAME    File to read auction item information from at the start of the server.\n");
}

char* myStrcpy(char* source){
    char* to_return=malloc(1);
    int size=1;
    while(*source!='\0'){
        *(to_return+size-1)=*source;
        to_return=realloc(to_return,size+1);
        source++;
        size++;
    }
    *(to_return+size)='\0';
    return to_return;
}

// shared resources
int auction_ID = 1;
List_t* user_list;
List_t* auction_list;
List_t* job_queue;		// job buffer

int num_job_thread = 2;	// default number of job threads
int tick_second = 0 ;	// default tick in seconds (when 0 -> tick for each stdin input)
int server_port = -1;
char* auction_file_name = NULL;

int listen_fd; //server listening file directory
char buffer[BUFFER_SIZE]; //to receive message from client

/////////////////////////////////////INITIATE SOCKET IN SERVER//////////////////////////////////////////
    int server_init(int server_port){
        int sockfd;
        struct sockaddr_in servaddr;

        // socket create and verification
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            printf("socket creation failed...\n");
            exit(EXIT_FAILURE);
        }
        else
            printf("Socket successfully created\n");

        bzero(&servaddr, sizeof(servaddr));

        // assign IP, PORT
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(server_port);

        int opt = 1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
        {
        perror("setsockopt");exit(EXIT_FAILURE);
        }

        // Binding newly created socket to given IP and verification
        if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
            printf("socket bind failed\n");
            exit(EXIT_FAILURE);
        }
        else
            printf("Socket successfully binded\n");

        // Now server is ready to listen and verification
        if ((listen(sockfd, 1)) != 0) {
            printf("Listen failed\n");
            exit(EXIT_FAILURE);
        }
        else
            printf("Server listening on port: %d.. Waiting for connection\n", server_port);

        return sockfd;
    }

////////////////////////////////////////TICK THREAD////////////////////////////////////////////////////

void* tick_thread() {
  // int client_fd = *(int*)clientfd_ptr;
  
  while(1) {
    if (tick_second == 0) {
      getchar();
      // bzero(buffer, BUFFER_SIZE);
      // received_size = read(client_fd, buffer, sizeof(buffer));
    }
    else 
    	sleep(tick_second);

    printf("ticked!\n");
  
    int i = 0;
    node_t* head = auction_list->head;
    node_t* current = head;
    while (current != NULL) { 
      ((auction_t*)(current->value))->duration = ((auction_t*)(current->value))->duration - 1;
      if (((auction_t*)(current->value))->duration == 0) {
          current = current->next;
          removeByIndex(auction_list, i); // removing by index isn't enough: I need to free
      }
      else {
          current = current->next;
          i += 1;
      }
    } // end inner while
  } // end outer while
  
  // close(client_fd);
  
} // end tick_thread



int main(int argc, char* argv[]) {
    ///////////////////////////////////PARSING INPUT COMMAND///////////////////////////////////////////
        if (argc < 3){
            printInstructions();
            if (argc == 2 && strcmp(argv[1], "-h") == 0)
              	return EXIT_SUCCESS;
            return EXIT_FAILURE;
        }

        auction_file_name = argv[argc-1];
        server_port = atoi(argv[argc-2]);
  
        int iter;
        for(iter = 1; iter < argc-2; iter++) {
            if (strcmp(argv[iter], "-h") == 0) {
                printInstructions();
                return EXIT_SUCCESS;
            }
            else if (strcmp(argv[iter], "-j") == 0) {
                iter++;
                if (iter >= argc-2) {
                    printInstructions();
                    return EXIT_FAILURE;
                }
                num_job_thread = atoi(argv[iter]);
            }
            else if (strcmp(argv[iter], "-t") == 0) {
                iter++;
                if(iter >= argc-2) {
                    printInstructions();
                    return EXIT_FAILURE;
                }
                tick_second = atoi(argv[iter]);
            }
            else {
                printInstructions();
                return EXIT_FAILURE;
            }
        }

        //--------------------------------------------------------------TESTING
        //printf("auction_file_name = %s\n",auction_file_name);

    ////////////////////////////////PREFILLING LIST AND INITIALISE GLOBAL VAR/////////////////////////////
        auction_list = (List_t*)malloc(sizeof(List_t));
  		// if auction_file_name == NULL, ignore
  		if (auction_file_name != NULL) {
          	// opens file, prefills auctions list
          	FILE* fp = fopen(auction_file_name, "r");
            if (fp == NULL) {
                return EXIT_FAILURE;
            }
          	
          	int i = 1;
          	char* cur = (char*)malloc(sizeof(char));				// current row in file
          	auction_t* auc = (auction_t*)malloc(sizeof(auction_t));	// auction information
          	while (fgets(cur, 100, fp) != NULL) {
            	if ((i % 4) == 0) {
                	auc = (auction_t*)malloc(sizeof(auction_t));
                    i = 0;
                }
              	else if (i == 1) {
                  	char* temp_cur = (char*)malloc(sizeof(char) * (strlen(cur) + 1));
                    strcpy(temp_cur, cur);
                    auc->item_name = temp_cur;
                  
                  	auc->ID = auction_ID;
                  	auction_ID++;
                }
              	else if (i == 2) {
                  	auc->duration = atoi(cur);
                }
              	else {
                  	auc->min_bid_amount = atoi(cur);
              		insertRear(auction_list, (void*)auc);
                }
              	i++;
          	}
          	free(auc);		// freeing last, unused auc
          	auc = NULL; 	// avoiding future error 
        }

        /*---------------------------------------------------------------TESTING
        node_t* curNode=auction_list->head;
        while(curNode!=NULL){
            auction_t* curAuc=(auction_t*)curNode->value;
            printf("iter_name: %s",curAuc->item_name);
            printf("    ID: %d\n",curAuc->ID);
            printf("    duration: %d\n",curAuc->duration);
            printf("    min_bid_amount: %d\n",curAuc->min_bid_amount);
            curNode=curNode->next;
        }
        */

    /////////////////////////////////////////RUN SERVER////////////////////////////////////////////////
        //spawn tick thread and N job threads
            pthread_t tickID;
            pthread_create(&tickID, NULL, tick_thread, NULL); 
        user_list=(List_t*)malloc(sizeof(List_t));
        listen_fd = server_init(server_port); // Initiate server and start listening on specified port
        int client_fd;
        struct sockaddr_in client_addr;
        unsigned int client_addr_len = sizeof(client_addr);

        pthread_t tid;

        while(1){
            // Wait and Accept the connection from client
            printf("Wait for new client connection\n");
            int* client_fd = malloc(sizeof(int));
            *client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
            if (*client_fd < 0) {
                printf("server acccept failed\n");
                exit(EXIT_FAILURE);
            }
            else{
                printf("Client connetion accepted\n");
                bzero(buffer, BUFFER_SIZE);
                read(*client_fd, buffer, BUFFER_SIZE);
                char* msgbody = buffer+8;
                char* username_check=msgbody;
                char* password_check=msgbody;
                while(*password_check!='\n'){
                    password_check++;
                }
                *(password_check-1)='\0';
                password_check+=1;

                printf("------------------------username received: %s\n",username_check);
                printf("------------------------password received: %s\n",password_check);

                int is_new_account=1;

    			node_t* user_iter = user_list->head;
                while(user_iter!=NULL){
                    user_t* cur_user=(user_t*)user_iter->value;
                    if(strcmp(cur_user->username,username_check)==0){
                        if(strcmp(cur_user->password,password_check)!=0 || cur_user->is_online==1){
                            //reject connection
                            if(strcmp(cur_user->password,password_check)!=0){
                                printf("incorrect password\n");
                            }
                            if (cur_user->is_online==1){
                                printf("account is being used\n");
                            }
                        }else{
                            //create client thread
                            printf("existing account logged in\n");
                        }
                        is_new_account=0;
                        break;
                    }
                    user_iter=user_iter->next;
                }

                if(is_new_account==1){
                    //create new user and add to user list
                        user_t* new_user=malloc(sizeof(user_t));
                        new_user->username=myStrcpy(username_check);
                        new_user->password=myStrcpy(password_check);
                        new_user->won_auctions=malloc(sizeof(List_t));
                        new_user->listing_auctions=malloc(sizeof(List_t));
                        new_user->file_descriptor=*client_fd;
                        new_user->balance=0;
                        new_user->is_online=1;
                        insertRear(user_list,new_user);
                    //create client thread with client_fd as argument to continue communication
                        printf("new account logged in\n");
                }
                
            }
        }
        close(listen_fd);

    /*/////////////////////////////////////////////////////////////////////////////////////////////////
        1 main thread
            loop to wait for client
                check username and password
                if new username
                    save password and username
                    create client thread
                else
                    if incorrect password or account currently in use
                        reject connection
                    else
                        create client thread
        short-lived client threads - producer after successful login
            read jobs from protocal messages sent from clients via socket
            insert jobs into queue to be handled in job threads
            clean up and terminate when client terminates the connection
        N job threads - consumer
            are created when server is started
            never terminate and is blocked when there are no jobs to process
            will process jobs and dequeue in FIFO
        1 time thread (ticks the time left on running auctions - never terminate)
            counts down the auctions once every tick cycle
            look for ended auctions at the end of each tick
            handle when an auction should end by adding to job queue (producer)
            is created when server started
    *//////////////////////////////////////////////////////////////////////////////////////////////////

  	return 0;
}
