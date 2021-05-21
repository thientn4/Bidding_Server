typedef struct user {
    char* username;
    char* password;
    List_t* wonAuctions;
    List_t* listingAuctions;
    int balance;
    int fileDescriptor;
} user_t;

typedef struct auction {
    user_t* creator;
    char* itemName;
    List_t* watchingUsers;
    int curBidAmount;
    int ID;
    int remainingTick;
} auction_t;
