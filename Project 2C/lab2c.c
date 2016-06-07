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
int num_iters = 1, num_threads = 1, len = 0, total_runs = 0, num_lists = 1; 
char *yield_modes;
//Set the extern opt_yield to zero to begin with
int opt_yield = 0;
//Stores the argument specified for the option for syncing
char sync_arg = '\0';
//Array of locks for each list
pthread_mutex_t* mutex_arr;
//Array of spin-lock vars
int* spin_arr;
//List Array
SortedList_t* list_arr;
//Element Array
SortedListElement_t* elem_arr;
//Indices Array
int* indices;

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
//Prime number hash
unsigned long hash_function(const char* key)
{
	int prime[NUM_ALPHABETS] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101};
	unsigned long uuid = 1;
	for(int i = 0; key[i] != '\0'; i++)
	{
		uuid *= prime[key[i] - 'a'];
	}
	return uuid % num_lists;
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
				pthread_mutex_lock(&mutex_arr[indices[i]]);
				SortedList_insert(&list_arr[indices[i]], &elem_arr[i]); 
				pthread_mutex_unlock(&mutex_arr[indices[i]]);
				break;
			case 's':
				while(__sync_lock_test_and_set(&spin_arr[indices[i]], 1));
				SortedList_insert(&list_arr[indices[i]], &elem_arr[i]);
				__sync_lock_release(&spin_arr[indices[i]]);
				break;
			default:
				SortedList_insert(&list_arr[indices[i]], &elem_arr[i]);
				break;
		}
	}
	len = 0;
	switch(sync_arg)
	{
		case 'm':
			for(int m = 0; m < num_lists; m++)
			{
				pthread_mutex_lock(&mutex_arr[m]);
				len += SortedList_length(&list_arr[m]);
				pthread_mutex_unlock(&mutex_arr[m]);
			}
			break;
		case 's':
			for(int l = 0; l < num_lists; l++)
			{
				while(__sync_lock_test_and_set(&spin_arr[l], 1));
				len += SortedList_length(&list_arr[l]);
				__sync_lock_release(&spin_arr[l]);
			}
			break;
		default:
			for(int l = 0; l < num_lists; l++)
			{
				len += SortedList_length(&list_arr[l]);
			}
			break;
	}

	SortedListElement_t *temp;
	//Lookup and delete based on locking, with the same indexing as before
	for(int j = *(int*)thread_num; j < total_runs; j += num_threads)
	{
		switch(sync_arg)
		{
			case 'm':
				pthread_mutex_lock(&mutex_arr[indices[j]]);
				temp = SortedList_lookup(&list_arr[indices[j]], elem_arr[j].key);
				SortedList_delete(temp);
				pthread_mutex_unlock(&mutex_arr[indices[j]]);
				break;
			case 's':
				while(__sync_lock_test_and_set(&spin_arr[indices[j]], 1));
				temp = SortedList_lookup(&list_arr[indices[j]], elem_arr[j].key);
				SortedList_delete(temp);
				__sync_lock_release(&spin_arr[indices[j]]);
				break;
			default:
				temp = SortedList_lookup(&list_arr[indices[j]],elem_arr[j].key);
				SortedList_delete(temp);
				break;
		}
	}
	return NULL;
}

void initialize_lists()
{
	list_arr = malloc(num_lists * sizeof(SortedList_t));
	for(int l = 0; l < num_lists; l++)
	{
		list_arr[l].key = NULL;
		list_arr[l].next = &list_arr[l];
		list_arr[l].prev = &list_arr[l];
	}
}

void initialize_locks()
{
	//Initialize mutex if argument specified
	if(sync_arg == 'm')
	{
		mutex_arr = malloc(num_lists * sizeof(pthread_mutex_t));
		for(int i = 0; i < num_lists; i++)
		{
			pthread_mutex_init(&mutex_arr[i], NULL);
		}
	} else if(sync_arg == 's')
	{
		spin_arr = malloc(num_lists * sizeof(int));
		for(int i = 0; i < num_lists; i++)
		{
			spin_arr[i] = 0;
		}
	}
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
		{"lists", required_argument, 0, 'l'},
		{"yield", required_argument, 0, 'y'},
		{"sync", required_argument, 0, 's'},
		{"iterations", required_argument, 0, 'i'}
	};

	while((opt = getopt_long(argc, argv, "t:i:s:y:l:", long_opts, NULL)) != -1)
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
				//Lists argument
			case 'l':
				num_lists = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Usage [ti] default threads is 1 and iterations is 1\n");
				exit(1);
		}
	}

	//Total number of runs for the threads to make through the list
	total_runs = num_threads * num_iters;

	//Initialize Circular Doubly Linked List
	initialize_lists();
	//Create a global array of Nodes
	elem_arr = malloc(total_runs * sizeof(SortedListElement_t));

	//Generate a sequence of random keys
	generate_keys();	

	//Initialize the locks for the various lists
	initialize_locks();

	//Create an array of threads
	pthread_t *threads = malloc(num_threads * sizeof(pthread_t));

	//Thread ID's
	int* tids = malloc(num_threads * sizeof(int));

	//Calculate all the indices into the list based on the element key
	indices = malloc(total_runs * sizeof(int));
	for(int i = 0; i < total_runs; i++)
	{
		indices[i] = hash_function(elem_arr[i].key);
	}
	//Start timing
	if(clock_gettime(CLOCK_MONOTONIC, &start) == -1) { perror("Time Start Error"); exit(1); }
	for(int t = 0; t < num_threads; t++)
	{
		tids[t] = t;
		int status = pthread_create(&threads[t], NULL, thread_list, &tids[t]);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	for(int t = 0; t < num_threads; t++)
	{
		int status = pthread_join(threads[t], NULL);
		if(status) { perror("Creation Error"); exit(1); } 
	}
	//Stop timing
	if(clock_gettime(CLOCK_MONOTONIC, &end) == -1) { perror("Time End Error"); exit(1); }


	//Calculate overall time
	long long tot_time = BIL * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
	int num_ops = total_runs * 2;
	//Cost per operation
	long long per_time = tot_time / num_ops;

	fprintf(stdout, "%d threads x %d iterations x (insert + lookup/delete) = %d operations\n", num_threads, num_iters, num_ops);	

	len = 0;
	for(int l = 0; l < num_lists; l++)
	{
		len += SortedList_length(&list_arr[l]);
	}

	//If the length is not zero we have an error due to race conditions
	if(len != 0)
	{
		fprintf(stderr, "ERROR: final count = %d\n", len);
		exit_status = 1;
	}

	fprintf(stdout, "elapsed time: %lldns\n", tot_time);
	fprintf(stdout, "per operation: %lldns\n", per_time);

	//Free malloced items
	free(tids);
	free(elem_arr);
	free(list_arr);
	free(threads);
	free(mutex_arr);
	exit(exit_status);
}
