#include "common.h"
#include <termios.h> // library terminal asynchronous

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char current_input[BUFFER_SZ] = {0}; // buffer untuk menyimpan ketikan yang belum di-enter
int input_pos = 0; // posisi kursor ketikan saat ini

// fungsi terminal asynchronous
void set_conio_mode() {
    struct termios new_termios;
    tcgetattr(0, &new_termios);
    new_termios.c_lflag &= ~ICANON; // matikan canonical mode (tunggu enter)
    new_termios.c_lflag &= ~ECHO;   // matikan echo (print manual)
    new_termios.c_lflag &= ~ISIG; // matikan signal
    tcsetattr(0, TCSANOW, &new_termios);
}

// reset terminal saat keluar
void reset_conio_mode() {
    struct termios new_termios;
    tcgetattr(0, &new_termios);
    new_termios.c_lflag |= ICANON;
    new_termios.c_lflag |= ECHO;
    tcsetattr(0, TCSANOW, &new_termios);
}

// fungsi helper untuk hapus baris saat ini
void clear_current_line() {
    printf("\33[2K\r"); // clear line + carriage return
}

// fungsi helper print prompt + ketikan saat ini
void print_prompt_and_input() {
    printf("> %s", current_input);
    fflush(stdout);
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

// fungsi terima pesan
void *recv_msg_handler(void *arg) {
    char message[BUFFER_SZ] = {};
    while (1) {
        int receive = recv(sockfd, message, BUFFER_SZ, 0);
        
        if (receive > 0) {
            clear_current_line();
            printf("%s\n", message);
            print_prompt_and_input();
        } else if (receive == 0) {
            flag = 1;
            break;
        } else {
            flag = 1;
            break;
        }
        memset(message, 0, sizeof(message));
    }
    return NULL;
}

// fungsi kirim pesan
void *send_msg_handler(void *arg) {
    char buffer[BUFFER_SZ] = {};
    char c;

    set_conio_mode(); // set terminal ke mode raw

    while(1) {
        c = getchar(); // baca character

        // break untuk ctrl c
        if (c == 3) { 
            flag = 1;
            break;
        }

        // user tekan enter
        if (c == '\n') {
            printf("\n"); // pindah baris visual
            
            if (input_pos > 0) {
                // copy current_input ke buffer untuk dikirim
                strcpy(buffer, current_input);

                if (strcmp(buffer, "!exit") == 0) {
                    send(sockfd, buffer, strlen(buffer), 0);
                    break;
                } else {
                    send(sockfd, buffer, strlen(buffer), 0);
                }
            }
            
            // reset buffer input
            memset(buffer, 0, BUFFER_SZ);
            memset(current_input, 0, BUFFER_SZ);
            input_pos = 0;
            
            print_prompt_and_input(); // prompt baru setelah enter
            
        } 
        // user tekan backspace
        else if (c == 127 || c == 8) {
            if (input_pos > 0) {
                input_pos--;
                current_input[input_pos] = '\0';
                
                // hapus baris dan print ulang
                clear_current_line();
                print_prompt_and_input();
            }
        }
        // user input karakter biasa
        else {
            if (input_pos < BUFFER_SZ - 1) {
                current_input[input_pos] = c;
                input_pos++;
                current_input[input_pos] = '\0';
                
                // echo manual karena echo dimatikan
                printf("%c", c);
                fflush(stdout);
            }
        }
    }
    
    // kembalikan mode terminal sebelum keluar
    reset_conio_mode();
    catch_ctrl_c_and_exit(2);
    return NULL;
}

int main(){
    int port = PORT; 
    signal(SIGINT, catch_ctrl_c_and_exit);
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(port);

    int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf("ERROR: connect failed\n");
        return EXIT_FAILURE;
    }

    // buat thread
    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
        printf("ERROR: pthread\n");
        return EXIT_FAILURE;
    }

    while (1){
        if(flag){
            printf("\n");
            break;
        }
    }
    
    // reset terminal ketika keluar
    reset_conio_mode();
    close(sockfd);
    return EXIT_SUCCESS;
}