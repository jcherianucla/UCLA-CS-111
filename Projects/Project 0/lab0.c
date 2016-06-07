#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>

//Default usage message printed if incorrect argument passed
void print_usage()
{
  printf("Usage: lab0 [sc] -i filename -o filename2\n");
}

//Function that reads from fd 0 and outputs byte by byte to fd 1
void read_and_write(int i_fd, int o_fd)
{
  char* buffer;
  buffer = (char*) malloc(sizeof(char));
  ssize_t status = read(i_fd,buffer,1);
  while(status > 0)
  {
	write(o_fd,buffer,1);
	status = read(i_fd,buffer,1);
  }
  free(buffer);
}

//Signal handler that prints error message if SIGSEGV encountered
void signal_handler(int signal_num)
{
  if(signal_num == SIGSEGV)
  {
	perror("Segmentation fault caught. Errno reports");
	exit(3);
  }
}

int main(int argc, char** argv)
{
  int opt = 0;
  //Arguments allowed by program
  static struct option long_opts[] =
  {
	{"input",	 required_argument, 0, 'i'},
	{"output", 	 required_argument, 0, 'o'},
	{"segfault", no_argument, 		0, 's'},
	{"catch", 	 no_argument, 		0, 'c'}
  };
  char* input_file = NULL;
  char* output_file = NULL;
  char* n = NULL; //Used to cause a Segmentation fault
  int seg_flag = 0; //Used to register a segmentation fault for handler
  while( (opt = getopt_long(argc, argv, "i:o:sc", long_opts, NULL)) != -1)
  {
	switch(opt)
	{
	  case 'i':
		input_file = optarg;
		break;
	  case 'o':
		output_file = optarg;
		break;
	  case 's':
		seg_flag = 1;
		break;
	  case 'c':
		signal(SIGSEGV, signal_handler);
		break;
	  default:
		print_usage();
		exit(EXIT_FAILURE); //Exit with status of failure
	}
  }
  int in_fd = 0, o_fd = 1;
  if(input_file)
  {
	in_fd = open(input_file, O_RDONLY);
	//If successful open, then clone input file descriptor to file
	if(in_fd >= 0)
	{
	  close(0);
	  dup(in_fd);
	  close(in_fd);
	}
	else
	{
	  perror("Opening file produced error. Errno reports");
	  exit(1);
	}

  }
  if(output_file)
  {
	o_fd = creat(output_file, S_IRWXU);
	//If successful creat, then clone output file descriptor to file
	if(o_fd >= 0)
	{
	  close(1);
	  dup(o_fd);
	  close(o_fd);
	}
	else
	{
	  perror("Creating file produced error. Errno reports");
	  exit(2);
	}
  }
  //If segfault specified, then cause a segmentation fault by dereferencing null
  if(seg_flag){
  	char deref = *n;
  }
  //Perform main task of program - read from new fd to new output fd
  read_and_write(0,1);
  //Successful exit of 0
  exit(0);
}
