// INCLUDES 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 

// DEFINES 
#define MAX_CONNECTIONS 100
#define PATH_MAX 4096 

// GLOBAL SEC. 
char ROOT[PATH_MAX];
int BUFFER_SIZE = 4096; // 4Kb = default page table size
int PORT = 31337;

char *responses[] = 
{
	"HTTP/1.1 200 OK\n",
	"HTTP/1.0 400 Bad Request\n",
	"HTTP/1.0 403 Forbidden\n",
	"HTTP/1.0 404 Not Found\n",
};	

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
///////////////////////////////////////////////////////////////////////////////////////////////


void getClientAddr(int sock_fd)
{
	char clientip[16];
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	

	int res = getpeername(sock_fd,(struct sockaddr *)&addr, &addr_size);
	strcpy(clientip, inet_ntoa(addr.sin_addr));

	printf("Client connected. [IP addr: %s]\n",clientip);
}
///////////////////////////////////////////////////////////////////////////////////////////////


void read_whitelist(char *filename)
{
	FILE *fp  = NULL;
	int **arrayIPs, arrayIPs2[10][6], *arrayIPs3[10];
	char c;
	int i,j;
	
	// Initially presumes total of 100's IP addresses, with 16 chars (ipv4 )
	arrayIPs = (int **)malloc(10 * sizeof(int *));
	for(i = 0; i < 10; i++)
	{

		arrayIPs[i] = malloc(16 * sizeof(int));
		arrayIPs[i][0] = '7';
	}

	if((fp = fopen("whitelist.txt","r")) != NULL)
	{
		
		i = 0; 
		j = 0;
		c = fgetc(fp);
		while(c != EOF)
		{
			
			if((c == '\n'))
			{
				
				printf("%c",c);
				c = fgetc(fp);
				arrayIPs[i][j] = '\0';
				i++;
				j++;
				continue;
			}
			
			else
			{
				printf("%c",c);
				arrayIPs[i][j] = c;
				c = fgetc(fp);
				j++;
			}
		}
		
		fclose(fp);
	}

	exit(0);
}
///////////////////////////////////////////////////////////////////////////////////////////////


void handle_request(char *client_request, int sock_fd)
{
	char *reqline[3], path[PATH_MAX], data_to_send[BUFFER_SIZE*10];
	int bytes_read = 0, bytes_write = 0, temp, fd, c;
	FILE *fp  = NULL;

	reqline[0] = malloc(sizeof(strlen(client_request)));
	reqline[1] = malloc(BUFFER_SIZE);
	reqline[2] = malloc(BUFFER_SIZE);
	

	// parser the request received from client
	printf("\033[1;92mPARSING REQUEST...\033[0m\n");
	// printf("%s",client_request);

	// reqline[0] = http method
	// reqline[1] = file required
	// reqline[2] = protocol version
	reqline[0]	= strtok(client_request," ");
	reqline[1] 	= strtok(NULL," ");
	reqline[2] 	= strtok(NULL,"\r\n");
	

	// ONLY accepts HTTP/1.0 and HTTP/1.1 request headers from client
	// Persistent connection is an opt-in feature, anyway 
	if (strncmp(reqline[2], "HTTP/1.0", 8)!=0 && strncmp(reqline[2], "HTTP/1.1", 8)!=0 )
	{

		send(sock_fd, responses[2], strlen(responses[2]), 0);
		return;
	}
	printf("reqline[0]	= %s \n",reqline[0]);
	printf("reqline[1]	= %s \n",reqline[1]);
	printf("reqline[2]	= %s \n",reqline[2]);
	


	/*	****************************** HANDLE HTTP METHODS ****************************** */
	//	GET METHOD
	if(strcmp(reqline[0],"GET") == 0)
	{		
		if ( strncmp(reqline[1], "/\0", 2)==0 )
			// If no file is specified, index.html will be opened by default   
			// strcpy(reqline[1],"/cat.jpg");
			strcpy(reqline[1],"/index.html");

	
		// printf("ROOT = %s \n",ROOT);	
		strcpy(path, ROOT);
		strcpy(&path[strlen(ROOT)], reqline[1]);
		printf("File Required = %s\n", path);
	
		
		memset((void*)data_to_send, (int)'\0', strlen(data_to_send));
		bytes_read = 0;
		bytes_write = 0;
		temp = 0;
		fp = NULL;
		if ((fp = fopen(path, "r" )) != NULL)	//FILE FOUND
		{
			fd = fileno(fp);

			// bytes_read = 0;
			// bytes_write = 0;
			c = fgetc(fp);
			while(c != EOF)
			{
				data_to_send[bytes_read] = c;
				c = fgetc(fp);
				bytes_read += 1;
			}
			rewind(fp);
			fclose(fp);	


			char str[4];
			sprintf(str, "%d", bytes_read);
			send(sock_fd, responses[0], strlen(responses[0]), 0);
			send(sock_fd, "Connection: keep-alive\n", strlen("Connection: keep-alive\n"), 0);
			send(sock_fd, "Content-Length: ", strlen("Content-Length: "), 0);
			send(sock_fd, str, strlen(str), 0);
			send(sock_fd, "\n", 1, 0);
			// send(sock_fd, "Keep-Alive: max=3\n", strlen("Keep-Alive: max=3\n"), 0);
			send(sock_fd, "Keep-Alive: timeout=3\n\n", strlen("Keep-Alive: timeout=3\n\n"), 0);
			

			// bytes_read = 0;
			// while ( (temp = read(fd, data_to_send, BUFFER_SIZE)) > 0 )
			// {
				
			// 	bytes_read += temp;
			// 	bytes_write += write(sock_fd, data_to_send, temp);
			// }
			bytes_write = write(sock_fd, data_to_send, bytes_read);
			
					
			printf("bytes_read = %d\n", bytes_read);
			printf("bytes_write = %d\n\n", bytes_write);
		}
		else
		{
			write(sock_fd, responses[1], strlen(responses[1])); //FILE NOT FOUND
			printf("File not found.\n");
			return;
		}
	}
	

	return;
}
///////////////////////////////////////////////////////////////////////////////////////////////


void * respond(void *arg)
{
	int i, j, bytes_read, crlf_count, singleByte, sock_fd, total_requests;
	char clientRequest[BUFFER_SIZE], singleChar[1];
	clock_t t;
	
	// get socket FD 
	sock_fd = *((int *)arg);
	printf("#[sock_fd]: %d\n", sock_fd);
	
	// initialize consumer variables
	memset((void*)clientRequest, (int)'\0', BUFFER_SIZE);
	singleChar[0] = '\0';
	bytes_read = 0;
	crlf_count = 0;
	total_requests = 0;
	i = 0, j = 0;

	// read data from client socket until find 2x CRLF's, or the timeout param value has achieved their limit
	// default timeout set to 3 sec's
	while (1)
	{	
		
		if(j == 0)
			printf("\n\033[1;35mRECEIVING HEADERS...\033[0m\n");
		
		// start timeout mark
		// t = clock();
	
		singleByte = recv(sock_fd, singleChar, 1, 0);
		if (singleByte < 0) // receive error
		{
			fprintf(stderr,"Error receiving data from client.\n");
			break;
		}
		else if (singleByte == 0) // socket closes
		{
			fprintf(stderr,"Client disconnected unexpectedly.\n");
			break;
		}


		// verifys '\r\n\r\n' sequence
		if(singleChar[0] == '\n' && clientRequest[i - 2] == '\n')
		{						
			// clients header already received here
		
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			// printf("#[bytes_read] CLIENT REQUEST HEADER = %d\n", bytes_read);


			// handle request type properly
			handle_request(clientRequest, sock_fd);
			
			// prepare to a new request header
			memset((void*)clientRequest, (int)'\0', BUFFER_SIZE);
			crlf_count = 0;
			i = 0;
			j = 0;
			continue;		
		}

		else if(singleChar[0] == '\n' && crlf_count == 0)
		{
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			crlf_count = 1;
			j = 1;
			i++;
			continue;	
		}

		else
		{
			clientRequest[i] = singleChar[0];
			printf("%c",clientRequest[i]);
			j = 1;
			i++;
			continue;	
		}
	}


	shutdown(sock_fd, SHUT_RDWR);
	close(sock_fd);
	pthread_exit(&sock_fd);
	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    pthread_t threads[MAX_CONNECTIONS];
    struct sockaddr_in clientAddr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int server_fd, sock_fd;
    int i = 0;

	
	// If port number is not specified, set default http port
	PORT = ((argv[1] != (char *)' ') && (argc >= 2)) ? atoi(argv[1]) : PORT;

	// Read whitelist, if provided
	// if((argv[2] != (char *)' ') && (argc >= 3))
		// read_whitelist(argv[2]);
		// read_whitelist(NULL);


	// set root to serve files
	strcpy(ROOT,getenv("PWD"));

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(1);
    }
	
	clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = htons(PORT);

	// Attach socket to the clientAddr:port specified 
    if (bind(server_fd, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
    {
        perror("bind failed");
        exit(1);
    }


	// run the server
    while (1)
    {
		if (listen(server_fd, MAX_CONNECTIONS) < 0)
        {
            perror("listen failed");
            exit(1);
        }
		
        printf("\033[1;37mLISTENING CONNECTION... # %d \033[0m\n",i);
        if ((sock_fd = accept(server_fd, (struct sockaddr *)&clientAddr, (socklen_t *)&addrlen)) < 0)
        {
            
			perror("Cannot accept connection");
			exit(1);
        }

	
		// Get and prints client address 
        getClientAddr(sock_fd);

		// SEQUENCIAL MODE
		// respond(&sock_fd);

		// MULTITHREAD MODE 
		// Create thread and handle request
        if (pthread_create(&threads[i % MAX_CONNECTIONS], NULL, &respond, &sock_fd) != 0)
		{
			fprintf(stderr, "error: Cannot create thread # %d\n", i);
			break;	
		}
		else
		{
			printf("\033[1;37mTHREAD [# %d] HAS SUCCESSFULLY FINISHED THEIR JOB. \033[0m \n\n",i);
		}
		

		//	BEGIN OF CRITICAL SECTION
		// pthread_mutex_lock(&lock); 
			
		// pthread_mutex_unlock(&lock);
		//	END OF CRITICAL SECTION
	
		// increments # total conns
		i++;
    }



    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////
