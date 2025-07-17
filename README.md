# burger-shop-simulation
void *customer_thread(void *arg) {
    thread_arg_t *data = (thread_arg_t *)arg;
    int id = data->id;
    printf("Customer %d entered\n", id);
    sem_post(data->init_done);
    sleep(rand() % WAIT_TIME + 1);  // Simulate arrival time

    sem_wait(&customer_mutex);
    sem_post(&customer_sem);
    sem_wait(&cashier_wake);

    cashier_t c = shared_cashier_data;
    sem_post(&customer_mutex);

    printf("Customer %d ordering from Cashier %d\n", id, c.id);


    sem_post(c.order);              // Place order
    sem_wait(c.food_ready);         // Wait for food
    printf("Customer %d received food from Cashier %d\n", id, c.id);


    return NULL;
}
