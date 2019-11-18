#ifndef _2310_DEPOT_H_
#define _2310_DEPOT_H_

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <unistd.h>
#include <semaphore.h>
#include "utilities.h"

#define MIN_ARGS 2
#define NAME_POS 1

#define ERROR_ARGS 1
#define ERROR_NAME 2
#define ERROR_QUANTITY 3

#define CON_LIMIT 50

#define DELIVER 0
#define WITHDRAW 1
#define TRANSFER 2
#define DEFER 3
#define EXECUTE 4
#define CONNECT 6
#define IM 5
#define MESSAGE_COUNT 7

#define ADD_ITEM_COUNT 3
#define DELIMITER ":"

#define CONNECT_MSG "IM"

/**
 * Structure to store the depots goods
 * 
 * @param name - The goods description
 * @param quantity - The amount of good to be
 */ 
typedef struct Item {
    char* name;
    int quantity;
} Item;

/**
 * Structure to store deferred messages
 * 
 * @param key - The unique identifier
 * @param messageCount - The number of messages to handle
 * @param messageBuffer - The size of the message array
 * @param messages - A list of all messages to be deferred
 */ 
typedef struct Deferred {
    char* key;
    int messageCount;
    int messageBuffer;
    char** messages;
} Deferred;

/**
 * Structure to store Connections
 * 
 * @param port - The connected port
 * @param name - The name associated with the port
 * @param write - The place to send messages
 * @param read - The place to listen for messages
 */
typedef struct Connection {
    char* port;
    char* name;
    FILE* write;
    FILE* read;
} Connection;

/**
 * Structure to store the hubs information
 * 
 * @param name - The hub's given identifier
 * @param port - The ephemeral port that is connected to
 * @param guard - A semaphore to maintain thread safety
 * @param goods - A list of goods stored in the depot
 * @param itemLength - The number of goods stored in the depot
 * @param itemBuffer - The size of the goods array
 * @param deferrals - A list of messages to be executed in the future
 * @param deferralCount - The number of deferrals stored in the depot
 * @param deferralBuffer - The size of the deferrals array
 * @param con - A list of connection that the depot currently has
 * @param conCount - The number of connections stored in the depot
 * @param conBuffer - The size of the connections array
 */
typedef struct {
    char* name;
    char* port;
    sem_t* guard;
    Item* goods;
    int itemLength;
    int itemBuffer;
    Deferred* deferrals;
    int deferralCount;
    int deferralBuffer;
    Connection* con;
    int conCount;
    int conBuffer;
} Depot;

/**
 * Structure to pass information to threads
 * 
 * @param depot - Information about the hub's state 
 * @param read - The place to listen for messages
 */
typedef struct Worker {
    Depot* depot;
    FILE* read;
} Worker;

/* Core operations */
void output_depot(Depot* depot);
void process_message(Depot* depot, char* message);
void exit_depot(int exitCondition);

/* Sub operations */
void execute_goods(Depot* depot);
void move_goods(Depot* depot, bool act);
void connect_new(Depot* depot);
void defer_goods(Depot* depot);
void transfer_goods(Depot* depot);

/* Initialisations and threads */
void init_server(Depot* depot);
void init_depot(Depot* depot);
void init_worker(Depot* depot, int fdRead);
void* init_thread(void* info);
void* sigmund(void* dep);

/* Processing functions */
void wait_server(Depot* depot, int serv);
void launch_worker(Depot* depot, Connection con);
void launch_depot(Depot* depot);

/* Assisting functions */
int item_order(const void* obj1, const void* obj2);
int con_order(const void* obj1, const void* obj2);
int find_item(Item* goods, int itemLength, char* name);
int find_deferral(Depot* depot, char* key);
bool check_port(Depot* depot, char* portToCheck);
bool add_item(Depot* depot, int quant, char* name);

#endif // _2310_DEPOT_H_
