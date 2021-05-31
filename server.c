/*





            not thread safe note: atoi, strlen
            remember to do auction sorting based on ID





            
*/



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

//////////////////////////////////////////////////SHARED RESOURCE///////////////////////////////////////////////

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

user_t* server_fake;

///////////////////////////////////////////////////HELPER FUNCTION///////////////////////////////////////////////////

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
    *(to_return+size-1)='\0';
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
    *(to_return+size-1)='\0';
    return to_return;
}

auction_t* searchAuction(int search_ID){
    node_t* iter=auction_list->head;
    while(iter!=NULL){
        auction_t* cur_auc=(auction_t*)(iter->value);
        if(cur_auc->ID==search_ID)return cur_auc;
        iter=iter->next;
    }
    return NULL;
}

int myStrlen(char* to_count){
    int to_return=0;
    char* count_iter=to_count;
    while(*count_iter!='\0'){
        count_iter++;
        to_return++;
    }
    return to_return;
}

int myAtoi(char* source){
    int to_return=0;
    char* atoi_iter=source;
    while(*atoi_iter!='\0'&&*atoi_iter!='\n'){
        to_return*=10;
        to_return+=*atoi_iter-'0';
        atoi_iter++;
    }
    return to_return;
}

void printMsg(char* input){
    char* iter=input;
    while(*iter!='\0'){
        if(*iter=='\n')printf("\\n");
        else if(*iter=='\r')printf("\\r");
        else printf("%c",*iter);
        iter++;
    }
    printf("\n");
}

int myStrcmp(const char* str1, const char* str2) {
  	while (*str1 && *str2) {
      	if (*str1 != *str2)
            break;
      	str1++;
      	str2++;
	}
  	return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

void myStrcat(char* str1, const char* str2) {
    char* temp = str1 + myStrlen(str1);

    while (*str2 != '\0') {
        *temp = *str2;
        temp++;
        str2++;
    }
    *temp = '\0';
}

int List_tComparator(void* lhs, void* rhs) {
	if (lhs == NULL || rhs == NULL) 
        return 0;

   auction_t* l = (auction_t*)lhs;
   auction_t* r = (auction_t*)rhs;
   return myStrcmp(intToStr(l->ID), intToStr(r->ID));
}

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
  int count_tick=0;
  while(1) {
    count_tick++;
    if (tick_second == 0) {
      getchar();
      // bzero(buffer, BUFFER_SIZE);
      // received_size = read(client_fd, buffer, sizeof(buffer));
    }
    else 
    	sleep(1);

    printf("%d ticked!\n",count_tick);
  
    int i = 0;
    node_t* head = auction_list->head;
    node_t* current = head;
    while (current != NULL) { 
        auction_t* cur_auc=(auction_t*)(current->value);
        cur_auc->duration -= 1;
        if (cur_auc->duration == 0) {
            printf("removing auction with itemname: %s\n",cur_auc->item_name );
            current = current->next;
            removeByIndex(auction_list, i); // removing by index isn't enough: I need to free
            /////////////////////update winner and notify other watcher with 0x22 and message aucID\r\nwinner_name\r\nwin_price or aucID\r\n\r\n
            petr_header* to_send=malloc(sizeof(petr_header));
            char* message;
            printf("check for winner\n");
            user_t* highest=cur_auc->cur_highest_bidder;
            if(highest==NULL){
                printf("no winner for this ended auction\n");
                char* ID_str=intToStr(cur_auc->ID);
                message=malloc(myStrlen(ID_str)+5);
                *message='\0';
                myStrcat(message,ID_str);
                myStrcat(message,"\r\n\r\n");
                to_send->msg_type=0x22;
                to_send->msg_len=myStrlen(message)+1;
                free(ID_str);
            }else{
                    cur_auc->cur_highest_bidder->balance-=cur_auc->cur_bid_amount;
                    insertInOrder(cur_auc->cur_highest_bidder->won_auctions,(void*)cur_auc);
                    cur_auc->creator->balance+=cur_auc->cur_bid_amount;
                    printf("winner %s balance = %d\n",cur_auc->cur_highest_bidder->username,cur_auc->cur_highest_bidder->balance);
                    printf("seller %s balance = %d\n",cur_auc->creator->username,cur_auc->creator->balance);
                printf("user %s won this auction\n",cur_auc->cur_highest_bidder->username);
                char* ID_str=intToStr(cur_auc->ID);
                char* price_str=intToStr(cur_auc->cur_bid_amount);
                message=malloc(myStrlen(ID_str)+myStrlen(price_str)+myStrlen(cur_auc->cur_highest_bidder->username)+5);
                *message='\0';
                myStrcat(message,ID_str);
                myStrcat(message,"\r\n");
                myStrcat(message,cur_auc->cur_highest_bidder->username);
                myStrcat(message,"\r\n");
                myStrcat(message,price_str);
                to_send->msg_type=0x22;
                to_send->msg_len=myStrlen(message)+1;
                free(ID_str);
                free(price_str);
            }
            printf("notify other watchers\n");
            node_t* cur_watch_iter=cur_auc->watching_users->head;
            while(cur_watch_iter!=NULL){
                user_t* cur_watcher=(user_t*)(cur_watch_iter->value);
                wr_msg(cur_watcher->file_descriptor,to_send,message);
                cur_watch_iter=cur_watch_iter->next;
            }
            free(message);
            free(to_send);
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
                job->job_body = NULL;
                if(job->job_protocol->msg_type==0x20||job->job_protocol->msg_type==0x24||job->job_protocol->msg_type==0x25||job->job_protocol->msg_type==0x26){
                    read(user->file_descriptor, client_buffer, BUFFER_SIZE); /////-------------------->to read message body
                    job->job_body = client_buffer;
                }
                    printf("+---------------new_job_info----------------\n");
                    printf("|       job type: %d\n",job->job_protocol->msg_type);
                    printf("|       job body length: %d\n",job->job_protocol->msg_len);
                    printf("|       requestor name: %s\n",job->requestor->username);
                    printf("+---------------client_buffer---------------\n");
                    printMsg(client_buffer);
                    printf("\n");
                    printf("+---------------new_job_body----------------\n");
                    if(job->job_body!=NULL)printf("%s\n",job->job_body);
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
        if(job_queue->length!=0){
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
                int new_item_duration=myAtoi(new_item_duration_str);
                int new_item_max=myAtoi(new_item_max_str);
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
                        new_auction->watching_users->length=0;
                        new_auction->creator=cur_job->requestor;
                        new_auction->cur_bid_amount=0;
                        new_auction->ID=auction_ID;
                        new_auction->cur_highest_bidder=NULL;
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
                        insertInOrder(auction_list,(void*)new_auction);
                        insertInOrder(cur_job->requestor->listing_auctions,(void*)new_auction);
                    //respond to client with ANCREATE and new auction's ID
                        char* ID_to_send=intToStr(new_auction->ID);    /////////////remember to convert ID from int to string
                        to_send->msg_len=myStrlen(ID_to_send)+1;
                        to_send->msg_type=0x20;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,ID_to_send);
                        free(ID_to_send);
                }
            }
            //if job is to list all currently running auctions
                //message body contains auctions with info in the order:
                    //auction ID; item_name; current_highest_bid; number_of_watchers; number of cycles remaining\n --> repeated
                    //auctions must be ordered by lexicographically ascending (sort by auction_id)
            if(cur_job->job_protocol->msg_type == 0x23){
                node_t* auc_list_iter=auction_list->head;
                if(auction_list->length==0){
                    to_send->msg_len=0;
                    to_send->msg_type=0x23;
                    wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
                char* auc_list_message=malloc(1);
                *auc_list_message='\0';
                int auc_list_size=2;
                while(auc_list_iter!=NULL){
                    auction_t* cur_auc=(auction_t*)(auc_list_iter->value);
                    char* cur_ID=intToStr(cur_auc->ID);
                    char* cur_item_name=cur_auc->item_name;
                    char* cur_max_price=intToStr(cur_auc->max_bid_amount);
                    char* cur_watcher_count=intToStr(cur_auc->watching_users->length);
                        int duration_in_tick=cur_auc->duration;
                        if(tick_second!=0){
                            duration_in_tick/=tick_second;
                            if(cur_auc->duration%tick_second!=0)duration_in_tick+=1;
                        }
                    char* cur_highest_bid=intToStr(cur_auc->cur_bid_amount);
                    char* cur_cycles_remain=intToStr(duration_in_tick);
                    printf("%d; %s; %d; %d; %d\n",cur_auc->ID,cur_auc->item_name,cur_auc->cur_bid_amount,cur_auc->watching_users->length,cur_auc->duration);
                    printf("%s; %s; %s; %s; %s\n",cur_ID,cur_item_name,cur_highest_bid,cur_watcher_count,cur_cycles_remain);

                    auc_list_size+=(myStrlen(cur_ID)+myStrlen(cur_item_name)+myStrlen(cur_max_price)+myStrlen(cur_highest_bid)+myStrlen(cur_watcher_count)+myStrlen(cur_cycles_remain)+6);
                    auc_list_message=realloc(auc_list_message,auc_list_size);
                    myStrcat(auc_list_message,cur_ID);
                    myStrcat(auc_list_message,";");
                    myStrcat(auc_list_message,cur_item_name);
                    myStrcat(auc_list_message,";");
                    myStrcat(auc_list_message,cur_max_price);
                    myStrcat(auc_list_message,";");
                    myStrcat(auc_list_message,cur_watcher_count);
                    myStrcat(auc_list_message,";");
                    myStrcat(auc_list_message,cur_highest_bid);
                    myStrcat(auc_list_message,";");
                    myStrcat(auc_list_message,cur_cycles_remain);
                    myStrcat(auc_list_message,"\n");
                    
                    auc_list_iter=auc_list_iter->next;
                }
                myStrcat(auc_list_message,"\0");

                printf("    %d == %d\n",myStrlen(auc_list_message)+1,auc_list_size);
                printf("%s",auc_list_message);

                to_send->msg_len=myStrlen(auc_list_message)+1;
                to_send->msg_type=0x23;
                wr_msg(cur_job->requestor->file_descriptor,to_send,auc_list_message);
                free(auc_list_message);
            }
            //if job is to watch an auction
            if (cur_job->job_protocol->msg_type == 0x24) {
                    petr_header* return_msg = malloc(sizeof(petr_header));
                    return_msg->msg_len = 0;
                                
                    int ID = myAtoi(cur_job->job_body);
                    auction_t* auc = searchAuction(ID);
                //if provided auction_id does not exist
              	if (auc == NULL) {
                    //respond to client with EANOTFOUND
                        return_msg->msg_type = 0x2C;
                  	    wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                }
                //else
              	else {
                    //if auction reached a maximum number of watchers (ignore if we support infinite watchers) --> should ask professor again
                  	if (auc->watching_users->length > 5) {
                        //respond to client with EANFULL
                      	    return_msg->msg_type = 0x2B;
                      	    wr_msg(cur_job->requestor->file_descriptor, return_msg, NULL);
                    }
                    //else
                  	else {
                        //add requester to auction's watcher_list
                            insertRear(auc->watching_users,(void*)(cur_job->requestor));
                        //respond to client with ANWATCH and name of item
                            char* max_bid_str=intToStr(auc->max_bid_amount);
                            char* watch_message=malloc(myStrlen(auc->item_name)+2+myStrlen(max_bid_str));
                            *watch_message='\0';
                            myStrcat(watch_message,auc->item_name);
                            myStrcat(watch_message,"\r\n");
                            myStrcat(watch_message,max_bid_str);
                            return_msg->msg_type = 0x24;
                            return_msg->msg_len = myStrlen(watch_message)+1;
                            wr_msg(cur_job->requestor->file_descriptor, return_msg, watch_message);
                            free(watch_message);
                            free(max_bid_str);
                    }
                }
                free(return_msg);
            }
            //if job is to leave or stop watching an auctions
            if(cur_job->job_protocol->msg_type==0x25){
                    int ID_to_leave=myAtoi(cur_job->job_body);
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
                            if(myStrcmp(cur_job->requestor->username,cur_user->username)==0){
                                removeByIndex(auc_to_leave->watching_users,index_to_leave);
                                break;
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
                    int bid_amount=myAtoi(bid_iter+1);
                    int id_to_bid=myAtoi(cur_job->job_body);
                //if provided auction_id does not exist
                auction_t* auc_to_bid=searchAuction(id_to_bid);
                if(auc_to_bid==NULL){
                    //respond to client with EANOTFOUND
                        printf("we cannot find this bid\n");
                        to_send->msg_len=0;
                        to_send->msg_type=0x2C;
                        wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                }
                //else
                else{
                        int is_watching=0;
                        node_t* watcher_iter=auc_to_bid->watching_users->head;
                        printf("------------wacher of this item----------\n");
                        while(watcher_iter!=NULL){
                            user_t* cur_watcher=(user_t*)(watcher_iter->value);
                            printf("%s\n",cur_watcher->username);
                            if(myStrcmp(cur_watcher->username,cur_job->requestor->username)==0){
                                printf("-->this user is watching this item\n");
                                is_watching=1;
                            }
                            watcher_iter=watcher_iter->next;
                        }
                        printf("-----------------------------------------\n");
                    //if user is not watching this item or is both requester and creator of this item
                    if(is_watching==0||myStrcmp(cur_job->requestor->username,auc_to_bid->creator->username)==0){
                        //respond to clietn with EANDENIED
                            if(is_watching==0)printf("this user is not watching this item\n");
                            if(myStrcmp(cur_job->requestor->username,auc_to_bid->creator->username)==0)printf("this bidder is the bid maker\n");
                            to_send->msg_len=0;
                            to_send->msg_type=0x2D;
                            wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                    }
                    else{
                        //if user's bid is lower than current bid
                        if(auc_to_bid->cur_bid_amount>=bid_amount){
                            //respond to client with EBIDLOW
                                printf("bid is too low\n");
                                to_send->msg_len=0;
                                to_send->msg_type=0x2E;
                                wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                        }
                        //if valid
                        else{
                            //update current highest bid and bidder of item
                                auc_to_bid->cur_bid_amount=bid_amount;
                                auc_to_bid->cur_highest_bidder=cur_job->requestor;
                            //respond to client with OK
                                to_send->msg_len=0;
                                to_send->msg_type=0x00;
                                wr_msg(cur_job->requestor->file_descriptor,to_send,NULL);
                            //send ANUPDATE to all other watchers of the item in form of <auc_id>\r\n<item_name>\r\n<new_bidder_name>\r\n<new bid amount>
                                char* ID_str=intToStr(auc_to_bid->ID);
                                char* cur_bid_str=intToStr(auc_to_bid->cur_bid_amount);
                                char* bid_message=malloc(6+myStrlen(ID_str)+myStrlen(cur_bid_str)+myStrlen(auc_to_bid->item_name)+myStrlen(auc_to_bid->cur_highest_bidder->username));
                                *bid_message='\0';
                                myStrcat(bid_message,ID_str);
                                myStrcat(bid_message,"\r\n");
                                myStrcat(bid_message,auc_to_bid->item_name);
                                myStrcat(bid_message,"\r\n");
                                myStrcat(bid_message,auc_to_bid->cur_highest_bidder->username);
                                myStrcat(bid_message,"\r\n");
                                myStrcat(bid_message,cur_bid_str);
                                to_send->msg_len=myStrlen(bid_message)+1;
                                to_send->msg_type=0x27;
                                node_t* other_bid_iter=auc_to_bid->watching_users->head;
                                while(other_bid_iter!=NULL){
                                    user_t* cur_bidder=(user_t*)(other_bid_iter->value);
                                    wr_msg(cur_bidder->file_descriptor,to_send,bid_message);
                                    other_bid_iter=other_bid_iter->next;
                                }
                                free(cur_bid_str);
                                free(ID_str);
                                free(bid_message);
                        }
                    }
                }
            }
            //if job is to list all active user
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
                      	if (myStrcmp(user->username, cur_job->requestor->username) != 0) {
                          	msg = (char*)realloc(msg, sizeof(char) * (myStrlen(msg) + myStrlen(user->username) + 2));
                          	myStrcat(msg, user->username);
                          	myStrcat(msg, "\n");
                        }
                        current = current->next;
                    }
                  	return_msg->msg_len = myStrlen(msg)+1;
                    printf("message length is %d\n",return_msg->msg_len);
                    printf("%s\n",msg);
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
                  
                  	char* msg = malloc(1);
                  	*msg = '\0';
                  
                    node_t* head = cur_job->requestor->won_auctions->head;
                    node_t* current = head;
                    while (current != NULL) {
                        auction_t* auction = (auction_t*)current->value;
                      
                      	int ID_length = 1, temp_ID = auction->ID;
                      	while (temp_ID > 0) {
                          	ID_length += 1;
                          	temp_ID /= 10;
                        }
                      	int bid_amount_length = 1, temp_bid_amount = auction->cur_bid_amount;
                      	while (temp_bid_amount > 0) {
                          	bid_amount_length += 1;
                          	temp_bid_amount /= 10;
                        }
                      
                      	msg = (char*)realloc(msg, myStrlen(msg) + ID_length + 1 + myStrlen(auction->item_name) + 1 + bid_amount_length + 2);                        
                      
                      	char* str = intToStr(auction->ID);
                      	myStrcat(msg, str);
                      	myStrcat(msg, ";");
                      	free(str);
                      	
                      	myStrcat(msg, auction->item_name);
                      	myStrcat(msg, ";");
                      
                      	str = intToStr(auction->cur_bid_amount);
                      	myStrcat(msg, str);
                      	myStrcat(msg, "\n");
                      	free(str);
                      	
                        current = current->next;
                    }
                  	msg = (char*)realloc(msg, myStrlen(msg) + 1);
                  	myStrcat(msg, "\0");
                  
                  	return_msg->msg_len = myStrlen(msg) + 1;
                  	wr_msg(cur_job->requestor->file_descriptor, return_msg, msg);
                  	free(return_msg);
                  	free(msg);
                }
            }
            //if job is to list of all created auctions of the sender--------------------------> does not work
                //the message body will be in format:
                    //auction_id;item_name;winning_user;winning_bid\n --> repeated
                    //responded list must be lexicographically ascending by auction_id
            if (cur_job->job_protocol->msg_type == 0x34){
                printf("we need to make a list of ended auctions for %s\n",cur_job->requestor->username);
              	if (cur_job->requestor->listing_auctions->length == 0) {
                    printf("no listing auction for %s\n",cur_job->requestor->username);
                  	to_send->msg_len = 0;
                    to_send->msg_type = 0x34;
                    wr_msg(cur_job->requestor->file_descriptor, to_send, NULL);
                    free(to_send);
                }
              	else {
                    printf("there are listing auctions for %s\n",cur_job->requestor->username);
                  
                  	char* msg = (char*)malloc(1);
                  	*msg = '\0';
                    int size=1;
                  
                    node_t* current = cur_job->requestor->listing_auctions->head;
                    while (current != NULL) {
                        auction_t* cur_auc=(auction_t*)(current->value);
                        if(cur_auc->duration==0){
                            char* ID_str=intToStr(cur_auc->ID);
                            char* win_price_str=intToStr(cur_auc->cur_bid_amount);
                            size+=myStrlen(ID_str);
                            size+=myStrlen(cur_auc->item_name);
                            size+=myStrlen(cur_auc->cur_highest_bidder->username);
                            size+=myStrlen(win_price_str);
                            size+=4;
                            msg=realloc(msg,size);
                            myStrcat(msg,ID_str);
                            myStrcat(msg,";");
                            myStrcat(msg,cur_auc->item_name);
                            myStrcat(msg,";");
                            myStrcat(msg,cur_auc->cur_highest_bidder->username);
                            myStrcat(msg,";");
                            myStrcat(msg,win_price_str);
                            myStrcat(msg,"\n");
                        }
                        current=current->next;
                    }
                    to_send->msg_len=myStrlen(msg)+1;
                    to_send->msg_type=0x34;
                    printf("<%s>\n",msg);
                    wr_msg(cur_job->requestor->file_descriptor,to_send,msg);
                }
            }
            //if job is to show the balance of the sender
                //respond to client with message body:
                    //balance = total sold - total bought
            if (cur_job->job_protocol->msg_type == 0x35) {
              	char* bal;
              	petr_header* return_msg = (petr_header*)malloc(sizeof(petr_header));
                if(cur_job->requestor->balance>=0){
                    bal = intToStr(cur_job->requestor->balance);
                    return_msg->msg_len = myStrlen(bal) + 1;
                    return_msg->msg_type = 0x35;
                    wr_msg(cur_job->requestor->file_descriptor, return_msg, bal);
                }
                else{
                    bal = intToStr(cur_job->requestor->balance*-1);
                    char* neg_bal=malloc(myStrlen(bal)+1);
                    *neg_bal='-';
                    *(neg_bal+1)='\0';
                    myStrcat(neg_bal,bal);
                    return_msg->msg_len = myStrlen(neg_bal) + 1;
                    return_msg->msg_type = 0x35;
                    wr_msg(cur_job->requestor->file_descriptor, return_msg, neg_bal);
                    free(neg_bal);
                }
                free(return_msg);
              	free(bal);
            }
        }
    }
}

void printTest(){
    node_t* curNode=auction_list->head;
    while(curNode!=NULL){
        auction_t* curAuc=(auction_t*)(curNode->value);
        printf("iter_name: %s\n",curAuc->item_name);
        printf("    ID: %d\n",curAuc->ID);
        printf("    duration: %d\n",curAuc->duration);
        printf("    max_bid_amount: %d\n",curAuc->max_bid_amount);
        printf("    cur_bid_amount: %d\n",curAuc->cur_bid_amount);
        curNode=curNode->next;
    }
}

int main(int argc, char* argv[]) {
    ///////////////////////////////////PARSING INPUT COMMAND///////////////////////////////////////////
        if (argc < 3){
            printInstructions();
            if (argc == 2 && myStrcmp(argv[1], "-h") == 0)
              	return EXIT_SUCCESS;
            return EXIT_FAILURE;
        }

        auction_file_name = argv[argc-1];
        server_port = myAtoi(argv[argc-2]);
  
        int iter;
        for(iter = 1; iter < argc-2; iter++) {
            if (myStrcmp(argv[iter], "-h") == 0) {
                printInstructions();
                return EXIT_SUCCESS;
            }
            else if (myStrcmp(argv[iter], "-j") == 0) {
                iter++;
                if (iter >= argc-2) {
                    printInstructions();
                    return EXIT_FAILURE;
                }
                num_job_thread = myAtoi(argv[iter]);
            }
            else if (myStrcmp(argv[iter], "-t") == 0) {
                iter++;
                if(iter >= argc-2) {
                    printInstructions();
                    return EXIT_FAILURE;
                }
                tick_second = myAtoi(argv[iter]);
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
            server_fake->won_auctions->comparator= List_tComparator;
			server_fake->listing_auctions=malloc(sizeof(List_t));//////free this too
            server_fake->listing_auctions->comparator= List_tComparator;
			server_fake->balance=0;
			server_fake->file_descriptor=-1;/////////not sure if I should set this to -1
			server_fake->is_online=1;
        auction_list = (List_t*)malloc(sizeof(List_t));
        auction_list->comparator= List_tComparator;
  		// if auction_file_name == NULL, ignore
  		if (auction_file_name != NULL) {
          	// opens file, prefills auctions list
          	FILE* fp = fopen(auction_file_name, "r");
            if (fp == NULL) {
                return EXIT_FAILURE;
            }
          	
          	int i = 1;
          	char* cur = (char*)malloc(sizeof(char));				// current row in file
          	auction_t* auc = malloc(sizeof(auction_t));	// auction information
          	while (fgets(cur, 100, fp) != NULL) {
            	if ((i % 4) == 0) {
                	auc = (auction_t*)malloc(sizeof(auction_t));
                    i = 0;
                }
              	else if (i == 1) {
                  	char* temp_cur = (char*)malloc(sizeof(char) * (myStrlen(cur) + 1));
                    temp_cur=myStrcpy(cur);
                    auc->item_name = temp_cur;
                    *(temp_cur+myStrlen(temp_cur)-1)='\0';
                  
                  	auc->ID = auction_ID;
                  	auction_ID++;
                }
              	else if (i == 2) {
                  	auc->duration = myAtoi(cur);
                    if(tick_second!=0)
                      auc->duration*=tick_second; //////////////////// to be suitable with tick thread functionality 
                }
              	else {
                  	auc->max_bid_amount = myAtoi(cur);
                  	auc->creator = server_fake;
                  	auc->cur_bid_amount = 0;
                  	auc->watching_users = malloc(sizeof(List_t));
                    auc->cur_highest_bidder=NULL;
                    printf("cur_highest_bidder=NULL\n");
              		insertInOrder(auction_list, (void*)auc);
                }
              	i++;
          	}
          	auc = NULL; 	// avoiding future error 
        }

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


            printTest();

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
                    if(myStrcmp(cur_user->username,username_check)==0){
                        if(myStrcmp(cur_user->password,password_check)!=0 || cur_user->is_online==1){
                            //reject connection
                            if(myStrcmp(cur_user->password,password_check)!=0){
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
                        new_user->won_auctions->comparator= List_tComparator;
                        new_user->listing_auctions=malloc(sizeof(List_t));
                        new_user->listing_auctions->comparator= List_tComparator;
                        new_user->file_descriptor=*client_fd;
                        new_user->balance=0;
                        new_user->is_online=1;
                        insertRear(user_list,new_user);
                        printf("+-----------new_user_info---------------\n");
                        printf("|   username: %s\n",new_user->username);
                        printf("|   password: %s\n",new_user->password);
                        printf("|   file_descriptor: %d\n",new_user->file_descriptor);
                        printf("+---------------------------------------\n");
                    //send message with type=0x00 and name=OK
                        to_send->msg_len=0;
                        to_send->msg_type=0x00;
                        wr_msg(*client_fd,to_send,NULL);
                    //create client thread with client_fd as argument to continue communication
                        pthread_t clientID;
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
