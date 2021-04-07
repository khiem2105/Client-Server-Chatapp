#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>

#define BUFFER_SIZE 512

#define NTP_TIMESTAMP_DELTA 2208988800ull

#define LI(packet) (unit8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li & 11 000 000) >> 6
#define VN(packet) (unit8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vm & 00 111 000) >> 3
#define MODE(packet) (unit8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

#define SET_LI(packet, li) (packet.li_vn_mode |= (li << 6))
#define SET_VN(packet, vn) (packet.li_vn_mode |= (vn << 3))
#define SET_MODE(packet, mode) (packet.li_vn_mode |= (mode << 0))

typedef struct {
    uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
    // li.   Two bits.   Leap indicator.
    // vn.   Three bits. Version number of the protocol.
    // mode. Three bits. Client will pick mode 3 for client.
    uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    uint8_t precision;       // Eight bits. Precision of the local clock.

    uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    uint32_t refId;          // 32 bits. Reference clock identifier.

    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} ntp_packet;

void red() {
  printf("\033[1;31m");
}

void yellow() {
  printf("\033[1;33m");
}

void blue() {
	printf("\033[0;34m");
}

void green() {
	printf("\033[0;32m");
}

void cyan() {
	printf("\033[0;36m");
}

void white() {
	printf("\033[0;37m");
}

void reset() {
  printf("\033[0m");
}

int main(int argc , char *argv[]) {
	// create and zero out the packet. All 48 bytes worth
	ntp_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	// set the first byte's bits to 00, 011, 011 for li=0, vn=3 and mode=3
	SET_LI(packet, 0);
	SET_VN(packet, 3);
	SET_MODE(packet, 3);

	// UDP socket 
	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(udp_sockfd < 0) {
		perror("Error creating udp socket");
		exit(EXIT_FAILURE);
	}

	// convert an URL to a hostent structure to easy get the IP
	// NTP server host name
	char *hostname = "fr.pool.ntp.org";
	struct hostent *ntp_server = gethostbyname(hostname);
	if(ntp_server == NULL) {
		perror("No such host");
		exit(EXIT_FAILURE);
	}

	// create a ntp server address structure
	struct sockaddr_in ntp_serv_addr;
	bzero(&ntp_serv_addr, sizeof(&ntp_serv_addr));

	//server addr
	ntp_serv_addr.sin_family = AF_INET;
	bcopy(ntp_server->h_addr_list[0], &ntp_serv_addr.sin_addr.s_addr, ntp_server->h_length);

	//ntp port
	int ntp_port = 123;
	ntp_serv_addr.sin_port = htons(ntp_port);

	// Connect to ntp server
	if(connect(udp_sockfd, (struct sockaddr *)&ntp_serv_addr, sizeof(ntp_serv_addr)) < 0) {
		perror("Error connecting to NTP server");
		exit(EXIT_FAILURE);
	}


	int socket_desc;
	struct sockaddr_in server;
	char buf[BUFFER_SIZE];
	int port, valread;

	// message
	char *message_reject = "Nick name already existed\n";
	
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_family = AF_INET;
	if(sscanf(argv[2], "%d", &port) != 1) {
		puts("Erreur: Le paramètre NOMBRE doit être un nombre entier !");
		return EXIT_FAILURE;
	}
	server.sin_port = htons( port );

	//Connect to remote server
	if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0) {
		puts("connect error");
		return 1;
	}

	puts("Connected");
		
	puts("Enter nickname:");
	bzero(buf,BUFFER_SIZE);
	scanf("%s",buf);
	if(send(socket_desc, buf, strlen(buf), 0) < 0)
		puts("recv failed");

	bzero(buf, BUFFER_SIZE);
	recv(socket_desc, buf, BUFFER_SIZE-1, 0);
	if(strcmp(buf, message_reject) == 0) {
		puts("Nick name already existed");
		return 1;
	}
	else
		printf("%s", buf);
	
	// SOCKET AND STDIN non-blocking mode
	fcntl(socket_desc, F_SETFL, fcntl(socket_desc, F_GETFL) | O_NONBLOCK);
	fcntl(STDIN_FILENO, F_SETFL, fcntl(socket_desc, F_GETFL) | O_NONBLOCK);
	// fcntl(udp_sockfd, F_SETFL, fcntl(udp_sockfd, F_GETFL) | O_NONBLOCK);
	
	while(1) {
		// STDIN read	
		bzero(buf, BUFFER_SIZE);
		if(read(STDIN_FILENO, buf, BUFFER_SIZE -1) < 0) {	
			if (errno != EAGAIN)
				puts("recv failed");
		}   
        else {
			if(strstr(buf, "/file") != NULL) {
				send(socket_desc, buf, strlen(buf), 0);
				char *filename = strstr(buf, " ") + 1;
				filename[strlen(filename)-1] = '\0';
				printf("%s\n", filename);
				int fd = open(filename, O_RDONLY);
				int nbbytes;
				while((nbbytes = read(fd, buf, BUFFER_SIZE-1)) != 0) {
					buf[nbbytes] = '\0';
					send(socket_desc, buf, strlen(buf), 0);
				}
			}
			else
				send(socket_desc, buf, strlen(buf), 0);
		}

		//Receive a reply from the server
		while((valread = recv(socket_desc, buf, BUFFER_SIZE -1, 0)) > 0) {	
			// char *msg = strstr(buf, " ") + 1;
			// char *src_nickname = buf;
			// src_nickname[strlen(src_nickname) - strlen(msg) - 2] = '\0';
			// blue();
			// printf("%s: ", src_nickname);
			// cyan();
			// printf("%s", msg);
			// white();
			buf[valread] = '\0';
			if(strstr(buf, "\033[1;31m") != NULL)
				red();
			printf("%s", buf);
			reset();
			if(write(udp_sockfd, &packet, sizeof(ntp_packet)) < 0)
				perror("Error sending ntp packet");
			if(read(udp_sockfd, &packet, sizeof(ntp_packet)) < 0)
				perror("Error reading ntp packet");
			
			time_t txTm = (time_t) (ntohl(packet.txTm_s) - NTP_TIMESTAMP_DELTA);
			printf("Time: %s", ctime((const time_t*)&txTm));

			bzero(&packet, sizeof(ntp_packet));

			// set the first byte's bits to 00, 011, 011 for li=0, vn=3 and mode=3
			SET_LI(packet, 0);
			SET_VN(packet, 3);
			SET_MODE(packet, 3);

			usleep(100000);
		}
	}

	return 0;
}