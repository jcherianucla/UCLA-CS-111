#define _GNU_SOURCE

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "SortedList.h"
#include <time.h>

#define BIL 1000000000L
#define KEY_LEN 10
#define NUM_ALPHABETS 26

//GLOBAL VARIABLES
//Defaults the number of iterations and threads to 1 and sets list len to 0 
//and the exclusion flag (used in test and set) to 0
int num_iters = 1, num_threads = 1, len = 0, total_runs = 0, exclusion = 0;
char *yield_modes;
//Set the extern opt_yield to zero to begin with
int opt_yield = 0;
//Stores the argument specified for the option for syncing
char sync_arg = '\0';
//Mutex
pthread_mutex_t mutex;
//List
SortedList_t *list;
//Element Array
SortedListElement_t* elem_arr;

//Update the opt_yield to hold the defined macros based on the passed in arguments
//to yield
void set_yield_modes(char* modes)
{
	const char accepted[] = { 'i','d','s'};
	for(int i = 0; *(modes+i) != '\0'; i++)
	{
		if(*(modes+i) == accepted[0])
		{
			opt_yield |= INSERT_YIELD;
		}
		else if(*(modes+i) == accepted[1])
		{
			opt_yield |= DELETE_YIELD;
		}
		else if(*(modes+i) == accepted[2])
		{
			opt_yield |= SEARCH_YIELD;
		}
		else
		{
			//Error! Invalid character!
			exit(EXIT_FAILURE);
		}
	}
}

void* thread_list(void* thread_num)
{
	//Insert based on locking, and operate on chunks of nodes indexed by
	//the thread id in the array of nodes
	for(int i = *(int*)thread_num; i < total_runs; i += num_threads)
	{
		switch(sync_arg)
		{
			case 'm':
				pthread_mutex_lock(&mutex);
				SortedList_insert(list, &elem_arr[i]); 
				pthread_mutex_unlock(&mutex);
				break;
			case 's':
				while(__sync_lock_test_and_set(&exclusion, 1));
				SortedList_insert(list, &elem_arr[i]);
				__sync_lock_release(&exclusion);
				break;
			default:
				SortedList_insert(list, &elem_arr[i]);
				break;
		}
	}

	len = SortedList_length(list);
	SortedListElement_t *temp;
	//Lookup and delete based on locking, with the same indexing as before
	for(int j = *(int*)thread_num; j < total_runs; j += num_threads)
	{
		switch(sync_arg)
		{
			case 'm':
				pthread_mutex_lock(&mutex);
				temp = SortedList_lookup(list, elem_arr[j].key);
				SortedList_delete(temp);
				pthread_mutex_unlock(&mutex);
				break;
			case 's':
				while(__sync_lock_test_and_set(&exclusion, 1));
				temp = SortedList_lookup(list, elem_arr[j].key);
				SortedList_delete(temp);
				__sync_lock_release(&exclusion);
				break;
			default:
				temp = SortedList_lookup(list, elem_arr[j].key);
				SortedList_delete(temp);
				break;
		}
	}
	len = SortedList_length(list);
	return NULL;
}

void generate_keys()
{
	//Seed the random generator
	srand(time(NULL));

	for(int t = 0; t < total_runs; t++)
	{
		//Generate Random Key
		int rand_length = rand() % KEY_LEN + 5; //Generate between 5 and KEY_LEN
		int rand_letter = rand() % NUM_ALPHABETS;
		char* rand_key = malloc((rand_length+1) * sizeof(char));
		for(int i = 0; i < rand_length; i++)
		{
			rand_key[i] = 'a' + rand_letter;
			rand_letter = rand() % 26;
		}
		rand_key[rand_length] = '\0';	
		//Set respective nodes key to random key
		elem_arr[t].key = rand_key;
	}
}

int main(int argc, char **argv)
{
	//The exit status defaults to 0 unless of error
	int opt = 0, exit_status = 0;
	struct timespec start, end;
	static struct option long_opts[] = 
	{
		{"threads", required_argument, 0, 't'},
		{"yield", required_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'},
		{"iterations", required_argument, 0, 'i'}
	};

	while((opt = getopt_long(argc, argv, "t:i:s:y:", long_opts, NULL)) != -1)
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
				set_yield_modes(optarg);
				break;
			//Syncing argument
			case 's':
				if(strlen(optarg) == 1 && optarg[0] == 'm') { sync_arg = 'm'; }
				else if(strlen(optarg) == 1 && optarg[0] == 's') { sync_arg = 's'; }
				else { fprintf(stderr, "Incorrect argument for sync. m = mutex; s = spin-lock\n"); exit(1); }
				break;
			default:
				fprintf(stderr, "Usage [ti] default threads is 1 and iterations is 1\n");
				exit(1);
		}
	}
	//Initialize mutex if argument specified
	if(sync_arg == 'm') { pthread_mutex_init(&mutex, NULL); }

	//Total number of runs for the threads to make through the list
	total_runs = num_threads * num_iters;

	//Initialize Circular Doubly Linked List
	list = malloc(sizeof(SortedList_t));
	list->key = NULL;
	list->next = list;
	list->prev = list;

	//Create a global array of Nodes
	elem_arr = malloc(total_runs * sizeof(SortedListElement_t));

	generate_keys();	

	//Create an array of threads
	pthread_t *threads = malloc(num_threads * sizeof(pthread_t));

	//Thread ID's
	int* tids = malloc(num_threads * sizeof(int));

	//Start timing
	if(clock_gettime(CLOCK_MONOTONIC, &start) == -1) { perror("Time Start Error"); exit(1); }
	for(int t = 0; t < num_threads; t++)
	{
		tids[t] = t;
		int status = pthread_create(threads + t, NULL, thread_list, &tids[t]);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	//test_print(list);
	for(int t = 0; t < num_threads; t++)
	{
		int status = pthread_join(threads[t], NULL);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	//Stop timing
	if(clock_gettime(CLOCK_MONOTONIC, &end) == -1) { perror("Time End Error"); exit(1); }

	//Free malloced items
	free(tids);
	free(elem_arr);
	free(threads);

	//Calculate overall time
	long tot_time = BIL * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	int num_ops = total_runs * 2;
	//Cost per operation
	long per_time = tot_time / num_ops;
	
	fprintf(stdout, "%d threads x %d iterations x (insert + lookup/delete) = %d operations\n", num_threads, num_iters, num_ops);	

	//If the length is not zero we have an error due to race conditions
	if(len != 0)
	{
		fprintf(stderr, "ERROR: final count = %d\n", len);
		exit_status = 1;
	}

	fprintf(stdout, "elapsed time: %dns\n", tot_time);
	fprintf(stdout, "per operation: %dns\n", per_time);
	exit(exit_status);
}
