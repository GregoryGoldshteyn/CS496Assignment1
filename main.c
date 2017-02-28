#include <sys/time.h>
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

//Shared varibales that are initiated by the program inputs
int NUMBER_OF_PRODUCERS;
int NUMBER_OF_CONSUMERS;
int TOTAL_NUMBER_OF_PRODUCTS;
int SIZE_OF_QUEUE;
int SCHEDULE_CHOICE;
int VALUE_OF_QUANTUM;
int RNG_SEED;

//A struct for representing products
//clock_t is used for simplicity
struct Product{
	
	int product_id;
	struct timeval birthday;
	int life;
	
};

void print_product(struct Product p){
	
	printf("The product id is %d\n", p.product_id);
	printf("The product birthday is %lu\n", p.birthday.tv_usec);
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
	struct Product buffer[];
	
};

//Function to initialize the queue
struct Product_queue *init_queue(int size){
	
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
struct Product queue_remove(struct Product_queue *q){
	
	struct Product ret_product = q->buffer[q->head];
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

//The producer function, which will be executed by a pthread
void* producer(void *q_ptr){
	
	struct Product_queue *q = (struct Product_queue *)q_ptr;
	
	while(TOTAL_NUMBER_OF_PRODUCTS > total_number_produced){
		
		pthread_mutex_lock(q->q_mutex);//locks the mutex
		
		if(TOTAL_NUMBER_OF_PRODUCTS <= total_number_produced){
			pthread_mutex_unlock(q->q_mutex);
			return(NULL);
		}
		
		//waits for the queue not to be full
		while(q->is_full == 1){
			pthread_cond_wait(q->is_not_full, q->q_mutex);
		}
		
		//Creates a new product
		struct Product p;
		p.product_id = total_number_produced;
		gettimeofday(&p.birthday, NULL);
		p.life = rand() % 1024;
		
		//Adds the product
		queue_add(q, p);
		total_number_produced += 1;
		
		printf("Producer %p has made a product\n", pthread_self());
		print_product(p);
		
		//Unlocks the mutex, signals the queue is not empty
		pthread_mutex_unlock(q->q_mutex);
		pthread_cond_signal(q->is_not_empty);
		usleep(100000);//Sleep for 100 miliseconds
		
	}
	
	return (NULL);
	
}

//The consumer function, which will be executed by a pthread
void* consumer(void *q_ptr){
	
	struct Product_queue *q = (struct Product_queue *)q_ptr;
	
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
		
		struct Product p = queue_remove(q);
		total_number_consumed += 1;
		printf("Consumer %p has consumed a product\n", pthread_self());
		print_product(p);
		
		pthread_mutex_unlock(q->q_mutex);
		pthread_cond_signal(q->is_not_full); \
		
		usleep(100000);//Sleep for 100 miliseconds
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
	
	int i = 0; //A loop variable, used several times
	
	//Handles the arguemnts passed to the program
	NUMBER_OF_PRODUCERS = atoi(argv[1]);
	NUMBER_OF_CONSUMERS = atoi(argv[2]);
	TOTAL_NUMBER_OF_PRODUCTS = atoi(argv[3]);
	SIZE_OF_QUEUE = atoi(argv[4]);
	SCHEDULE_CHOICE = atoi(argv[5]);
	VALUE_OF_QUANTUM = atoi(argv[6]);
	RNG_SEED = atoi(argv[7]);
	
	srand(RNG_SEED); //Seeds the random number generator with the specified value
	struct Product_queue *the_queue = init_queue(SIZE_OF_QUEUE);//Initializes the queue
	/* Code used to test the queue before we stated threading
	struct Product_queue *the_queue = init_queue(SIZE_OF_QUEUE);
	for(i = 0; i < 10; ++i){
		
		struct Product p;
		p.product_id = i;
		gettimeofday(&p.birthday, NULL);
		p.life = rand() % 1024;
		
		print_product(p);
		
		queue_add(the_queue, p);
		usleep(1000000);
	}
	
	for(i = 0; i < 10; ++i){
		struct Product p = queue_remove(the_queue);
		print_product(p);
	}
	*/
	
	pthread_t producers[NUMBER_OF_PRODUCERS];
	pthread_t consumers[NUMBER_OF_CONSUMERS];
	
	for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
		pthread_create(&producers[i], NULL, producer, the_queue);
	}
	for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
		pthread_create(&consumers[i], NULL, consumer, the_queue);
	}
	
	for(i = 0; i < NUMBER_OF_PRODUCERS; ++i){
		pthread_join(producers[i], NULL);
	}
	for(i = 0; i < NUMBER_OF_CONSUMERS; ++i){
		pthread_join(consumers[i], NULL);
	}
	
	destroy_queue(the_queue);
	
	return 0;
}

