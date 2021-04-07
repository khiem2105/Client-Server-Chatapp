/**
	Handle multiple socket connections with select and fd_set on Linux
*/
 
#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include <sys/stat.h>
#include <fcntl.h>
 
#define TRUE   1
#define FALSE  0
#define PORT 8888
#define BUFSIZE 1025
#define NAMESIZE 9
#define ALERT "\033[1;31m"

struct client {
    int sockfd;
    char *name;
};

void send_to_unique(struct client client_socket[], int log_fd[], int dst_index, char *message_to_send, char *log) {
    send(client_socket[dst_index].sockfd, message_to_send, strlen(message_to_send), 0);
    write(log_fd[dst_index], log, strlen(log));
    write(log_fd[dst_index], message_to_send, strlen(message_to_send));

}

void send_all(struct client client_socket[], int max_clients, int log_fd[], char *message_to_send, char *log, int src_index) {
    for(int j = 0; j < max_clients; j++) {
        if(j != src_index && client_socket[j].sockfd != 0) {
            send(client_socket[j].sockfd, message_to_send, strlen(message_to_send), 0);
            write(log_fd[j], log, strlen(log));
            write(log_fd[j], message_to_send, strlen(message_to_send));
        }
    }
}

int main(int argc , char *argv[])
{
    int opt = TRUE;
    int master_socket , addrlen , new_socket , max_clients = 30 , activity, i , valread , sd;
	struct client client_socket[30];
    int log_fd[30];
    int max_sd;
    struct sockaddr_in address;
     
    char buffer[BUFSIZE];  //data buffer of 1K
     
    //set of socket descriptors
    fd_set readfds;
     
    //a message
    char *message;
    char *message_welcome = "Welcome ";
    char *message_reject = "Nick name already existed\n";
    char *message_not_exists = "Nick name not exists\n";
    char *message_welcomeback = "Welcome back. Your history:\n";
    char *message_private = "Private message from: ";
    // to store log file name
    char *log_rep = "log/";
    char *log_filename;
    char *log_send = "Send:\n";
    char *log_receive = "Receive:\n";
    // color
    char *red = "\033[1;31m";
 
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++) 
        client_socket[i].sockfd = 0;
     
    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
 
    //set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
 
    //type of socket created
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
     
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
	printf("Listenning on port %d \n", PORT);
	
    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
     
    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");
    
	while(TRUE) 
    {
        //clear the socket set
        FD_ZERO(&readfds);
 
        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;
		
        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++) 
        {
            //socket descriptor
			sd = client_socket[i].sockfd;
            
			//if valid socket descriptor then add to read list
			if(sd > 0)
				FD_SET(sd, &readfds);
            
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
				max_sd = sd;
        }
 
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);
   
        if((activity < 0) && (errno!=EINTR)) {
            printf("select error");
        }
         
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
            
            // read nick name of client
            bzero(buffer, BUFSIZE);
            if(read(new_socket, buffer, BUFSIZE-1) < 0) {
                perror("Error reading nickname");
                exit(EXIT_FAILURE);
            }
            printf("%s\n", buffer);
            int isExisted = 0;
            for(i = 0; i < max_clients; i++) {
                if(client_socket[i].sockfd != 0 && strcmp(client_socket[i].name, buffer) == 0)  {
                    isExisted = 1;
                    break;
                }
            }
            if(!isExisted) {
                // send new connection greeting message
                message = calloc(strlen(message_welcome)+strlen(buffer)+2,sizeof(char));
                bzero(message,strlen(message_welcome)+strlen(buffer)+2);
                strncpy(message,message_welcome,strlen(message_welcome));
                strncat(message,buffer, strlen(buffer));
                strncat(message,"\n", 1);

                if(send(new_socket, message, strlen(message), 0) != strlen(message)) 
                    perror("send");
                
                puts("Welcome message sent successfully");
                // free(message);
        
                //add new socket to array of sockets
                for (i = 0; i < max_clients; i++) 
                {
                    //if position is empty
                    if( client_socket[i].sockfd == 0 )
                    {
                        client_socket[i].sockfd = new_socket;
                        client_socket[i].name = (char*)calloc(strlen(buffer)+1, sizeof(char));
                        bzero(client_socket[i].name, strlen(buffer) + 1);
                        strncpy(client_socket[i].name, buffer, strlen(buffer));
                        struct stat statut;
                        log_filename = calloc(strlen(log_rep) + strlen(client_socket[i].name) + 1, sizeof(char));
                        strncpy(log_filename, log_rep, strlen(log_rep));
                        strncat(log_filename, client_socket[i].name, strlen(client_socket[i].name));

                        if(lstat(log_filename, &statut) == -1) {
                            log_fd[i] = open(log_filename, O_RDWR | O_CREAT | O_APPEND, 0655);
                        }
                        else {
                            log_fd[i] = open(log_filename, O_RDWR | O_APPEND, statut.st_mode);
                            puts("Sending history of chat...");
                            send(client_socket[i].sockfd, message_welcomeback, strlen(message_welcomeback), 0);
                            while((valread = read(log_fd[i], buffer, BUFSIZE-1))) {
                                buffer[valread] = '\0';
                                send(client_socket[i].sockfd, buffer, strlen(buffer), 0);
                            }
                            puts("Finish.");
                        }
                        // printf("%s\n", client_socket[i].name);
                        printf("Adding to list of sockets as %d\n" , i);
                        break;
                    }
                }
            }
            else {
                message = calloc(strlen(message_reject)+1, sizeof(char));
                strcpy(message, message_reject);
                send(new_socket, message, strlen(message), 0);
                puts("Nick name existed. Reject connect");
            }
        }
         
        //else its some IO operation on some other socket :)
        for (i = 0; i < max_clients; i++) 
        {
            sd = client_socket[i].sockfd;
             
            if (FD_ISSET(sd , &readfds)) 
            {
                bzero(buffer, BUFSIZE);
                //Check if it was for closing , and also read the incoming message
                while((valread = read(sd, buffer, BUFSIZE-1)) >= 0) {
                    if(valread == 0)
                    {
                        //Somebody disconnected , get his details and print
                        getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                        printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                        
                        //Close the socket and mark as 0 in list for reuse
                        close(sd);
                        client_socket[i].sockfd = 0;
                        free(client_socket[i].name);
                        close(log_fd[i]);
                    }
                    
                    //Echo back the message that came in
                    else
                    {
                        //set the string terminating NULL byte on the end of the data read
                        buffer[valread] = '\0';
                        write(log_fd[i], log_send, strlen(log_send));
                        write(log_fd[i], buffer, strlen(buffer));

                        if(strstr(buffer, "/nick") != NULL) {
                            buffer[valread-1] = '\0';
                            char *new_nickname = strstr(buffer, " ") + 1;
                            // printf("%s", new_nickname);
                            // check if new nickname exists
                            int existed = 0;
                            for(int j = 0; j < max_clients; j++) {
                                if(client_socket[j].sockfd != 0 && strcmp(client_socket[j].name, new_nickname) == 0) {
                                    existed = 1;
                                    break;
                                }
                            }
                            if(!existed) {
                                free(client_socket[i].name);
                                client_socket[i].name = calloc(strlen(new_nickname)+1, sizeof(char));
                                strncpy(client_socket[i].name, new_nickname, strlen(new_nickname));
                            }
                            else
                                send(client_socket[i].sockfd, message_reject, strlen(message_reject), 0);

                        }
                        else if(strstr(buffer, "/msg") != NULL) {
                            char *dst_nickname = strstr(buffer, " ") + 1;
                            char *msg = strstr(dst_nickname, " ") + 1;
                            // printf("%s\n", msg);
                            int nickname_len = strlen(dst_nickname) -strlen(msg) - 1;
                            // printf("%d\n", nickname_len);
                            dst_nickname[nickname_len] = '\0';
                            // printf("%s\n", dst_nickname);
                            // check if destination client exists
                            int existed = 0, dst_index;
                            for(int j = 0; j < max_clients; j++) {
                                if(j != i && client_socket[j].sockfd != 0 && strcmp(client_socket[j].name, dst_nickname) == 0) {
                                    existed = 1;
                                    dst_index = j;
                                    break;
                                }
                            }
                            if(existed) {
                                char *msg_to_send = calloc(strlen(message_private) + strlen(client_socket[i].name) + strlen(msg) + 2, sizeof(char));
                                strncpy(msg_to_send, message_private, strlen(message_private));
                                strncat(msg_to_send, client_socket[i].name, strlen(message_private) + strlen(client_socket[i].name) + strlen(msg) + 2);
                                strncat(msg_to_send, "\n", strlen(message_private) + strlen(client_socket[i].name) + strlen(msg) + 2);
                                strncat(msg_to_send, msg, strlen(message_private) + strlen(client_socket[i].name) + strlen(msg) + 2);
                                // send(client_socket[dst_index].sockfd, msg_to_send, strlen(msg_to_send), 0);
                                // write(log_fd[dst_index], log_receive, strlen(log_receive));
                                // write(log_fd[dst_index], msg_to_send, strlen(msg_to_send));
                                send_to_unique(client_socket, log_fd, dst_index, msg_to_send, log_receive);
                                // free(msg_to_send);
                            }
                            else
                                send(client_socket[i].sockfd, message_not_exists, strlen(message_not_exists), 0);
                            // free(msg);
                        }
                        else if(strstr(buffer, "/alert") != NULL) {
                            
                            char *msg = strstr(buffer, " ") + 1;
                            char *temp = strstr(msg, " ");
                            if(temp != NULL) {
                                char *dst_nickname = msg;
                                dst_nickname[strlen(msg) - strlen(temp)] = '\0';
                                int existed = 0, dst_index;
                                for(int j = 0; j < max_clients; j++) {
                                    if(j != i && client_socket[j].sockfd != 0 && strcmp(client_socket[j].name, dst_nickname) == 0) {
                                        existed = 1;
                                        dst_index = j;
                                        break;
                                    }
                                }
                                if(existed) {
                                    temp = temp + 1;
                                    char *msg_to_send = calloc(strlen(client_socket[i].name) + strlen(temp) + strlen(red) + 3, sizeof(char));
                                    strncpy(msg_to_send, red, strlen(red));
                                    strncat(msg_to_send, client_socket[i].name, strlen(client_socket[i].name) + strlen(temp) + strlen(red) + 3);
                                    strncat(msg_to_send, ": ", strlen(client_socket[i].name) + strlen(temp) + strlen(red) + 3);
                                    strncat(msg_to_send, temp, strlen(client_socket[i].name) + strlen(temp) + strlen(red) + 3);
                                    // send(client_socket[dst_index].sockfd, msg_to_send, strlen(msg_to_send), 0);
                                    // write(log_fd[dst_index], log_receive, strlen(log_receive));
                                    // write(log_fd[dst_index], msg_to_send, strlen(msg_to_send));
                                    send_to_unique(client_socket, log_fd, dst_index, msg_to_send, log_receive);
                                    // free(msg_to_send);
                                }
                                else
                                    send(client_socket[i].sockfd, message_not_exists, strlen(message_not_exists), 0);

                            }
                            else {
                                char *msg_to_send = calloc(strlen(red) + strlen(client_socket[i].name) + strlen(msg) + 3, sizeof(char));
                                strncpy(msg_to_send, red, strlen(red));
                                strncat(msg_to_send, client_socket[i].name, strlen(red) + strlen(client_socket[i].name) + strlen(msg) + 3);
                                strncat(msg_to_send, ": ", strlen(red) + strlen(client_socket[i].name) + strlen(msg) + 3);
                                strncat(msg_to_send, msg, strlen(red) + strlen(client_socket[i].name) + strlen(msg) + 3);
                                // for(int j = 0; j < max_clients; j++) {
                                //     if(j != i && client_socket[j].sockfd != 0) {
                                //         send(client_socket[j].sockfd, msg_to_send, strlen(msg_to_send), 0);
                                //         write(log_fd[j], log_receive, strlen(log_receive));
                                //         write(log_fd[j], msg_to_send, strlen(msg_to_send));                                    }
                                // }
                                send_all(client_socket, max_clients, log_fd, msg_to_send, log_receive, i);
                            }
                        }
                        else {
                            // send(sd , buffer , strlen(buffer) , 0 );
                            char *msg_to_send = calloc(strlen(client_socket[i].name) + strlen(buffer) + 3, sizeof(char));
                            strncpy(msg_to_send, client_socket[i].name, strlen(client_socket[i].name));
                            strncat(msg_to_send, ": ", 2);
                            strncat(msg_to_send, buffer, strlen(buffer));
                            // for(int j = 0; j < max_clients; j++) {
                            //     if(j != i && client_socket[j].sockfd != 0) {
                            //         send(client_socket[j].sockfd, msg_to_send, strlen(msg_to_send), 0);
                            //         write(log_fd[j], log_receive, strlen(log_receive));
                            //         write(log_fd[j], msg_to_send, strlen(msg_to_send));
                            //         // free(msg_to_send);
                            //     }
                            // }
                            send_all(client_socket, max_clients, log_fd, msg_to_send, log_receive, i);
                        }
                    }
                }
            }
        }
    }
    
    return 0;
}