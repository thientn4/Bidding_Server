typedef struct user {
    char* username;
    char* password;
    List_t* won_auctions;
    List_t* listing_auctions;
    int balance;
    int file_descriptor;
    int is_online;
    sem_t mutex;
} user_t;

typedef struct auction {
    user_t* creator;
    user_t* cur_highest_bidder;
    char* item_name;
    List_t* watching_users;
    int cur_bid_amount;
    int max_bid_amount;
    int ID;
    int duration;
    sem_t mutex;
} auction_t;

typedef struct job{
    petr_header* job_protocol;  //type of job to decide what to do
    char* job_body;             //extra argument if needed for job such as auction creating
    user_t* requestor;           //to send result to the client requesting the job
}job_t;
