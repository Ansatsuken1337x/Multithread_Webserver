// includes 
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>

// defines 
#define MAX_CONNECTIONS 100
#define EOL "\r\n"
#define EOL_SIZE 2
#define PATH_MAX 4096 

// global 
char *ROOT;
int BUFFER_SIZE = 4096; // 4Kb = default page table size
char data_to_send[4096];

char * responses[] = 
{
	"HTTP/1.0 200 OK\n",
	"HTTP/1.0 404 Not Found\n",
};	

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// typedef struct {
 // char *ext;
 // char *mediatype;
// } extn;

//Possible media types
// extn extensions[] ={
 // {"gif", "image/gif" },
 // {"txt", "text/plain" },
 // {"jpg", "image/jpg" },
 // {"jpeg","image/jpeg"},
 // {"png", "image/png" },
 // {"ico", "image/ico" },
 // {"zip", "image/zip" },
 // {"gz",  "image/gz"  },
 // {"tar", "image/tar" },
 // {"htm", "text/html" },
 // {"html","text/html" },
 // {"php", "text/html" },
 // {"pdf","application/pdf"},
 // {"zip","application/octet-stream"},
 // {"rar","application/octet-stream"},
 // {0,0} };

///////////////////////////////////////////////////////////////////////////////////////////////

int handle_request(char *mesg, int socked_fd)
{
	char *reqline[3], path[PATH_MAX];
	int bytes_read = 0, temp = 0;
	int fd;
	
	reqline[0] = malloc(sizeof(strlen(mesg)));
	
	// parser the mesg received
	printf("%s \n",mesg);
	// reqline[0] = http method
	// reqline[1] = protocol version
	// reqline[2] = file required
	reqline[0]	= strtok(mesg," ");
	reqline[1] = strtok(NULL," ");
	reqline[2] = strtok(NULL,EOL);
	
	
	if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 )
	{
		write(socked_fd, "HTTP/1.0 400 Bad Request\n", 25);
		return -1;
	}
	
	
	//	GET METHOD
	if(strcmp(reqline[0],"GET") == 0)
	{		
		if ( strncmp(reqline[1], "/\0", 2)==0 )
			//Because if no file is specified, index.html will be opened by default
			reqline[1] = "/cat.jpg";         
	
		strcpy(path, ROOT);
		strcpy(&path[strlen(ROOT)], reqline[1]);
		// printf("file: %s\n", path);
	
		if ( (fd=open(path, 0)) != -1 )	//FILE FOUND
		{
			send(socked_fd, responses[0], strlen(responses[0]), 0);
			
			// determines the file_size
			// fseek(fd, 0L, SEEK_END);
			// bytes_read = ftell(fd);
			// rewind(fd);
			
			//	BEGIN OF CRITICAL SECTION
			pthread_mutex_lock(&lock); 
			
			while ( (temp = read(fd, data_to_send, BUFFER_SIZE)) > 0 )
			{
				bytes_read += temp;
				write(socked_fd, data_to_send, temp);
			}
			// printf("\nbytes_read = %d\n", bytes_read);
			
			pthread_mutex_unlock(&lock);
			//	END OF CRITICAL SECTION
			
			return 22;
		}
		else
		{
			write(socked_fd, responses[1], strlen(responses[1])); //FILE NOT FOUND
			return 0;
		}
	}
	
	//	POST METHOD
	else if(strcmp(reqline[0],"POST") == 0)
	{
		printf("reqline[0]	= %s \n",reqline[0]);
		printf("reqline[1]	= %s \n",reqline[1]);
		printf("reqline[2]	= %s \n",reqline[2]);
	}
	
}
///////////////////////////////////////////////////////////////////////////////////////////////

int respond(void *arg)
{
	// optimal initial BUFFER_SIZE = ???
	char mesg[BUFFER_SIZE];
	int bytes_read, socked_fd, thread_ret;
	
	memset((void*)mesg, (int)'\0', BUFFER_SIZE);
	
	// get socket fd
	// ptr_thread_arg targ = (ptr_thread_arg)arg;
	// socked_fd = (ptr_thread_arg)targ->socket;
	socked_fd = *((int *)arg);
	
    bytes_read = read(socked_fd, mesg, BUFFER_SIZE);
	if (bytes_read < 0) // receive error
	{
		fprintf(stderr,"Error receiving data from client.\n");
		return -1;
	}
	else if (bytes_read == 0) // socket closes
	{
		fprintf(stderr,"Client disconnected unexpectedly.\n");
        return -1;
	}
	// msg received
	
	// handle request type properly
	thread_ret = handle_request(mesg, socked_fd);
	
    // write(socked_fd, hello, strlen(hello));
	shutdown (socked_fd, SHUT_RDWR);
	close(socked_fd);
	
	return thread_ret;
}
///////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    pthread_t threads[MAX_CONNECTIONS];
	void * retvals[MAX_CONNECTIONS] = {NULL};
    struct sockaddr_in address;
    int server_fd, new_socket, PORT;
    int addrlen = sizeof(address);
    int i = 0, j = 0;
	
	// If port number is not specified, set default http port
	// PORT = (argv[1] != ' ') ? atoi(argv[1]) : 80;
	PORT = 80;
	
	// ROOT to serve files
	ROOT = getenv("PWD");
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(1);
    }
	
	address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

	// Attach socket to the address:port specified 
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
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
		
        printf("\033[92mAceitando a requisição # %d \033[0m\n",j);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Cannot accept connection");
            exit(1);
        }
        printf("Conexão estabelecida. \n");
		
		while(retvals[i % MAX_CONNECTIONS] != NULL)
		{
			i++;
		}
		
	
        if (pthread_create(&threads[i], NULL, respond, &new_socket) != 0)
		{
			fprintf(stderr, "error: Cannot create thread # %d\n", i);
			break;	
		}
	
		if (pthread_join(threads[i], &retvals[i]) != 0)
        {
			fprintf(stderr, "error: Cannot join thread # %d\n", i);
			break;
        }	
		else 
		{
			printf("Thread returns = %d @%p\n",retvals[i],retvals[i]);
			retvals[i] = NULL;
		}
		
		j++;
    }

    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////

