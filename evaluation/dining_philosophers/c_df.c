#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef struct 
{
    int fork1;
    int fork2;
    char name[];
} philosopher_params;

pthread_t start_philosopher(char *, int, int);
void philosopher_thread(void *);
void acquire_fork(char *, int);
void replace_fork(char *, int);
void eat(char *);
void think(char *);
void philo_wait(void);

pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;
int table[5];

int main()
{
    srand(time(NULL));

    for (int i = 0; i < 5; i++)
    {
        table[i] = 1;
    }

    start_philosopher("Marx", 1, 2);
    start_philosopher("Descartes", 2, 3);
    start_philosopher("Nietzsche", 3, 4);
    start_philosopher("Aristotle", 4, 5);
    pthread_t last = start_philosopher("Kant", 1, 5);
    
    pthread_join(last, NULL);

    return 0;
}

pthread_t start_philosopher(char *name, int fork1, int fork2)
{
    pthread_t thread;
    philosopher_params *params = malloc(sizeof(philosopher_params) + (strlen(name) + 1) * sizeof(char));

    params->fork1 = fork1;
    params->fork2 = fork2;
    strcpy(params->name, name);

    pthread_create(&thread, NULL, (void *)&philosopher_thread, (void *)params);

    return thread;
}

void philosopher_thread(void *params_)
{
    philosopher_params *params = (philosopher_params *)params_;

    while (1)
    {
        think(params->name);
        acquire_fork(params->name, params->fork1);
        acquire_fork(params->name, params->fork2);
        eat(params->name);
        replace_fork(params->name, params->fork2);
        replace_fork(params->name, params->fork1);
    }
}

void acquire_fork(char *name, int fork)
{
    while (1)
    {
        pthread_mutex_lock(&table_mutex);
        if (table[fork - 1] == 1)
        {
            table[fork - 1] = 0;
            printf("%s has acquired fork %d.\n", name, fork);
            pthread_mutex_unlock(&table_mutex);
            return;
        }
        pthread_mutex_unlock(&table_mutex);
    }
}

void replace_fork(char *name, int fork)
{
    printf("%s is replacing fork %d.\n", name, fork);

    pthread_mutex_lock(&table_mutex);
    table[fork - 1] = 1;
    pthread_mutex_unlock(&table_mutex);
}

void eat(char *name)
{
    printf("%s is eating...\n", name);
    philo_wait();
}

void think(char *name)
{
    printf("%s is thinking...\n", name);
    philo_wait();
}

void philo_wait()
{
    sleep(rand() % 2 + 1);
}
