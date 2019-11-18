#include "depot.h"


int main(int argc, char** argv) {
    if (argc < MIN_ARGS || argc % 2 != 0) {
        exit_depot(ERROR_ARGS);
    } else if (!check_name(argv[NAME_POS])) {
        exit_depot(ERROR_NAME);
    }

    // Initialise semphore and depot
    sem_t lock;
    sem_init(&lock, 0, 1);

    Depot depot;
    depot.guard = &lock;

    depot.name = strdup(argv[NAME_POS]);

    init_depot(&depot);

    // Check command line arguments
    int quant;
    for (int i = MIN_ARGS; i < argc; i += 2) {
        if (strlen(argv[i + 1]) == 0 || (quant = read_int(argv[i + 1])) < 0) {
            exit_depot(ERROR_QUANTITY);
        } else if (!add_item(&depot, quant, argv[i])) {
            exit_depot(ERROR_NAME);
        }
    }

    launch_depot(&depot);

    exit_depot(NORMAL_EXIT);
}

/**
 * Create a thread to listen for signals then use the depot.
 * 
 * @param depot - Information about the hub's state 
 */ 
void launch_depot(Depot* depot) {
    pthread_t tid;
    sigset_t set;

    // Pass signal handling to a thread
    sigemptyset(&set);
    sigaddset(&set, SIGHUP); // Only handles HUP and PIPE.
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, 0);  
    pthread_create(&tid, 0, sigmund, depot); 

    init_server(depot);
}

/**
 * Initialise all data structures in the hub
 * 
 * @param depot - Information about the hub's state 
 */ 
void init_depot(Depot* depot) {
    depot->deferralCount = 0;
    depot->deferralBuffer = ARRAY_BUFFER;
    depot->deferrals = malloc(sizeof(Deferred) * depot->deferralBuffer);

    depot->itemLength = 0;
    depot->itemBuffer = ARRAY_BUFFER;
    depot->goods = malloc(sizeof(Item) * depot->itemBuffer);

    depot->conCount = 0;
    depot->conBuffer = ARRAY_BUFFER;
    depot->con = malloc(sizeof(Connection) * depot->conBuffer);
}

/**
 * Thread handler for signals
 * 
 * @param dep - A reference to the hub's data.
 */ 
void* sigmund(void* dep) {
    Depot* depot = (Depot*) dep;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGPIPE);

    int num;
    while (!sigwait(&set, &num)) {  // block here until a signal arrives
        if (num == SIGPIPE) {
            continue; // Ignore SIGPIPE
        }
        output_depot(depot); // Only SIGHUP gets here
    }
    return 0;
}

/**
 * Thread handler for new connections
 * 
 * @param info - Data to be shared with the thread
 */ 
void* init_thread(void* info) {
    Worker* worker = (Worker*) info; 

    // Wait for input from the connected hub and then act on it.
    char* line;
    while (read_line(worker->read, &line)) {
        if (strlen(line) == 0) {
            continue;
        }

        sem_wait(worker->depot->guard);
        process_message(worker->depot, line);
        sem_post(worker->depot->guard);

        free(line);
    }

    return 0;
}

/**
 * Bind the hub to a given port and listen for new connections
 * 
 * @param depot - Information about the hub's state 
 */ 
void init_server(Depot* depot) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Because we want to bind with it  

    int err;
    if ((err = getaddrinfo("localhost", 0, &hints, &ai))) { 
        freeaddrinfo(ai);
        return;   // could not work out the address
    }
    
    // create a socket and bind it to a port
    int serv = socket(AF_INET, SOCK_STREAM, 0); // default protocol
    if (bind(serv, (struct sockaddr*) ai->ai_addr, sizeof(struct sockaddr))) {
        return;
    }
    
    // Find the ephemeral port given
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr*) &ad, &len)) {
        return;
    }

    printf("%u\n", ntohs(ad.sin_port));
    fflush(stdout);

    // Save the port to the depot
    string_of(ntohs(ad.sin_port), &depot->port);
    
    wait_server(depot, serv);
}

/**
 * Set the connection limit and wait on new connections.
 * 
 * @param depot - Information about the hub's state 
 * @param serv - The socket information
 */ 
void wait_server(Depot* depot, int serv) {
    // Set the number of concurrent connections
    if (listen(serv, CON_LIMIT)) {
        return;
    }

    // Wait for new connections
    int fd;
    while (fd = accept(serv, NULL, NULL), fd >= 0) {
        sem_wait(depot->guard);
        init_worker(depot, fd);
        sem_post(depot->guard);
    }
}

/**
 * Create a new worker to read and send messages
 * 
 * @param depot - Information about the hub's state 
 * @param fdRead - The file descriptor to talk to
 */ 
void init_worker(Depot* depot, int fdRead) {
    int fdWrite = dup(fdRead);
    Connection con;

    FILE* read = fdopen(fdRead, "r");
    FILE* write = fdopen(fdWrite, "w");

    con.write = write;
    con.read = read;

    fprintf(con.write, "%s:%s:%s\n", CONNECT_MSG, depot->port, depot->name);
    fflush(con.write);
    launch_worker(depot, con);
}

/**
 * Create a thread to listen for commands from a given connection
 * 
 * @param con - Information about the connection
 * @param depot - Information about the hub's state 
 */ 
void launch_worker(Depot* depot, Connection con) {
    char* line;
    if (read_line(con.read, &line) && strlen(line) != 0
            && !strcmp(strtok(line, DELIMITER), CONNECT_MSG) 
            && (con.port = strtok(NULL, DELIMITER)) 
            && (con.name = strtok(NULL, DELIMITER)) 
            && !strtok(NULL, DELIMITER)
            && check_port(depot, con.port)) {

        Worker* newWork = malloc(sizeof(Worker));
        newWork->read = con.read;
        newWork->depot = depot;
        
        // Make the new thread ignore SIGHUP and SIGPIPE.
        pthread_t tid;
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigaddset(&set, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &set, 0);  

        // Reallocate connection memory if oversized.
        if (depot->conCount == depot->conBuffer) {
            depot->conBuffer *= 2;
            depot->con = realloc(depot->deferrals, 
                    sizeof(Connection) * depot->deferralBuffer);
        }
        depot->con[depot->conCount++] = con;

        pthread_create(&tid, 0, init_thread, newWork);
    } else {
        // Close the connection
        fclose(con.read);
        fclose(con.write);
    }
}


/**
 * Handle a message within the hub.
 * 
 * @param depot - Information about the hub's state 
 * @param message - The message to analyse
 */ 
void process_message(Depot* depot, char* message) {
    static const char* messages[] = {"Deliver", "Withdraw", "Transfer", 
            "Defer", "Execute", "IM", "Connect"};

    char* action = strtok(message, ":");

    // Check what message has been recieved
    for (int i = 0; i < MESSAGE_COUNT; i++) {
        if (action && !strcmp(action, messages[i])) {
            switch(i) {
                case WITHDRAW:
                case DELIVER:
                    move_goods(depot, i == DELIVER);
                    return;
                case TRANSFER:
                    transfer_goods(depot);
                    return;
                case EXECUTE:
                    execute_goods(depot);
                    return;
                case DEFER:
                    defer_goods(depot);
                    return;
                case CONNECT:
                    connect_new(depot);
                    return;
            }
        }
    }
}

/**
 * Transfer goods from one depot to another
 * 
 * @param depot - Information about the hub's state 
 */ 
void transfer_goods(Depot* depot) {
    int quantity;
    char* item;
    char* destination;

    // Read arguments from strtok
    if (!(quantity = read_int(strtok(NULL, DELIMITER))) || quantity <= 0 
            || !(item = strtok(NULL, DELIMITER)) 
            || !(destination = strtok(NULL, DELIMITER))
            || strtok(NULL, DELIMITER)) {
        return;
    }

    // Find the correct depot and send the data
    for (int i = 0; i < depot->conCount; i++) {
        if (!strcmp(destination, depot->con[i].name)) {
            fprintf(depot->con[i].write, "Deliver:%d:%s\n", quantity, item);
            fflush(depot->con[i].write);
            // Update internal counts.
            add_item(depot, -quantity, item);
            break;
        }
    }

}

/**
 * Find the index of a given deferred key. Otherwise give the max index.
 * 
 * @param depot - Information about the hub's state 
 * @param key - The key to compare with
 */ 
int find_deferral(Depot* depot, char* key) {
    for (int i = 0; i < depot->deferralCount; i++) {
        if (!strcmp(depot->deferrals[i].key, key)) {
            // Add more memory if necessary
            if (depot->deferrals[i].messageCount == 
                    depot->deferrals[i].messageBuffer) {
                depot->deferrals[i].messageBuffer *= 2;

                depot->deferrals[i].messages = realloc(
                        depot->deferrals[i].messages, sizeof(char*) * 
                        depot->deferrals[i].messageBuffer);               
            }
            return i;
        }
    }
    return depot->deferralCount;
}

/**
 * Defer a message for execution at a later date
 * 
 * @param depot - Information about the hub's state 
 * @param message - The instructions to defer.
 */ 
void defer_goods(Depot* depot) {
    char* key;
    char* message;

    // Read the key and message from strtok
    if (!(key = strtok(NULL, DELIMITER)) || strlen(key) <= 0 
            || read_int(key) < 0 || !(message = strtok(NULL, "")) 
            || strtok(NULL, "")) {
        return;
    }

    int keyIndex = find_deferral(depot, key);

    // Create a new deferral if necessary
    if (keyIndex == depot->deferralCount) {
        // Add more memory if necessary
        if (depot->deferralCount++ == depot->deferralBuffer) {
            depot->deferralBuffer *= 2;
            depot->deferrals = realloc(depot->deferrals, 
                    sizeof(Deferred) * depot->deferralBuffer);
        }
        Deferred def;
        def.key = strdup(key);
        def.messageCount = 0;
        def.messageBuffer = ARRAY_BUFFER;
        def.messages = malloc(sizeof(char*) * def.messageBuffer);
        depot->deferrals[keyIndex] = def;
    }

    // Save the message to hub
    depot->deferrals[keyIndex].messages[
            depot->deferrals[keyIndex].messageCount++] = strdup(message);
}

/**
 * Enact on all deferred messages with a given key.
 * 
 * @param depot - Information about the hub's state 
 */ 
void execute_goods(Depot* depot) {
    char* key;
    Deferred* def;

    // Read key from strtok
    if (!(key = strtok(NULL, DELIMITER)) || strtok(NULL, DELIMITER)) {
        return;
    }

    /* Find the corresponding key, attempt to execute all messages, 
    clearing them after execution. */
    int keyIndex = find_deferral(depot, key);
    if (keyIndex != depot->deferralCount) {
        def = &depot->deferrals[keyIndex];

        for (int i = 0; i < def->messageCount; i++) {
            process_message(depot, def->messages[i]);
            free(def->messages[i]);
        }

        def->messageCount = 0;
    }
}

/**
 * Read the message information from strtok and attempt to connect to the port.
 * 
 * @param depot - Information about the hub's state 
 */ 
void connect_new(Depot* depot) {
    char* port; 

    // Read port from strtok and check if it is a new port.
    if ((!(port = strtok(NULL, DELIMITER)) && strtok(NULL, DELIMITER)) 
            || !check_port(depot, port)) {
        return;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv6  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM; // Allow TCP exclude UDP
    hints.ai_flags = AI_PASSIVE;  // Because we want to bind with it  

    struct addrinfo* ai = 0; // Returns a linked list of addrinfo

    int err = getaddrinfo("localhost", port, &hints, &ai);

    if (err) { // no particular port
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return;   // could not work out the address
    }
    
    // create a socket and bind it to a port - check args later
    int fd = socket(AF_INET, SOCK_STREAM, 0); // default protocol
    if (connect(fd, (struct sockaddr*) ai->ai_addr, 
            sizeof(struct sockaddr))) {
        perror("Connecting");
        return;
    }

    init_worker(depot, fd);
}

/**
 * Read goods from strtok and add them to the depot
 * 
 * @param depot - Information about the hub's state 
 * @param act - Whether goods are added (T) or removed (F)
 */ 
void move_goods(Depot* depot, bool act) {
    int quantity;
    char* name;
    // Read args from strtok
    if ((quantity = read_int(strtok(NULL, DELIMITER))) > 0
            && (name = strtok(NULL, DELIMITER)) && !strtok(NULL, DELIMITER)) {
        quantity = (act) ? quantity : -quantity;
        add_item(depot, quantity, name);
    }
}

/**
 * Check if the hub already contains an item
 * 
 * @param goods - The list of items in the hub
 * @param itemLength - The number of items in the hub
 * @param name - The name of the good to search for
 * @return - The index of the item or the end of the list if no item was found
 */ 
int find_item(Item* goods, int itemLength, char* name) {
    for (int i = 0; i < itemLength; i++) {
        if (!strcmp(goods[i].name, name)) {
            return i;
        }
    }
    // No item was found.
    return itemLength;
}

/**
 * Add an item to the hub
 * 
 * @param depot - Information about the hub's state 
 * @param quant - The number of items to add
 * @param name - The name of the item
 * @return - Whether or not the item could be added
 */ 
bool add_item(Depot* depot, int quant, char* name) {
    if (!check_name(name)) {
        return false;
    } 

    // Check if more memory is needed
    if (depot->itemLength == depot->itemBuffer) {
        depot->itemBuffer *= 2;
        depot->goods = realloc(depot->goods, sizeof(Item) * depot->itemBuffer);
    }

    int index = find_item(depot->goods, depot->itemLength, name);

    // Check if the item has not been added before
    if (index == depot->itemLength) {
        Item temp;
        temp.quantity = 0;
        temp.name = strdup(name);
        depot->goods[depot->itemLength++] = temp;
    } 

    depot->goods[index].quantity += quant;
    return true;
}

/**
 * Compare two Connections
 * 
 * @param obj1 - An object to compare with
 * @param obj2 - An object to compare with
 */ 
int con_order(const void* obj1, const void* obj2) {
    Connection item1 = *(Connection*) obj1;
    Connection item2 = *(Connection*) obj2;

    return strcmp(item1.name, item2.name);
}

/**
 * Compare two Items
 * 
 * @param obj1 - An object to compare with
 * @param obj2 - An object to compare with
 */ 
int item_order(const void* obj1, const void* obj2) {
    Item item1 = *(Item*) obj1;
    Item item2 = *(Item*) obj2;

    return strcmp(item1.name, item2.name);
}

/**
 * Output the depot to the screen
 * 
 * @param depot - Information about the hub's state 
 */ 
void output_depot(Depot* depot) {
    sem_wait(depot->guard);

    // Sort the goods
    qsort(depot->goods, depot->itemLength, sizeof(Item), item_order);
    qsort(depot->con, depot->conCount, sizeof(Connection), con_order);

    printf("Goods:\n");

    // Output all non-zero goods and quantities
    for (int i = 0; i < depot->itemLength; i++) {
        if (depot->goods[i].quantity != 0) {
            printf("%s %d\n", depot->goods[i].name, depot->goods[i].quantity);
        }
    }

    printf("Neighbours:\n");

    for (int i = 0; i < depot->conCount; i++) {
        printf("%s\n", depot->con[i].name);
    }

    fflush(stdout);

    sem_post(depot->guard);
}

/**
 * Check if a port has been connected to before
 * 
 * @param depot - Information about the hub's state 
 * @param portCheck - The port to look at
 * @return - Has the port been accessed before
 */ 
bool check_port(Depot* depot, char* portToCheck) {
    for (int i = 0; i < depot->conCount; i++) {
        if (!strcmp(depot->con[i].port, portToCheck)) {
            return false;
        }
    }
    return true;
}

/**
 * Close the depot print a message to stderr
 * 
 * @param exitCondition - The exit status to exit with
 */ 
void exit_depot(int exitCondition) {
    const char* messages[] = {"",
            "Usage: 2310depot name {goods qty}\n",
            "Invalid name(s)\n",
            "Invalid quantity\n"};   
    fputs(messages[exitCondition], stderr);
    exit(exitCondition);
}
