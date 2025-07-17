#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define COOK_COUNT 2
#define CASHIER_COUNT 2
#define CUSTOMER_COUNT 5
#define RACK_SIZE 5
#define WAIT_TIME 3

typedef struct {
    int id;
    sem_t *order;
    sem_t *food_ready;
} cashier_t;

typedef struct {
    int id;
    sem_t *init_done;
} thread_arg_t;

bool stop_simulation = false;
sem_t rack_mutex, cook_sem, cashier_sem, cashier_wake, customer_sem, customer_mutex;
cashier_t shared_cashier_data;
int rack_count = 0;

void check_rack_state() {
    if (rack_count < 0 || rack_count > RACK_SIZE) {
        printf("Rack Error: Invalid count %d\n", rack_count);
        exit(1);
    }
}

void *cook_thread(void *arg) {
    thread_arg_t *data = (thread_arg_t *)arg;
    int id = data->id;
    printf("Cook %d started\n", id);
    sem_post(data->init_done);

    while (!stop_simulation) {
        sem_wait(&cook_sem);
        if (stop_simulation) break;

        sleep(rand() % WAIT_TIME);
        sem_wait(&rack_mutex);
        rack_count++;
        check_rack_state();
        sem_post(&rack_mutex);
        printf("Cook %d placed food\n", id);
        sem_post(&cashier_sem);
    }

    printf("Cook %d exiting\n", id);
    return NULL;
}

void *cashier_thread(void *arg) {
    thread_arg_t *data = (thread_arg_t *)arg;
    int id = data->id;
    sem_t order, ready;
    sem_init(&order, 0, 0);
    sem_init(&ready, 0, 0);
    printf("Cashier %d started\n", id);
    sem_post(data->init_done);

    while (!stop_simulation) {
        sem_wait(&customer_sem);
        if (stop_simulation) break;

        shared_cashier_data = (cashier_t){.id = id, .order = &order, .food_ready = &ready};
        sem_post(&cashier_wake);
        sem_wait(&order);

        sleep(rand() % WAIT_TIME);
        sem_wait(&cashier_sem);
        sem_wait(&rack_mutex);
        rack_count--;
        check_rack_state();
        sem_post(&rack_mutex);
        sem_post(&cook_sem);
        sem_post(&ready);
        printf("Cashier %d served food\n", id);
    }

    sem_destroy(&order);
    sem_destroy(&ready);
    printf("Cashier %d exiting\n", id);
    return NULL;
}

void *customer_thread(void *arg) {
    thread_arg_t *data = (thread_arg_t *)arg;
    int id = data->id;

    // Announce customer creation
    printf("Customer %d entered\n", id);

    // Notify main that this thread is ready
    sem_post(data->init_done);

    // Simulate time before customer reaches counter
    sleep(rand() % WAIT_TIME + 1);

    // Lock to ensure only one customer proceeds at a time
    sem_wait(&customer_mutex);

    // Signal that a customer is ready to be served
    sem_post(&customer_sem);

    // Wait until a cashier is ready and has shared their data
    sem_wait(&cashier_wake);

    // Get assigned cashier's shared data (order and response semaphores)
    cashier_t c = shared_cashier_data;

    // Release lock for other customers
    sem_post(&customer_mutex);

    // Inform which cashier this customer is interacting with
    printf("Customer %d ordering from Cashier %d\n", id, c.id);

    // Place the order (signal the cashier's order semaphore)
    sem_post(c.order);

    // Wait for the cashier to prepare and return the food
    sem_wait(c.food_ready);

    // Order is completed
    printf("Customer %d received food from Cashier %d\n", id, c.id);

    return NULL;
}


int main() {
    srand(time(NULL));
    sem_init(&rack_mutex, 0, 1);
    sem_init(&cook_sem, 0, RACK_SIZE);
    sem_init(&cashier_sem, 0, 1);
    sem_init(&cashier_wake, 0, 0);
    sem_init(&customer_sem, 0, 0);
    sem_init(&customer_mutex, 0, 1);

    pthread_t cooks[COOK_COUNT], cashiers[CASHIER_COUNT], customers[CUSTOMER_COUNT];
    sem_t init_done;
    sem_init(&init_done, 0, 0);
    thread_arg_t args = {.init_done = &init_done};

    for (int i = 0; i < COOK_COUNT; i++) {
        args.id = i + 1;
        pthread_create(&cooks[i], NULL, cook_thread, &args);
        sem_wait(&init_done);
    }

    for (int i = 0; i < CASHIER_COUNT; i++) {
        args.id = i + 1;
        pthread_create(&cashiers[i], NULL, cashier_thread, &args);
        sem_wait(&init_done);
    }

    for (int i = 0; i < CUSTOMER_COUNT; i++) {
        args.id = i + 1;
        pthread_create(&customers[i], NULL, customer_thread, &args);
        sem_wait(&init_done);
    }

    for (int i = 0; i < CUSTOMER_COUNT; i++) pthread_join(customers[i], NULL);
    printf("All customers served.\n");

    stop_simulation = true;
    for (int i = 0; i < COOK_COUNT; i++) sem_post(&cook_sem);
    for (int i = 0; i < CASHIER_COUNT; i++) sem_post(&customer_sem);

    for (int i = 0; i < COOK_COUNT; i++) pthread_join(cooks[i], NULL);
    for (int i = 0; i < CASHIER_COUNT; i++) pthread_join(cashiers[i], NULL);

    printf("Simulation ended.\n");
    return 0;
}
