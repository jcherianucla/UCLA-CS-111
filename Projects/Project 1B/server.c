//SERVER
#include <stdio.h>
#include <string.h>
#include <mcrypt.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>

//GLOBAL VARIABLES
//Buffer size
unsigned int BUFF_LIMIT = 1;
//Pair of pipes for communication between shell and main process
int p1_fd[2];
int p2_fd[2];
//Flags for encryption, length of key generated and socket file descriptors
int crypt_flag = 0, key_len, socket_fd, newsocket_fd;
//Child process and thread id's
pid_t child_id;
pthread_t thread_id;
//File descriptors for encryption modules
MCRYPT crypt_fd, decrypt_fd;

//Uses fstat and the name of the file to grab the contents of the file (the key)
char* grab_key(char *filename)
{
	struct stat key_stat;
	int key_fd = open(filename, O_RDONLY);
	if(fstat(key_fd, &key_stat) < 0) { perror("fstat"); exit(EXIT_FAILURE); }
	char* key = (char*) malloc(key_stat.st_size * sizeof(char));
	if(read(key_fd, key, key_stat.st_size) < 0) { perror("read"); exit(EXIT_FAILURE); }
	key_len = key_stat.st_size;
	return key;
}

//Runs the blowfish ofb encryption algorithm on specified buffer for specified size
void encrypt(char *buff, int crypt_len)
{
	if(mcrypt_generic(crypt_fd, buff, crypt_len) != 0) { perror("Error in encryption"); exit(EXIT_FAILURE); }
}

//Runs the corresponding decryption algorithm on the specified buffer for the size
void decrypt(char * buff, int decrypt_len)
{
	if(mdecrypt_generic(decrypt_fd, buff, decrypt_len) != 0) { perror("Error in decryption"); exit(EXIT_FAILURE); }
}

//Initializes the encryption and decryption modules while performing error checks
void encryption_decryption_init(char *key, int key_len)
{
	crypt_fd = mcrypt_module_open("blowfish", NULL, "ofb", NULL);
	if(crypt_fd == MCRYPT_FAILED) { perror("Error opening module"); exit(EXIT_FAILURE); }
	if(mcrypt_generic_init(crypt_fd, key, key_len, NULL) < 0) { perror("Error while initializing encrypt"); exit(EXIT_FAILURE); }

	decrypt_fd = mcrypt_module_open("blowfish", NULL, "ofb", NULL);
	if(decrypt_fd == MCRYPT_FAILED) { perror("Error opening module"); exit(EXIT_FAILURE); }
	if(mcrypt_generic_init(decrypt_fd, key, key_len, NULL) < 0) { perror("Error while initializing decrypt"); exit(EXIT_FAILURE); }
}

//Deinitializes (closes) the modules that were opened for encryption/decryption
void encryption_decryption_deinit()
{
	mcrypt_generic_deinit(crypt_fd);
	mcrypt_module_close(crypt_fd);

	mcrypt_generic_deinit(decrypt_fd);
	mcrypt_module_close(decrypt_fd);
}

//Closes all the open file descriptors
void close_all()
{
	close(0);
	close(1);
	close(2);
	close(p1_fd[1]);
	close(p2_fd[0]);
	close(socket_fd);
}

//Ends the program by killing the child, closing all pipes, and exiting with the
//respective codes. The thread is cancelled if the command is not from the socket
void end_program(int from_socket)
{
	close_all();
	kill(child_id, SIGKILL);
	if(from_socket) { exit(1); }
	else { 
		pthread_cancel(thread_id);
		exit(2); }
}

//Reads and writes one character at a time to the respective file descriptors
//taking care of whether the command is from the socket or to the socket, while
//allowing for encryption and corresponding decryption of data
void read_write(int read_fd, int write_fd, int from_sock)
{
	int w_flag = 0, byte_offset = 0;
	char buffer[BUFF_LIMIT]; 
	ssize_t num_bytes = read(read_fd, buffer, 1);
	if(num_bytes == 0) { end_program(from_sock); }
	while(num_bytes)
	{
		if(crypt_flag && from_sock) { decrypt(buffer, BUFF_LIMIT); } 

		if(*(buffer + byte_offset) == 4)
		{ 
			end_program(from_sock);
		}

		w_flag = 0;
		//Runs all the time with buffer of size one, but allows for flexibility
		//of increasing the buffer size
		if(num_bytes + byte_offset >= BUFF_LIMIT)
		{
			while(byte_offset < BUFF_LIMIT)
			{
				if(crypt_flag && !from_sock) { encrypt(buffer, BUFF_LIMIT); }
				write(write_fd, buffer + byte_offset, 1);
				byte_offset++;
				w_flag = 1;
			}
			if(!w_flag) 
			{
				if(crypt_flag && !from_sock) { encrypt(buffer, BUFF_LIMIT); }
				write(write_fd, buffer+byte_offset,1); 
			}
			num_bytes = read(read_fd, buffer,1);
			if(num_bytes == 0) { end_program(from_sock); }
			byte_offset = 0;
			continue;
		}
		//Runs if buffer size > 1
		if(crypt_flag && !from_sock) { encrypt(buffer, BUFF_LIMIT); }
		write(write_fd,buffer+byte_offset,1);
		byte_offset++;
		num_bytes = read(read_fd, buffer + byte_offset,1);
		if(num_bytes == 0) { end_program(from_sock); }
	}
}

//Creates pipes
void create_pipe(int fd[2])
{
	if(pipe(fd) == -1)
	{
		perror("Pipe Error");
		exit(EXIT_FAILURE);
	}
}

//Used by the child process to redirect the respective file descriptors and 
//execute the bash shell
void exec_shell()
{
	close(p1_fd[1]);
	close(p2_fd[0]);
	dup2(p1_fd[0], 0);
	dup2(p2_fd[1], 1);
	dup2(p2_fd[1], 2);
	close(p1_fd[0]);
	close(p2_fd[1]);
	if(execvp("/bin/bash",NULL) == -1)
	{
		perror("Exec Error");
		exit(EXIT_FAILURE);
	}
}

//Called by the thread to output the shell data to the socket to the client
void* transfer_shell(void* newsocket_fd)
{
	close(p1_fd[0]);
	close(p2_fd[1]);
	read_write(p2_fd[0],1, 0);
}

int main(int argc, char **argv)
{
	int opt = 0, port_num, client_len;
	struct sockaddr_in server_addr, client_addr;
	static struct option long_opts[] =
	{
		{"port", required_argument, 0, 'p'},
		{"encrypt", no_argument, 0, 'e'}
	};

	while((opt = getopt_long(argc, argv, "p:e", long_opts, NULL)) != -1)
	{
		switch(opt)
		{
			case 'p':
				//Grab port number
				port_num = atoi(optarg);
				break;
			case 'e':
				//Turn on Encryption
				crypt_flag = 1;
				//Get the key
				char* key = grab_key("my.key");
				//Initialize encryption and decryption
				encryption_decryption_init(key, key_len);
				break;
			default:
				//Usage message
				fprintf(stderr, "Usage [e] port_number");
				break;
		}
	}
	//Set up socket connection
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0) { perror("Error opening socket"); exit(1); }
	memset((char*) &server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_num);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		perror("Error binding socket");
		exit(1);
	}
	//Listen for data over the socket
	listen(socket_fd, 5);
	client_len = sizeof(client_addr);
	//Block while accepting data input as a stream
	newsocket_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_len);
	if(newsocket_fd < 0) { perror("Error accepting the socket"); exit(1); }
	create_pipe(p1_fd);
	create_pipe(p2_fd);
	child_id = fork();
	if(child_id < 0) { perror("Error forking"); exit(1); }
	//Run the child process that is the shell
	else if(child_id == 0)	{ exec_shell(); }
	else
	{
		//Create the thread to send shell's output to the client
		pthread_create(&thread_id, NULL, transfer_shell, &newsocket_fd);
	}
	//Redirect stdin/stdout/stderr to the socket
	dup2(newsocket_fd, 0);
	dup2(newsocket_fd, 1);
	dup2(newsocket_fd, 2);
	close(newsocket_fd);
	read_write(0, p1_fd[1], 1);
	encryption_decryption_deinit();
	exit(0);
}
