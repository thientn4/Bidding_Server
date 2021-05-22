typedef struct user {
    char* username;
    char* password;
    List_t* won_auctions;
    List_t* listing_auctions;
    int balance;
    int file_descriptor;
} user_t;

typedef struct auction {
    user_t* creator;
    char* item_name;
    List_t* watching_users;
    int cur_bid_amount;
    int min_bid_amount;
    int ID;
    int duration;
} auction_t;
