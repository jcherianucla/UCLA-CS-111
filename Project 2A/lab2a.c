#define _GNU_SOURCE

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#define BIL 1000000000L

//GLOBAL VARIABLES
//Defaults the number of iterations to 1 and sets the yield flag to 0
//and the exclusion flag (used in test and set) to 0
int num_iters = 1, opt_yield = 0, exclusion = 0;
//Stores the argument specified for the option for syncing
char sync_arg;
//Mutex
pthread_mutex_t mutex;

//General addition function provided by the specification,
//that demonstrates the possibilities of race conditions.
void add(long long *pointer, long long value)
{
	long long sum = *pointer + value;
	if(opt_yield) { pthread_yield(); }
	*pointer = sum;
}

//Special add function using the atomic compare and swap function
void add_compare(long long *pointer, long long value)
{
	long long prev;
	long long sum;
	do
	{
		prev = *pointer;
		sum = prev + value;
		if(opt_yield) { pthread_yield(); }
	} while(__sync_val_compare_and_swap(pointer, prev, sum) != prev);
}

//Function run by thread
void* thread_add(void* counter)
{
	//Addition by 1, for the different syncing cases
	for(int i = 0; i < num_iters; i++)
	{
		//Mutex locking
		if(sync_arg == 'm')
		{
			pthread_mutex_lock(&mutex);
			add((long long*) counter, 1);
			pthread_mutex_unlock(&mutex);
		} 
		//Spin locking
		else if (sync_arg == 's')
		{
			while(__sync_lock_test_and_set(&exclusion, 1));
			add((long long *) counter, 1);
			__sync_lock_release(&exclusion);
		} 
		//Compare and swap
		else if (sync_arg == 'c')
		{
			add_compare((long long*) counter, 1);
		} 
		//Generic add
		else
		{
			add((long long*) counter, 1);
		}
	}
	//Addition by -1, for the different syncing cases
	for(int j = 0; j < num_iters; j++)
	{
		//Mutex locking
		if(sync_arg == 'm')
		{
			pthread_mutex_lock(&mutex);
			add((long long*) counter, -1);
			pthread_mutex_unlock(&mutex);
		} 
		//Spin locking
		else if (sync_arg == 's')
		{
			while(__sync_lock_test_and_set(&exclusion, 1));
			add((long long *) counter, -1);
			__sync_lock_release(&exclusion);
		} 
		//Compare and swap
		else if (sync_arg == 'c')
		{
			add_compare((long long*) counter, -1);
		}
		//Generic add
		else
		{
			add((long long*) counter, -1);
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	//Stores the number of threads and defaults to 1, the exit status defaults to 0 unless of error
	int opt = 0, num_threads = 1, exit_status = 0;
	struct timespec start, end;
	static struct option long_opts[] = 
	{
		{"threads", required_argument, 0, 't'},
		{"yield", no_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'},
		{"iterations", required_argument, 0, 'i'}
	};

	while((opt = getopt_long(argc, argv, "t:i:", long_opts, NULL)) != -1)
	{
		switch(opt)
		{
			//Threads argument
			case 't':
				num_threads = atoi(optarg);
				break;
			//Iterations argument
			case 'i':
				num_iters = atoi(optarg);
				break;
			//Yielding argument
			case 'y':
				opt_yield = 1;
				break;
			//Syncing argument
			case 's':
				if(strlen(optarg) == 1 && optarg[0] == 'm') { sync_arg = 'm'; }
				else if(strlen(optarg) == 1 && optarg[0] == 's') { sync_arg = 's'; }
				else if(strlen(optarg) == 1 && optarg[0] == 'c') { sync_arg = 'c'; }
				else { fprintf(stderr, "Incorrect argument for sync. m = mutex; s = spin-lock; c = compare"); exit(1); }
				break;
			default:
				fprintf(stderr, "Usage [ti] default threads is 1 and iterations is 1");
				break;
		}
	}

	pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
	long long counter = 0;
	//Initialize mutex if argument specified
	if(sync_arg == 'm') { pthread_mutex_init(&mutex, NULL); }
	//Start timing
	if(clock_gettime(CLOCK_MONOTONIC, &start) == -1) { perror("Time Start Error"); exit(1); }
	//Thread creation
	for(int t = 0; t < num_threads; t++)
	{
		int status = pthread_create(threads + t, NULL, thread_add, &counter);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	//Thread joining
	for(int t = 0; t < num_threads; t++)
	{
		int status = pthread_join(*(threads + t), NULL);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	//Stop timing
	if(clock_gettime(CLOCK_MONOTONIC, &end) == -1) { perror("Time End Error"); exit(1); }
	//Calculate overall time
	long tot_time = BIL * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	int num_ops = num_threads * num_iters * 2;
	//Cost per operation
	long per_time = tot_time / num_ops;

	fprintf(stdout, "%d threads x %d iterations x (add + subtract) = %d operations\n", num_threads, num_iters, num_ops);	
	//If the counter is not zero we have an error due to race conditions
	if(counter)
	{
		fprintf(stderr, "ERROR: final count = %d\n", counter);
		exit_status = 1;
	}
	fprintf(stdout, "elapsed time: %dns\n", tot_time);
	fprintf(stdout, "per operation: %dns\n", per_time);
	exit(exit_status);
}
