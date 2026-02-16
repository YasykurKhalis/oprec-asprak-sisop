#include "common.h"
#include <signal.h>

client_t *clients[MAX_CLIENTS];
int cli_count = 0;
int uid = 10;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// fungsi helper
void str_trim_lf (char* arr, int length) {
    int i;
    for (i = 0; i < length; i++) { 
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void queue_add(client_t *cl){
    pthread_mutex_lock(&clients_mutex);
    for(int i=0; i < MAX_CLIENTS; ++i){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid){
    pthread_mutex_lock(&clients_mutex);
    for(int i=0; i < MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// fungsi untuk kirim pesan client lain
void send_message_to_others_in_room(char *s, int uid, char *room){
    pthread_mutex_lock(&clients_mutex);
    for(int i=0; i<MAX_CLIENTS; ++i){
        if(clients[i]){
            // kirim ke room yang sama tapi bukan pengirim
            if(clients[i]->uid != uid && strlen(clients[i]->room) > 0 && strcmp(clients[i]->room, room) == 0){
                if(write(clients[i]->sockfd, s, strlen(s)) < 0){
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// fungsi kirim pesan ke aja sendiri
void send_message_to_self(const char *s, int sockfd){
    if(write(sockfd, s, strlen(s)) < 0){
        perror("ERROR: write to self failed");
    }
}

// thread utama client utk handle user input
void *handle_client(void *arg){
    char buff_out[BUFFER_SZ];
    char name[NAME_LEN];
    char room[NAME_LEN];
    int leave_flag = 0;

    cli_count++;
    client_t *cli = (client_t *)arg;

    // input nama
    while(1) {
        char *msg = "[+] Insert your name:"; 
        write(cli->sockfd, msg, strlen(msg));
        
        memset(name, 0, NAME_LEN); 
        int receive = recv(cli->sockfd, name, NAME_LEN, 0);
        if (receive <= 0) {
            leave_flag = 1;
            break;
        }
        
        str_trim_lf(name, receive);

        if (strlen(name) < 2 || strlen(name) >= 30) {
            char *err = "Name must be 2-30 characters long\n";
            write(cli->sockfd, err, strlen(err));
            continue;
        }

        int name_exists = 0;
        pthread_mutex_lock(&clients_mutex);
        for(int i=0; i<MAX_CLIENTS; ++i){
            if(clients[i] && clients[i]->uid != cli->uid){
                if(strcmp(clients[i]->name, name) == 0){
                    name_exists = 1;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (name_exists) {
            char *err = "That name already exists! Try another.\n";
            write(cli->sockfd, err, strlen(err));
        } else {
            strcpy(cli->name, name);
            break;
        }
    }

    // input room
    if (!leave_flag) {
        char *msg = "[+] Insert Room Name:";
        write(cli->sockfd, msg, strlen(msg));

        memset(room, 0, NAME_LEN);
        int receive = recv(cli->sockfd, room, NAME_LEN, 0);
        if (receive > 0) {
            str_trim_lf(room, receive);
            strcpy(cli->room, room);
            
            // notif ke user lain
            sprintf(buff_out, ">>> %s has joined %s <<<", cli->name, cli->room);
            send_message_to_others_in_room(buff_out, cli->uid, cli->room);

            // pesan welcome ke aja sendiri
            char welcome_msg[BUFFER_SZ];
            sprintf(welcome_msg, ">>> Welcome to %s, %s! <<<\n>>> Type '!help' to see available commands. <<<", cli->room, cli->name);
            send_message_to_self(welcome_msg, cli->sockfd);

            printf("%s\n", buff_out); // server log
        } else {
            leave_flag = 1;
        }
    }

    memset(buff_out, 0, BUFFER_SZ);

    // loop chat dan command
    while(1){
        if (leave_flag) {
            break;
        }

        memset(buff_out, 0, BUFFER_SZ);
        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);

        if (receive > 0){
            if(strlen(buff_out) > 0){
                str_trim_lf(buff_out, strlen(buff_out));

                // command
                if(strcmp(buff_out, "!help") == 0) {
                    char *help_msg = 
                        "=== AVAILABLE COMMANDS ===\n"
                        " !help   : Display this menu\n"
                        " !online : View online users in this room\n"
                        " !exit   : Exit the program\n"
                        "==========================";
                    send_message_to_self(help_msg, cli->sockfd);
                }
                else if(strcmp(buff_out, "!online") == 0) {
                    char list_msg[BUFFER_SZ];
                    memset(list_msg, 0, BUFFER_SZ); 
                    
                    sprintf(list_msg, "Users online in %s:", cli->room);
                    
                    pthread_mutex_lock(&clients_mutex);
                    for(int i=0; i < MAX_CLIENTS; ++i){
                        if(clients[i]){
                            if(strcmp(clients[i]->room, cli->room) == 0){
                                strcat(list_msg, "\n- ");
                                strcat(list_msg, clients[i]->name);
                                if(clients[i]->uid == cli->uid) {
                                    strcat(list_msg, " (You)");
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    send_message_to_self(list_msg, cli->sockfd);
                }
                else if(strcmp(buff_out, "!exit") == 0) {
                    sprintf(buff_out, "<<< %s has left %s >>>", cli->name, cli->room);
                    printf("%s\n", buff_out);
                    send_message_to_others_in_room(buff_out, cli->uid, cli->room);
                    leave_flag = 1;
                }
                // chat biasa
                else {
                    char msg_formatted[BUFFER_SZ + NAME_LEN + NAME_LEN + 10];
                    sprintf(msg_formatted, "%s @ %s: %s", cli->name, cli->room, buff_out);
                    
                    send_message_to_others_in_room(msg_formatted, cli->uid, cli->room);
                    printf("%s\n", msg_formatted);
                }
            }
        } else if (receive == 0) {
            sprintf(buff_out, "<<< %s has left %s >>>", cli->name, cli->room);
            printf("%s\n", buff_out);
            send_message_to_others_in_room(buff_out, cli->uid, cli->room);
            leave_flag = 1;
        } else {
            leave_flag = 1;
        }
    }

    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

// main function daemon dan socket
void server_start() {
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // cegah error address already in use saat restart server
    int option = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

    signal(SIGPIPE, SIG_IGN); // cegah crash broken pipe saat client keluar paksa

    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("ERROR: Socket binding failed");
        return;
    }

    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return;
    }

    printf("=== SERVER RUNNING ON PORT %d ===\n", PORT);

    while(1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        if((cli_count + 1) == MAX_CLIENTS){
            printf("Max clients reached. Rejected.\n");
            close(connfd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));

        memset(cli, 0, sizeof(client_t));

        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-stop") == 0) {
        int res = system("fuser -k -n tcp 8888 > /dev/null 2>&1");
        return EXIT_SUCCESS;
    }
    
    if (argc == 2 && strcmp(argv[1], "-start") == 0) {
        // Cek apakah port 8888 listening
        if (system("fuser -n tcp 8888 > /dev/null 2>&1") == 0) {
            printf("Server is already running (Found PID file)!\n");
            return EXIT_FAILURE;
        }

        printf("Running server in background...\n");

        // daemonize
        pid_t pid = fork();
        
        if (pid < 0) exit(EXIT_FAILURE); // error Fork
        if (pid > 0) exit(EXIT_SUCCESS); // matikan Parent (terminal kembali aktif)

        // child proses (daemon) lanjut di sini
        if (setsid() < 0) exit(EXIT_FAILURE); // buat sesi baru
        
        signal(SIGPIPE, SIG_IGN); // abaikan sinyal broken pipe

        server_start(); // logika server
        
    } else {
        printf("Usage: %s -start | -stop\n", argv[0]);
    }
    return EXIT_SUCCESS;
}
