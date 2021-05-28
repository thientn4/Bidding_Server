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

#include "protocol.h"
#include "linkedlist.h"
#include "auction.h"
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

/////////////////////////////////////HELPER FUNCTION AND GLOBAL VAR//////////////////////////////////////////

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

char* intToStr(int source){
    char* to_return=malloc(1);
    if(source==0){
        *to_return='0';
        *(to_return+1)='\0';
        return to_return;
    }
    int size=1;
    while(source!=0){
        *(to_return+size-1)=source%10+48;
        to_return=realloc(to_return,size+1);
        source/=10;
        size++;
    }
    int iter_flip=0;
    while(iter_flip<(size-1)/2){
        char holder=*(to_return+iter_flip);
        *(to_return+iter_flip)=*(to_return+size-2-iter_flip);
        *(to_return+size-2-iter_flip)=holder;
        iter_flip++;
    }
    *(to_return+size)='\0';
    return to_return;
}

// shared resources
int auction_ID = 1;
List_t* user_list;
List_t* auction_list;
List_t* job_queue;		// job buffer

auction_t* searchAuction(int search_ID){
    node_t* iter=auction_list->head;
    while(iter!=NULL){
        auction_t* cur_auc=(auction_t*)(iter->value);
        if(cur_auc->ID==search_ID)return cur_auc;
        iter=iter->next;
    }
    return NULL;
}

int num_job_thread = 2;	// default number of job threads
int tick_second = 0 ;	// default tick in seconds (when 0 -> tick for each stdin input)
int server_port = -1;
char* auction_file_name = NULL;          

int listen_fd; //server listening file directory
char buffer[BUFFER_SIZE]; //to receive message from client

user_t* server_fake;

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
    	sleep(1);

    printf("ticked!\n");
  
    int i = 0;
    node_t* head = auction_list->head;
    node_t* current = head;
    while (current != NULL) { 
      ((auction_t*)(current->value))->duration = ((auction_t*)(current->value))->duration - 1;
      if (((auction_t*)(current->value))->duration == 0) {
          printf("removing auction with itemname: %s\n",((auction_t*)(current->value))->item_name );
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

////////////////////////////////////////CLIENT THREAD//////////////////////////////////////////////////
/*
short-lived client threads - producer after successful login
            read jobs from protocal messages sent from clients via socket
            insert jobs into queue to be handled in job threads
            clean up and terminate when client terminates the connection
*/
void* client_thread(void* user_ptr){
    user_t* user = (user_t*)user_ptr;
    char client_buffer[BUFFER_SIZE];   /////-------------------->to read message body

    while(1) {
      	job_t* job = (job_t*)malloc(sizeof(job_t));
  		job->requestor = user;
        job->job_protocol=malloc(sizeof(petr_header));
        int err = rd_msgheader(user->file_descriptor, job->job_protocol);
      	if (err == 0) {
            if (job->job_protocol->msg_type == 0x11){ 
                printf("%s have logged out\n",user->username);
                        petr_header* to_send=malloc(sizeof(petr_header));
                        to_send->msg_len=0;
                        to_send->msg_type=0x00;
                        wr_msg(user->file_descriptor,to_send,NULL);
                        free(to_send);
                break;
            }
            else {
                printf("we received a job from client\n");
                if(job->job_protocol->msg_type==0x20||job->job_protocol->msg_type==0x24||job->job_protocol->msg_type==0x26)
                    read(user->file_descriptor, client_buffer, BUFFER_SIZE); /////-------------------->to read message body
                job->job_body = (char*)client_buffer;
                    printf("+---------------new_job_info----------------\n");
                    printf("|       job type: %d\n",job->job_protocol->msg_type);
                    printf("|       requestor name: %s\n",job->requestor->username);
                    printf("+-------------------------------------------\n");
                insertRear(job_queue, job);
            } // end else
        } // end if
        else{
            printf("rd_msgheader has error\n");
        }
    } // end while
    user->is_online = 0;
    close(user->file_descriptor);
    return NULL;
}

//////////////////////////////////////////JOB THREAD///////////////////////////////////////////////////
/*
N job threads - consumer
            are created when server is started
            never terminate and is blocked when there are no jobs to process
            will process jobs and dequeue in FIFO
*/
void* job_thread(){
    petr_header* to_send=malloc(sizeof(petr_header));//////////////////////remember to free this
    while(1){
        sleep(0.00005); ////////////////////////I got segfault if I dont have this line --> not sure why
        if(job_queue != NULL && job_queue->length!=0){
            //get the top job and dequeue
                job_t* cur_job=(job_t*)removeFront(job_queue);////////////////////remember to free this
            //if job is to create new auction
            if(cur_job->job_protocol->msg_type==0x20){
                char* new_item_iter=cur_job->job_body;
                char* new_item_name=cur_job->job_body;
                    while(*new_item_iter!='\n')new_item_iter++;
                    *(new_item_iter-1)='\0';
                char* new_item_duration_str=new_item_iter+1;
                    new_item_iter++;
                    while(*new_item_iter!='\n')new_item_iter++;
                    *(new_item_iter-1)='\0';
                char* new_item_max_str=new_item_iter+1;
                new_item_name=myStrcpy(new_item_name);
                int new_item_duration=atoi(new_item_duration_str);
                int new_item_max=atoi(new_item_max_str);
                //if duration<1 or max_bid<0 or item_name is empty
                if(new_item_duration<1||new_item_max<0||new_item_name==NULL||*new_item_name=='\0'){
                    printf("bid not valid to be created\n");
                    //respond to client with EINVALIDARG
                        to_send->msg_len=0;
                        to_send->msg_type=0x2F;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
                //else
                else{
                    //malloc new auction
                        auction_t* new_auction=malloc(sizeof(auction_t));
                    //set item_name
                        new_auction->item_name=new_item_name;
                    //set duration
                        new_auction->duration=new_item_duration;
                  		if(tick_second!=0)new_auction->duration*=tick_second;////////////////////////wait for confirmation of change in tick thread
                    //set maximum bid
                        new_auction->max_bid_amount=new_item_max;
                    //set other info
                        new_auction->watching_users=malloc(sizeof(List_t));
                        new_auction->creator=cur_job->requestor;
                        new_auction->cur_bid_amount=0;
                        new_auction->ID=auction_ID;
                  	    auction_ID++;
                    //testing
                        printf("+--------------new_bid_info-----------------+\n");
                        printf("|       item_name: %s\n",new_auction->item_name);
                        printf("|       duration: %d seconds\n", new_auction->duration);
                        printf("|       max bid: %d\n",new_auction->max_bid_amount);
                        printf("|       cur_bid: %d\n",new_auction->cur_bid_amount);
                        printf("|       ID: %d\n",new_auction->ID);
                        printf("|       creator: %s\n",new_auction->creator->username);
                        printf("+-------------------------------------------+\n");
                    //add new auction
                        insertFront(auction_list,(void*)new_auction);
                    //respond to client with ANCREATE and new auction's ID
                        char* ID_to_send=intToStr(new_auction->ID);    /////////////remember to convert ID from int to string
                        to_send->msg_len=strlen(ID_to_send);
                        to_send->msg_type=0x20;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,ID_to_send);
                        free(ID_to_send);
                }
            }
            //if job is to list all currently running auctions
                //message body contains jobs with info in the order:
                    //auction ID; item_name; current_highest_bid; number_of_watchers; number of cycles remaining\n --> repeated
                    //auctions must be ordered by lexicographically ascending (sort by auction_id)
            //if job is to watch an auction-------------------------------------------------------------->FOR ABNER TO CHOOSE
                //if provided auction_id does not exist
                    //respond to client with EANOTFOUND
                //else
                    //if auction reached a maximum number of watchers (ignore if we support infinite watchers) --> should ask professor again
                        //respond to client with EANFULL
                    //else
                        //add requester to auction's watcher_list
          	if (cur_job->job_protocol->msg_type == 0x24) {
              	petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
              	return_msg->msg_len = 0;
                            
              	int ID = atoi(cur_job->job_body);
              	auction_t* auc = searchAuction(ID);
              	if (auc == NULL) {
                    return_msg->msg_type = 0x2C;
                  	wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                }
              	else {
                  	if (auc->watching_users->length > 5) {
                      	return_msg->msg_type = 0x2B;
                      	wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                    }
                  	else {
                      	return_msg->msg_type = 0x24;
                      	return_msg->msg_len = strlen(auc->item_name) + 1;
                      	wr_msg(cur_job->requestor->file_descriptor, return_msg, auc->item_name);
                    }
                }
                free(return_msg);
            }
                        //respond to client with ANWATCH and name of item
            //if job is to leave or stop watching an auctions-------------------------------------------------------------->FOR ABNER TO CHOOSE
            if(cur_job->job_protocol->msg_type==0x25){
                    int ID_to_leave=atoi(cur_job->job_body);
                    auction_t* auc_to_leave=searchAuction(ID_to_leave);
                //if provided auction_id does not exist
                if(auc_to_leave==NULL){
                    //respond to client with EANOTFOUND
                        to_send->msg_len=0;
                        to_send->msg_type=0x2C;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
                //else
                else{
                        int index_to_leave=0;
                        node_t* cur_leave_iter=auc_to_leave->watching_users->head;
                    //if requester is in watcher_list of item --> remove him/her
                        while(cur_leave_iter!=NULL){
                            user_t* cur_user=(user_t*)(cur_leave_iter->value);
                            if(strcmp(cur_job->requestor->username,cur_user->username)==0){
                                removeByIndex(auc_to_leave->watching_users,index_to_leave);
                            }
                            cur_leave_iter=cur_leave_iter->next;
                            index_to_leave++;
                        }
                    //respond to client with OK or 0x00
                        to_send->msg_len=0;
                        to_send->msg_type=0x00;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
            }
            //if job is to make a bid
            if(cur_job->job_protocol->msg_type==0x26){
                    char* bid_iter=cur_job->job_body;
                        while(*bid_iter!='\n')bid_iter++;
                        *(bid_iter-1)='\0';
                    int bid_amount=atoi(bid_iter+1);
                    int id_to_bid=atoi(cur_job->job_body);
                //if provided auction_id does not exist
                auction_t* auc_to_bid=searchAuction(id_to_bid);
                if(auc_to_bid==NULL){
                    //respond to client with EANOTFOUND
                        to_send->msg_len=0;
                        to_send->msg_type=0x2C;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
                //else
                else{
                        int is_watching=0;
                        node_t* watcher_iter=auc_to_bid->watching_users->head;
                        while(watcher_iter!=NULL){
                            user_t* cur_watcher=(user_t*)watcher_iter;
                            if(strcmp(cur_watcher->username,cur_job->requestor->username)==0){
                                is_watching=1;
                            }
                            watcher_iter=watcher_iter->next;
                        }
                    //if user is not watching this item or is both requester and creator of this item
                    if(is_watching==0||strcmp(cur_job->requestor->username,auc_to_bid->creator->username)==0){
                        //respond to clietn with EANDENIED
                            to_send->msg_len=0;
                            to_send->msg_type=0x2D;
                            wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                    }
                    else{
                        //if user's bid is lower than current bid
                        if(auc_to_bid->cur_bid_amount>bid_amount){
                            //respond to client with EBIDLOW
                                to_send->msg_len=0;
                                to_send->msg_type=0x2E;
                                wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                        }
                        //if valid
                        else{
                            //update current highest bid and bidder of item
                                auc_to_bid->cur_bid_amount=bid_amount;
                            //respond to client with OK
                                to_send->msg_len=0;
                                to_send->msg_type=0x00;
                                wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                            //send ANUPDATE to all other watchers of the item in form of <auc_id>\r\n<item_name>\r\n<new_bidder_name>\r\n<new bid amount>
                                ////////////////////////REMEMBER TO DO THIS////////////////////////////////
                        }
                    }
                }
            }
            //if job is to list all active user-------------------------------------------------------------->FOR ABNER TO CHOOSE
                //the requestor is not included in the list of active user
                //message body will be in format username1-->newline-->username2-->newline-->...
          	if(cur_job->job_protocol->msg_type == 0x32){
                if (user_list->length == 1) {
                    petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
                    return_msg->msg_len = 0;
                    return_msg->msg_type = 0x32;
                    wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                    free(return_msg);
                }
                else {
                  	petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
                    return_msg->msg_type = 0x32;
                  	
                  	char* msg = (char*)malloc(sizeof(char));
                  	*msg = '\0';
                  
                    node_t* head = user_list->head;
                    node_t* current = head;
                    while (current != NULL) { 
                        user_t* user = (user_t*)current->value;
                      	if (strcmp(user->username, cur_job->requestor->username) != 0) {
                          	msg = (char*)realloc(msg, sizeof(char) * (strlen(msg) + strlen(user->username) + 2));
                          	strcat(msg, user->username);
                          	strcat(msg, "\n");
                        }
                        current = current->next;
                    }
                  	msg = (char*)realloc(msg, sizeof(char) * (strlen(msg) + 1));
                  	strcat(msg, "\0");
                  
                  	return_msg->msg_len = strlen(msg) + 1;
                  	wr_msg(cur_job->requestor->file_descriptor, return_msg, msg);
                  	free(return_msg);
                  	free(msg);
                }
            }
          
            //if job is to list all won auctions of the sender
                //the message body will be in format:
                    //auction_id;item_name;winning_bid\n --> repeated
                    //responded list must be lexicographically ascending by auction_id
          	if(cur_job->job_protocol->msg_type == 0x33){
              	if (cur_job->requestor->won_auctions->length == 0) {
                  	petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
                    return_msg->msg_len = 0;
                    return_msg->msg_type = 0x33;
                    wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                    free(return_msg);
                }
              	else {
                  	petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
                    return_msg->msg_type = 0x33;
                  
                  	char* msg = (char*)malloc(sizeof(char));
                  	*msg = '\0';
                  
                    node_t* head = cur_job->requestor->won_auctions->head;
                    node_t* current = head;
                    while (current != NULL) {
                        auction_t* auction = (auction_t*)current->value;
                      
                      	int ID_length = 1, temp_ID = auction->ID;
                      	while (temp_ID > 0) {		//FROM THIEN---------------> I am not sure what this is for
                          	ID_length += 1;
                          	temp_ID /= 10;
                        }
                      	int bid_amount_length = 1, temp_bid_amount = auction->cur_bid_amount;
                      	while (temp_bid_amount > 0) {		//FROM THIEN---------------> I am not sure what this is for
                          	bid_amount_length += 1;
                          	temp_bid_amount /= 10;
                        }
                      
                      	msg = (char*)realloc(msg, sizeof(char) * (strlen(msg) + ID_length + 2 + strlen(auction->item_name) + 2 + bid_amount_length + 2));                        
                      
                      	char* str = intToStr(auction->ID);
                      	strcat(msg, str);
                      	strcat(msg, "; ");
                      	free(str);
                      	
                      	strcat(msg, auction->item_name);
                      	strcat(msg, "; ");
                      
                      	str = intToStr(auction->cur_bid_amount);
                      	strcat(msg, str);
                      	strcat(msg, "\n");
                      	free(str);
                      	
                        current = current->next;
                    }
                  	msg = (char*)realloc(msg, sizeof(char) * (strlen(msg) + 1));
                  	strcat(msg, "\0");
                  
                  	return_msg->msg_len = strlen(msg) + 1;
                  	return_msg->msg_len = strlen(msg);
                  	wr_msg(cur_job->requestor->file_descriptor, return_msg, msg);
                  	free(return_msg);
                  	free(msg);
                }
            }
            //if job is to list of all created auctions of the sender
                //the message body will be in format:
                    //auction_id;item_name;winning_user;winning_bid\n --> repeated
                    //responded list must be lexicographically ascending by auction_id
            //if job is to show the balance of the sender-------------------------------------------------------------->FOR ABNER TO CHOOSE
                //respond to client with message body:
                    //balance = total sold - total bought
        }
    }
}



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
			server_fake=malloc(sizeof(user_t));
			server_fake->username="fake";
			server_fake->password="fake";
			server_fake->won_auctions=malloc(sizeof(List_t));///////remember to free this
			server_fake->listing_auctions=malloc(sizeof(List_t));//////free this too
			server_fake->balance=0;
			server_fake->file_descriptor=-1;/////////not sure if I should set this to -1
			server_fake->is_online=1;
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
                    if(tick_second!=0)
                      auc->duration*=tick_second; //////////////////// to be suitable with tick thread functionality 
                }
              	else {
                  	auc->max_bid_amount = atoi(cur);
                  	auc->creator = server_fake;
                  	auc->cur_bid_amount = 0;
                  
                  	auc->watching_users = (List_t*)malloc(sizeof(List_t));
                  	auc->watching_users->length = 0;
                  	auc->watching_users->head = NULL;
                  
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
        int iter_job=0;
        while(iter_job<num_job_thread){
            pthread_t job_thread_ID;
            pthread_create(&job_thread_ID, NULL, job_thread, NULL);
            iter_job++;
        }
        
        user_list=(List_t*)malloc(sizeof(List_t));
        job_queue=(List_t*)malloc(sizeof(List_t));
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
                petr_header* to_send=malloc(sizeof(petr_header));//////////////////////remember to free this

    			node_t* user_iter = user_list->head;
                while(user_iter!=NULL){
                    user_t* cur_user=(user_t*)user_iter->value;
                    if(strcmp(cur_user->username,username_check)==0){
                        if(strcmp(cur_user->password,password_check)!=0 || cur_user->is_online==1){
                            //reject connection
                            if(strcmp(cur_user->password,password_check)!=0){
                                //send message with type=0x1B and name=EWRNGPWD
                                    to_send->msg_len=0;
                                    to_send->msg_type=0x1B;
                                    wr_msg(*client_fd,to_send,NULL);
                                    printf("incorrect password\n");
                            }
                            else if (cur_user->is_online==1){
                                //send message with type=0x1A and name=EUSRLGDIN
                                    to_send->msg_len=0;
                                    to_send->msg_type=0x1A;
                                    wr_msg(*client_fd,to_send,NULL);
                                    printf("account is being used\n");
                            }
                        }else{
                                cur_user->file_descriptor=*client_fd;
                                cur_user->is_online=1;
                            //send message with type=0x00 and name=OK
                                to_send->msg_len=0;
                                to_send->msg_type=0x00;
                                wr_msg(*client_fd,to_send,NULL);
                            //create client thread
                                pthread_t clientID;
                                pthread_create(&clientID, NULL, client_thread, (void*)cur_user); 
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
                    //send message with type=0x00 and name=OK
                        to_send->msg_len=0;
                        to_send->msg_type=0x00;
                        wr_msg(*client_fd,to_send,NULL);
                    //create client thread with client_fd as argument to continue communication
                        pthread_t clientID;
                        printf("file descriptor in main: %d\n",new_user->file_descriptor);
                        pthread_create(&clientID, NULL, client_thread, (void*)new_user); 
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
