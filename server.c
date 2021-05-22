#include "linkedlist.h"
#include "auction.h"
#include <string.h>
#include <stdio.h>

void printInstruction(){
    printf("./bin/zbid_server [-h] [-j N] [-t M] PORT_NUMBER AUCTION_FILENAME.\n\n");
    printf("-h                  Displays this help menu, and returns EXIT_SUCCESS.\n");
    printf("-j N                Number of job threads. If option not specified, default to 2.\n");
    printf("-t M                M seconds between time ticks. If option not specified, default is to wait on input from stdin to indicate a tick.\n");
    printf("PORT_NUMBER         Port number to listen on.\n");
    printf("AUCTION_FILENAME    File to read auction item information from at the start of the server.\n");
}

int ID_for_new;
List_t* user_list;
List_t* aution_list;
List_t* job_queue;
int num_job_thread=2;
int tick_second=0;
int server_port=-1;
char* auction_file_name=NULL;

int main(int argc, char* argv[]) {
    //////////////////////////////////COLLECTING INFO FROM COMMAND INPUT//////////////////////////////
        if(argc<3){
            printInstruction();
            if(argc==2&&strcmp(argv[1],"-h")==0)return EXIT_SUCCESS;
            return EXIT_FAILURE;
        }
        auction_file_name=argv[argc-1];
        server_port=atoi(argv[argc-2]);
        int iter;
        for(iter=1;iter<argc-2;iter++){
            if(strcmp(argv[iter],"-h")==0){
                printInstruction();
                return EXIT_SUCCESS;
            }
            else if(strcmp(argv[iter],"-j")==0){
                iter++;
                if(iter>=argc-2){
                    printInstruction();
                    return EXIT_FAILURE;
                }
                num_job_thread=atoi(argv[iter]);
            }
            else if(strcmp(argv[iter],"-t")==0){
                iter++;
                if(iter>=argc-2){
                    printInstruction();
                    return EXIT_FAILURE;
                }
                tick_second=atoi(argv[iter]);
            }
            else{
                printInstruction();
                return EXIT_FAILURE;
            }
        }
        printf("NUM_JOB_THREAD = %d\n",num_job_thread);
        printf("TICK_SECOND = %d\n",tick_second);
        printf("PORT_NUMBER = %d\n",server_port);
        printf("AUCTION_FILENAME = %s\n",auction_file_name);


    ///////////////////////////////////////////RUN SERVER/////////////////////////////////////////////
    ID_for_new=1;
    user_list=malloc(sizeof(List_t));
    aution_list=malloc(sizeof(List_t));
    job_queue=malloc(sizeof(List_t));
    // when server is started:
    //      N job threads are created
    //      tick thread is created
    //      create shared resources:
    //          User
    //              username
    //              password
    //              list of won auctions
    //              file descriptors
    //              in reader/writer model
    //          Auction ID
    //              shared variable indicating the next available ID starting at 1
    //              in single mutex model
    //          Auctions
    //              auction id
    //              creator's usernae
    //              remainig ticks
    //              set of users watching
    //              in reader/writer model
    //          Job queue
    //              allow:
    //                  creating auction
    //                  make a bid
    //                  view running auctions
    //              in producer/consumer access model
    // 3 type of threads:
    //      1 main thread
    //          loop to wait for client
    //              check username and password
    //                  if new username
    //                      save password and username
    //                      create client thread
    //                  else
    //                      if incorrect password or account currently in use
    //                          reject connection
    //                  else
    //                      create new thread
    //      short-lived client threads - producer after successful login
    //              read jobs from protocal messages sent from clients via socket
    //              insert jobs into queue to be handled in job threads
    //              clean up and terminate when client terminates the connection
    //      N job threads - consumer
    //              are created when server is started
    //              never terminate and is blocked when there are no jobs to process
    //              will process jobs and deque in FIFO
    //      1 time thread (ticks the time left on running auctions - never terminate)
    //              counts down the auctions once every tick cycle
    //              look for ended auctions at the end of each tick
    //                  handle when an auction should end by adding to job queue (producer)
    //              is created when server started
    return 0;
}
