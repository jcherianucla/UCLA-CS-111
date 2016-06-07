#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/wait.h>

//Arbitrary buffer limit
unsigned int BUFF_LIMIT = 128;
//Used to reference the thread created in the main proc
pthread_t thread_id;
//Used to reference the child pid for successful koll and waits
pid_t proc_id;
//Information about whether the shell is running or not
int sh_flag = 0;
//User buffer to store the read information
char *buffer; 
//Two pipes to communicate to the shell (p1) and from the shell (p2)
int p1_fd[2];
int p2_fd[2];
//Saved state of the terminal, used for restoration
struct termios saved_terminal_state;

//Restores the terminal, and if we used --shell, it will print the exit status of the
//shell
void restore() 
{ 
	tcsetattr(STDIN_FILENO,TCSANOW,&saved_terminal_state); 
	if(sh_flag)
	{
		int status = 0;
		waitpid(proc_id, &status,0);
		//Used to check if the exit status was "normal"
		if(WIFEXITED(status)) { printf("Shell Exit Status: %d\n", WEXITSTATUS(status)); }
		else if(WIFSIGNALED(status)) { printf("Shell Exit due to Signal Num: %d\n", WTERMSIG(status)); }
		else { printf("Shell Exited Normally"); }
	}
}

//Function used to create a pipe for both pipes
void create_pipe(int fd[2])
{
	if(pipe(fd) == -1)
	{
		perror("Pipe Error");
		exit(EXIT_FAILURE);
	}
}

//Closes all pipes. Useful for before killing the shell
void close_all_pipes()
{
	close(p1_fd[0]);
	close(p1_fd[1]);
	close(p2_fd[0]);
	close(p2_fd[1]);
}

//Generic signal handler that will either send a kill on ^C or exit(1) if SIGPIPE
void signal_handler(int sig_num)
{
	if(sh_flag && sig_num == SIGINT)
	{
		kill(proc_id, SIGINT);
	}
	if(sig_num == SIGPIPE)
	{
		exit(1);
	}
}

//Reads and writes data into a buffer from the specificed file descriptor and to the
//specified output file descripter. There is an extra write if its not a process i.e.
//if it is a thread.
void read_write(int read_fd, int write_fd, int process)
{
	int w_flag = 0, byte_offset = 0;
	ssize_t num_bytes = read(read_fd, buffer, 1);
	if(!num_bytes && !process) { exit(1); }
	if(num_bytes < 0)
	{
		perror("Error");
		exit(EXIT_FAILURE);
	}
	while(num_bytes)
	{
		//Check for ^D specially when reading from terminal
		if(*(buffer + byte_offset) == 4)
		{ 
			if(sh_flag)
			{
				//Cancel thread to avoid race conditions
				pthread_cancel(thread_id);
				close_all_pipes();
				//Send SIGHUP to shell
				kill(proc_id,SIGHUP);
				exit(0);
			}
			exit(0);
		}
		//Special case to map the <cr> or <lf> to <cr><lf>
		if((*(buffer + byte_offset) == '\r' || *(buffer+ byte_offset) == '\n') && !sh_flag)
		{
			char temp_buff[2] = {'\r','\n'};
			write(write_fd, temp_buff, 2);
			if(process) { write(p1_fd[1], temp_buff, 2); }
			byte_offset++;
			continue;
		}
		w_flag = 0;
		//If we are about to overflow - example case would be copying and pasting
		//a very large chunk of data. The function will output the text and overwrite
		//the buffer.
		if(num_bytes + byte_offset >= BUFF_LIMIT)
		{
			while(byte_offset < BUFF_LIMIT)
			{
				write(write_fd, buffer + byte_offset, 1);
				if(process) { write(p1_fd[1], buffer + byte_offset, 1); }
				byte_offset++;
				w_flag = 1;
			}
			if(!w_flag) 
			{
				write(write_fd, buffer+byte_offset,1); 
				if(process) { write(p1_fd[1], buffer+byte_offset, 1);}
			}
			num_bytes = read(read_fd, buffer,1);
			if(!num_bytes && !process) { exit(1); }
			byte_offset = 0;
			continue;
		}
		write(write_fd,buffer+byte_offset,1);
		if(process) { write(p1_fd[1], buffer+byte_offset, 1); }
		byte_offset++;
		num_bytes = read(read_fd, buffer + byte_offset,1);
		//If we read and EOF from the shell.
		if(!num_bytes && !process) { exit(1); }
	}
}

//This function will run on the non-main thread to gather the output from the shell
void *seize_output()
{
	close(p1_fd[0]);
	close(p2_fd[1]);
	read_write(p2_fd[0],1,0);
	exit(0);
}

//Execute the shell by first duplicating all the necessary file descriptors to the
//correct ends of the pipes and then running bash through the exec syscall.
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

int main(int argc, char **argv)
{
	int opt = 0;
	buffer = (char*)malloc(BUFF_LIMIT * sizeof(char));
	//Stores the special terminal settings
	struct termios config_terminal_state;
	static struct option long_opts[] =
	{
		{"shell", no_argument, 0, 's'}
	};
	//Only supports only option
	while((opt = getopt_long(argc,argv,"s",long_opts,NULL)) != -1)
	{
		switch(opt)
		{
			case 's':
				signal(SIGINT, signal_handler);
				signal(SIGPIPE, signal_handler);
				sh_flag = 1;
				break;
			default:
				break;
		}
	}

	//We check stdin is a valid terminal
	if(!isatty(STDIN_FILENO))
	{
		perror("Error");
		exit(EXIT_FAILURE);
	}
	//Register current terminal state
	tcgetattr(STDIN_FILENO,&saved_terminal_state);
	atexit(restore);

	tcgetattr(STDIN_FILENO,&config_terminal_state);

	//Special terminal state of non-canonical and no echo
	config_terminal_state.c_lflag &= ~(ECHO | ICANON);
	config_terminal_state.c_cc[VMIN] = 1;
	config_terminal_state.c_cc[VTIME] = 0;
	config_terminal_state.c_iflag |= ICRNL;
	if(tcsetattr(STDIN_FILENO,TCSANOW, &config_terminal_state) < 0)
	{
		perror("Error");
		exit(EXIT_FAILURE);
	}
	create_pipe(p1_fd);
	create_pipe(p2_fd);
	if(sh_flag)
	{
		proc_id = fork();
		if(proc_id == -1)
		{
			perror("Fork Error");
			exit(EXIT_FAILURE);
		}
		//Child process executes the shell
		if(proc_id == 0)
		{
			exec_shell();
		}
		//Parent process spawns another thread
		else
		{
			pthread_create(&thread_id,NULL, seize_output, NULL);
		}
	}
	read_write(0,1,1);
	//Successful exit
	exit(0);
}

