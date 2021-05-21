#include "linkedlist.h"
#include "auction.h"

int IDforNew=1;
List_t* userList=malloc(sizeof(List_t));
List_t* autionList=malloc(sizeof(List_t));
List_t* jobQueue=malloc(sizeof(List_t));

int main() {
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
