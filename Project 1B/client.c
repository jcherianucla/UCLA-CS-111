//CLIENT
#include <termios.h>
#include <mcrypt.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>

//GLOBAL VARIABLES
//Store the saved terminal mode
struct termios saved_terminal_state;
//Buffer limit size
unsigned int BUFF_LIMIT = 1;
//Name of the log file
char* log_file;
//Flags and file descriptors for log, encryption and socket
int log_flag = 0, crypt_flag = 0, file_fd, socket_fd;
//ID of the thread
pthread_t thread_id;
//File descriptors of encryption/decryption modules
MCRYPT crypt_fd, decrypt_fd;
//Length of the key from my.key
int key_len;

//Using fstat and the name of the file to grab the contents of the file (the key)
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

//Runs the blowfish ofb encryption algorithm on specified buffeer for given size
void encrypt(char *buff,int crypt_len)
{
	if(mcrypt_generic(crypt_fd, buff, crypt_len) != 0) { perror("Error in encryption"); exit(EXIT_FAILURE); }
}

//Runs the corresponding decryption algorithm on the specified buffer for the size
void decrypt(char * buff,int decrypt_len)
{
	if(mdecrypt_generic(decrypt_fd, buff, decrypt_len) != 0) { perror("Error in decryption"); exit(EXIT_FAILURE); }
}

//Initializes the encryption and decryption modules while performing error checks
void encryption_decryption_init(char* key, int key_len)
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

//Used to restore terminal settings and deinitialize the modules used for encryption
void restore()
{
	tcsetattr(STDIN_FILENO, TCSANOW, &saved_terminal_state);
	if(crypt_flag)
	{
		encryption_decryption_deinit();
	}
}

//Writes the corresponding messages to the log file depending on if it was sent or
//received
void write_to_log(int num_bytes, int send_to_sock, char* buffer, int byte_offset)
{
	if(send_to_sock)
	{
		char log_string[14] = "SENT x bytes: ";	
		log_string[5] = '0' + num_bytes;
		write(file_fd, log_string, 14);
		write(file_fd, buffer+byte_offset, 1);
		write(file_fd, "\n",1);
	}
	else
	{
		char log_string[18] = "RECEIVED x bytes: ";	
		log_string[9] = '0' + num_bytes;
		write(file_fd, log_string, 18);
		write(file_fd, buffer+byte_offset, 1);
		write(file_fd, "\n",1);
	}
}

//Ends the program based on if it was a ^D or EOF, the threads are cancelled and
//the socket is closed, and we write one byte before closing to warn kill the TCP
//connection for immediate reuse and no TC_WAIT time
void end_program(int ctrlD)
{
	if(ctrlD) 
	{
		pthread_cancel(thread_id);
		close(socket_fd);
		exit(0);
	}
	else 
	{
		write(socket_fd, "0", 1);
		close(socket_fd);
		exit(1); 
	}
}

//Reads and writes one character at a time to the respective file descriptors
//taking care of whether the command is from the socket or to the socket, while
//allowing for encryption and corresponding decryption of data. Simultaneously writes
//to stdout and writes to log (post encryption/ pre decryption if encryption turned on)
void read_write(int read_fd, int write_fd, int to_out, int send_to_sock)
{
	int w_flag = 0, byte_offset = 0;
	char buffer[BUFF_LIMIT]; 
	ssize_t num_bytes = read(read_fd, buffer, 1);
	if(num_bytes == 0) { end_program(0); }

	while(num_bytes)
	{
		if(to_out && *(buffer + byte_offset) != '\r' && *(buffer+ byte_offset) != '\n') { write(STDOUT_FILENO, buffer+byte_offset,1); }
		if(crypt_flag && send_to_sock && *(buffer + byte_offset) != '\r' && *(buffer+ byte_offset) != '\n') { encrypt(buffer, BUFF_LIMIT); }
		if(log_flag )  { write_to_log(num_bytes, send_to_sock, buffer, byte_offset); }
		if(crypt_flag && !send_to_sock) { decrypt(buffer, BUFF_LIMIT); }

		if(*(buffer + byte_offset) == 4 && send_to_sock) { end_program(1); }

		//Performs respective mapping of <cr><nl> if to stdout but <nl> if to server/shell
		if(*(buffer + byte_offset) == '\r' || *(buffer+ byte_offset) == '\n') 
		{
			char temp_buff[2] = {'\r','\n'};
			if(to_out) { write(STDOUT_FILENO, temp_buff, 2);}
			if(!send_to_sock)
			{
				write(write_fd, temp_buff, 2);
			}
			else
			{
				if(crypt_flag) { encrypt(temp_buff + 1, 1); }
				write(write_fd, temp_buff + 1, 1);
			} 
			num_bytes = read(read_fd, buffer,1);
			if(num_bytes == 0) { end_program(0); }
			byte_offset = 0;
			continue;
		}

		w_flag = 0;
		//Runs all the time with buffer of size one, but allows for flexibility
		//of increasing the buffer size
		if(num_bytes + byte_offset >= BUFF_LIMIT)
		{
			while(byte_offset < BUFF_LIMIT)
			{
				write(write_fd, buffer + byte_offset, 1);
				byte_offset++;
				w_flag = 1;
			}
			if(!w_flag) 
			{
				write(write_fd, buffer+byte_offset,1); 
			}
			num_bytes = read(read_fd, buffer,1);
			if(num_bytes == 0) { end_program(0); }
			byte_offset = 0;
			continue;
		}
		//Runs if buffer size > 1
		write(write_fd,buffer+byte_offset,1);
		byte_offset++;
		num_bytes = read(read_fd, buffer + byte_offset,1);
		if(num_bytes == 0) { end_program(0); }
	}
}

//Function run by the thread to grab the data from the server over the socket and
//output it to stdout
void* socket_grab(void* sock_fd)
{
	read_write(*(int *)sock_fd, STDOUT_FILENO,0, 0);
}


int main(int argc, char **argv)
{
	int opt = 0, port_num;
	struct termios config_terminal_state;
	struct sockaddr_in server_addr;
	struct hostent* server;
	static struct option long_opts[] =
	{
		{"port", required_argument, 0, 'p'},
		{"log", required_argument, 0, 'l'},
		{"encrypt", no_argument, 0, 'e'}
	};
	while((opt = getopt_long(argc,argv,"p:l:e",long_opts,NULL)) != -1)
	{
		switch(opt)
		{
			case 'p':
				//Grab the port number
				port_num = atoi(optarg);
				break;
			case 'l':
				//Initialize logging options
				log_file = optarg;
				log_flag = 1;
				//Creates the file (overwriting a previous one with same name)
				file_fd = creat(log_file, S_IRWXU);
				break;
			case 'e':
				//Turn on Encryption
				crypt_flag = 1;
				//Get the key
				char *key = grab_key("my.key");
				//Initialize the encryption and decryption
				encryption_decryption_init(key, key_len);
				break;
			default:
				//Default usage message
				fprintf(stderr, "Usage [le] hostname port_num");
				break;
		}
	}
	//Make terminal non-canonical mode
	tcgetattr(STDIN_FILENO, &saved_terminal_state);
	atexit(restore);

	tcgetattr(STDIN_FILENO, &config_terminal_state);

	config_terminal_state.c_lflag &= ~(ECHO | ICANON);
	config_terminal_state.c_cc[VMIN] = 1;
	config_terminal_state.c_cc[VTIME] = 0;
	if(tcsetattr(STDIN_FILENO, TCSANOW, &config_terminal_state) < 0)
	{
		perror("Error");
		exit(EXIT_FAILURE);
	}
	//Socket initialzation
	//Establish socket file descriptor, for TCP with internet domain
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd < 0) { perror("Error opening socket"); exit(0); }
	server = gethostbyname("localhost");
	if(server == NULL) { fprintf(stderr, "Cannot find host"); exit(0); }
	//Initialize the server address to zero and then correctly assign it
	memset((char*) &server_addr,0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy((char *) &server_addr.sin_addr.s_addr,
			(char*) server->h_addr,
			server->h_length);
	server_addr.sin_port = htons(port_num);
	//Connect to our established server using the socket
	if(connect(socket_fd,(struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) { perror("Error connecting"); exit(0); }
	//Read and write to our socket
	pthread_create(&thread_id, NULL, socket_grab, &socket_fd);
	read_write(STDIN_FILENO, socket_fd,1, 1);
	exit(0);
}
