#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h> 
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

//Shared variables required to control the producer/consumer threads
int current_product_id = 0;
int total_number_produced = 0;
int total_number_consumed = 0;
pthread_mutex_t *producer_consumer_mutex;
pthread_cond_t *consumer_cond, *producer_cond;
struct Product_queue *the_queue;

struct timespec timeToWait;

//Shared varibales that are initiated by the program inputs
int NUMBER_OF_PRODUCERS;
int NUMBER_OF_CONSUMERS;
int TOTAL_NUMBER_OF_PRODUCTS;
int SIZE_OF_QUEUE;
int SCHEDULE_CHOICE;
int VALUE_OF_QUANTUM;
int RNG_SEED;

//Statistic varaibles for experimenting and getting data
double max_turn_around = 0;
double min_turn_around = -1;
double avg_turn_around = 0;

double min_wait = -1;
double max_wait = 0;
double avg_wait = 0;

//A struct for representing products
//clock_t is used for simplicity
struct Product{
	
	int product_id;
	struct timeval birthday;
	struct timeval insert_time;
	int life;
	
};

void print_product(struct Product p){
	
	printf("The product id is %d\n", p.product_id);
	printf("The product birthday is %lu\n", p.birthday.tv_usec/1000 + p.birthday.tv_sec * 1000);
	printf("The product life is %d\n\n", p.life);
	
}

//Variables for representing the queue
struct Product_queue{
	
	pthread_mutex_t *q_mutex;
	pthread_cond_t *is_not_full, *is_not_empty;
	int buffer_size;
	int head;
	int tail;
	int is_empty;
	int is_full;
	int mode;
	struct Product buffer[];
	
};

//Function to initialize the queue
struct Product_queue *init_queue(int size, int s_mode){
	
	int i = 0; //Loop variable
	
	//Sets up the product buffer
	struct Product_queue *ret_q = malloc(sizeof *ret_q + size * sizeof ret_q->buffer[0]);
	
	//Sets up the mutex and condition variables
	ret_q->q_mutex = (pthread_mutex_t *) malloc(sizeof (pthread_mutex_t));
	ret_q->is_not_full = (pthread_cond_t *) malloc(sizeof (pthread_cond_t));
	ret_q->is_not_empty = (pthread_cond_t *) malloc(sizeof (pthread_cond_t));
	
	pthread_mutex_init(ret_q->q_mutex, NULL);
	pthread_cond_init(ret_q->is_not_full, NULL);
	pthread_cond_init(ret_q->is_not_empty, NULL);
	
	//Sets the integers that the queue uses
	ret_q->buffer_size = size;
	ret_q->is_empty = 1;
	ret_q->is_full = 0;
	ret_q->head = 0;
	ret_q->tail = 0;
	ret_q->mode = s_mode;
	
	return ret_q;
	
}

//Function to delete the queue once the program is finished
void destroy_queue(struct Product_queue *q){
	
	int i = 0;
	pthread_mutex_destroy(q->q_mutex);
	pthread_cond_destroy(q->is_not_empty);
	pthread_cond_destroy(q->is_not_full);
	free(q->q_mutex);
	free(q->is_not_empty);
	free(q->is_not_full);
	free(q);
	
}

//Function to add a product to the queue
void queue_add(struct Product_queue *q, struct Product in_p){
	
	q->buffer[q->tail] = in_p;
	q->tail += 1;
	if(q->tail == q->buffer_size){
		q->tail = 0;
	}
	if(q->tail == q->head){
		q->is_full = 1;
	}
	q->is_empty = 0;
	
}

//Function to remove a product from the queue
struct Product *queue_remove(struct Product_queue *q){
	
	struct Product *ret_product = &(q->buffer[q->head]);
	q->head +=1;
	if(q->head == q->buffer_size){
		q->head = 0;
	}
	if(q->head == q->tail){
		q->is_empty = 1;
	}
	q->is_full = 0;
	
	return ret_product;
	
}

//Function to peak at the next product in the queue
struct Product *queue_peek(struct Product_queue *q){
	
	return &(q->buffer[q->head]);
	
}

//The producer function, which will be executed by a pthread
void* producer(void *t_id){
	
	struct Product_queue *q = the_queue;
	int id = *((int *)t_id);
	
	while(TOTAL_NUMBER_OF_PRODUCTS > total_number_produced){
		
		pthread_mutex_lock(q->q_mutex);//locks the mutex
		
		if(TOTAL_NUMBER_OF_PRODUCTS <= total_number_produced){//Makes sure that there are still products to produce
			pthread_mutex_unlock(q->q_mutex);
			return(NULL);
		}
		
		//Creates a new product
		struct Product p;
		p.product_id = total_number_produced;
		gettimeofday(&p.birthday, NULL);
		p.life = rand() % 1024;
		total_number_produced += 1;
		
		//waits for the queue not to be full
		while(q->is_full == 1){
			pthread_cond_wait(q->is_not_full, q->q_mutex);
		}
		
		//Adds the product
		gettimeofday(&p.insert_time, NULL);
		queue_add(q, p);
		
		printf("Producer %d has made a product\n", id);
		print_product(p);
		
		//Unlocks the mutex, signals the queue is not empty
		/*
		printf("Value of full is %d\n", q->is_full);
		printf("Empty is %d\n", q->is_empty);
		printf("Number produced is %d\n", total_number_produced);
		printf("Number consumed is %d\n", total_number_consumed);
		*/
		pthread_mutex_unlock(q->q_mutex);
		pthread_cond_signal(q->is_not_empty);
		
		usleep(100000);//Sleep for 100 miliseconds
		
	}
	
	return (NULL);
	
}

//The fibbonacci function for round robin scheduling
int fn(int x){
	
	if(x <= 0){
		return 0;
	}
	if(x == 1){
		return 1;
	}
	return fn(x - 1) + fn(x - 2);
	
}

//The consumer function, which will be executed by a pthread
void* consumer(void *t_id){
	
	struct Product_queue *q = the_queue;
	struct timeval current_time;
	double elapsed_time_wait = 0;
	double elapsed_time_turn = 0;
	int id = *((int *)t_id);
	
	if(q->mode == 0){
		while(TOTAL_NUMBER_OF_PRODUCTS > total_number_consumed){
			
			pthread_mutex_lock(q->q_mutex);//locks the mutex
			
			//printf("Total number consumed%d\n", total_number_consumed);
			
			if(TOTAL_NUMBER_OF_PRODUCTS <= total_number_consumed){
				pthread_mutex_unlock(q->q_mutex);
				return(NULL);
			}
			
			//Waits for the queue not to be empty
			while(q->is_empty == 1){
				pthread_cond_wait(q->is_not_empty, q->q_mutex);
			}
			gettimeofday(&current_time, NULL);
			struct Product *p = queue_remove(q);
			printf("Consumer %d has consumed a product\n", id);
			print_product(*p);
			gettimeofday(&current_time, NULL);
			
			elapsed_time_turn = (current_time.tv_sec - p->birthday.tv_sec) * 1000.0 + (current_time.tv_usec - p->birthday.tv_usec) / 1000.0;
			elapsed_time_wait = (current_time.tv_sec - p->insert_time.tv_sec) * 1000.0 + (current_time.tv_usec - p->insert_time.tv_usec) / 1000.0;
				
			avg_turn_around += elapsed_time_turn;
			avg_wait += elapsed_time_wait;
			
			if(elapsed_time_turn > max_turn_around){
				max_turn_around = elapsed_time_turn;
			}
			if(min_turn_around < 0){
				min_turn_around = elapsed_time_turn;
			}
			if(min_turn_around > elapsed_time_turn){
				min_turn_around = elapsed_time_turn;
			}
			
			if(elapsed_time_wait > max_wait){
				max_wait = elapsed_time_wait;
			}
			if(min_wait < 0){
				min_wait = elapsed_time_wait;
			}
			if(min_wait > elapsed_time_wait){
				min_wait = elapsed_time_wait;
			}
			total_number_consumed += 1;
			pthread_mutex_unlock(q->q_mutex);
			pthread_cond_signal(q->is_not_full); \
			
			usleep(100000);//Sleep for 100 miliseconds
		}
	}
	else{
		while(TOTAL_NUMBER_OF_PRODUCTS > total_number_consumed){
			
			pthread_mutex_lock(q->q_mutex);//locks the mutex
			
			if(TOTAL_NUMBER_OF_PRODUCTS <= total_number_consumed){
				pthread_mutex_unlock(q->q_mutex);
				return(NULL);
			}
			
			//Waits for the queue not to be empty
			while(q->is_empty == 1){
				pthread_cond_wait(q->is_not_empty, q->q_mutex);
			}
			
			struct Product *p = queue_peek(q);
			int i = 0;
			
			if(p->life >= VALUE_OF_QUANTUM){
				//printf("Consumer %d has found a life larger than quantum\n", id);
				//printf("Life: %d, Q: %d\n", p->life, VALUE_OF_QUANTUM);
				p->life -= VALUE_OF_QUANTUM;
				for(i = 0; i < VALUE_OF_QUANTUM; ++i){
					fn(10);
				}
				//printf("Consumer %d Done with the value of quantum\n", id);
			}
			else{
				p = queue_remove(q);
				
				for(i = 0; i < p->life; ++i){
					fn(10);
				}
				printf("Consumer %d has consumed a product\n", id);
				print_product(*p);
				gettimeofday(&current_time, NULL);
			
				elapsed_time_turn = (current_time.tv_sec - p->birthday.tv_sec) * 1000.0 + (current_time.tv_usec - p->birthday.tv_usec) / 1000.0;
				elapsed_time_wait = (current_time.tv_sec - p->insert_time.tv_sec) * 1000.0 + (current_time.tv_usec - p->insert_time.tv_usec) / 1000.0;
				
				avg_turn_around += elapsed_time_turn;
				avg_wait += elapsed_time_wait;
				
				if(elapsed_time_turn > max_turn_around){
					max_turn_around = elapsed_time_turn;
				}
				if(min_turn_around < 0){
					min_turn_around = elapsed_time_turn;
				}
				if(min_turn_around > elapsed_time_turn){
					min_turn_around = elapsed_time_turn;
				}
				
				if(elapsed_time_wait > max_wait){
					max_wait = elapsed_time_wait;
				}
				if(min_wait < 0){
					min_wait = elapsed_time_wait;
				}
				if(min_wait > elapsed_time_wait){
					min_wait = elapsed_time_wait;
				}
				total_number_consumed += 1;
			}
			pthread_mutex_unlock(q->q_mutex);
			pthread_cond_signal(q->is_not_full); \
			
			usleep(100000);//Sleep for 100 miliseconds
		}
	}
	
	return (NULL);
	
}

//Prints a product

/*Prints all products, used for debugging/*
void print_all_products(struct Product prods[], int size){
	
	int i = 0;
	for(i = 0; i < size; ++i){
		printf("Product_id: %d\n", prods[i].product_id);
		printf("Birthday: %d\n", prods[i].birthday);
		printf("Life: %d\n\n", prods[i].life);
	}
	
}
*/

int main(int argc, char** argv){
	
	/*
	argv[1] = Number of producer threads
	argv[2] = Number of consumer threads
	argv[3] = Total number of products to be generated by producers
	argv[4] = Size of the queue in which producers and consumers produce and consume (0 = infinite)
	argv[5] = 0 = First-come-first-serve, 1 = Round-Robin
	argv[6] = Value of quantum for round-robin
	argv[7] = Seed for RNG
	
	Conversion from string to int: int = atoi(string)
	*/
	struct timeval start_time;
	struct timeval end_time;
	gettimeofday(&start_time, NULL);//start timing the program
	
	int i = 0; //A loop variable, used several times
	
	//Handles the arguemnts passed to the program
	NUMBER_OF_PRODUCERS = atoi(argv[1]);
	NUMBER_OF_CONSUMERS = atoi(argv[2]);
	TOTAL_NUMBER_OF_PRODUCTS = atoi(argv[3]);
	SIZE_OF_QUEUE = atoi(argv[4]);
	SCHEDULE_CHOICE = atoi(argv[5]);
	VALUE_OF_QUANTUM = atoi(argv[6]);
	RNG_SEED = atoi(argv[7]);
	
	printf("The number of producers is %d\n", NUMBER_OF_PRODUCERS);
	printf("The number of consumers is %d\n", NUMBER_OF_CONSUMERS);
	printf("The total number of products is %d\n", TOTAL_NUMBER_OF_PRODUCTS);
	printf("The size of the queue is %d\n", SIZE_OF_QUEUE);
	printf("The choice of schedule is %d\n", SCHEDULE_CHOICE);
	printf("The value of quantum is %d\n", VALUE_OF_QUANTUM);
	printf("The seed for RNG is %d\n\n", RNG_SEED);
	
	srand(RNG_SEED); //Seeds the random number generator with the specified value
	the_queue = init_queue(SIZE_OF_QUEUE, SCHEDULE_CHOICE);//Initializes the queue
	//Creates and executes the pthreads
	pthread_t producers[NUMBER_OF_PRODUCERS];
	pthread_t consumers[NUMBER_OF_CONSUMERS];
	
	int ids[NUMBER_OF_CONSUMERS > NUMBER_OF_PRODUCERS ? NUMBER_OF_CONSUMERS : NUMBER_OF_PRODUCERS];
	
	for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
		ids[i] = i;
		pthread_create(&producers[i], NULL, producer, (void *)(ids + i));
	}
	for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
		ids[i] = i;
		pthread_create(&consumers[i], NULL, consumer, (void *)(ids + i));
	}
	//Joins the threads based on the number of one type of thread compared to another
	//pthread_cancel is required here, because otherwise the threads wait for
	//signals that never come
	if(NUMBER_OF_PRODUCERS > NUMBER_OF_CONSUMERS){
		
		for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
			pthread_join(consumers[i], NULL);
			//printf("\t\tCONSUMER THREAD % JOINED\n", i);
		}
		for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
			pthread_cancel(producers[i]);
			//printf("\t\tPRODUCER THREAD % JOINED\n", i);
		}
		
	}
	else if(NUMBER_OF_PRODUCERS < NUMBER_OF_CONSUMERS){
		for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
			pthread_join(producers[i], NULL);
			//printf("\t\tPRODUCER THREAD % JOINED\n", i);
		}
		while(total_number_consumed < TOTAL_NUMBER_OF_PRODUCTS){
			sleep(1);
		}
		for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
			pthread_cancel(consumers[i]);
			//printf("\t\tCONSUMER THREAD % JOINED\n", i);
		}
	}
	else{
		for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
			pthread_join(producers[i], NULL);
			//printf("\t\tPRODUCER THREAD % JOINED\n", i);
		}
		for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
			pthread_join(consumers[i], NULL);
			//printf("\t\tCONSUMER THREAD % JOINED\n", i);
		}
	}
	destroy_queue(the_queue);
	
	gettimeofday(&end_time, NULL);
	
	double time_consumed = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_usec - start_time.tv_usec) / 1000.0;
	
	printf("Done with program. Entire process took %f ms\n", time_consumed);
	avg_turn_around = avg_turn_around/TOTAL_NUMBER_OF_PRODUCTS;
	avg_wait = avg_wait / TOTAL_NUMBER_OF_PRODUCTS;
	printf("Min wait time is %f ms\n", min_wait);
	printf("Max wait time is %f ms\n", max_wait);
	printf("Avg wait time is %f ms\n", avg_wait);
	
	printf("Min turnaround time is %f ms\n", min_turn_around);
	printf("Max turnaround time is %f ms\n", max_turn_around);
	printf("Avg turnaround time is %f ms\n", avg_turn_around);
	
	//printf("Producer throughput is %f products per second\n", (double)TOTAL_NUMBER_OF_PRODUCTS / (time_consumed/60000.0));
	//printf("Consumer throughput is %f products per second\n",);
	
	return 0;
}

