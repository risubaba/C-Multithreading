#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

typedef struct Cab Cab;
typedef struct Rider Rider;
typedef struct Server Server;
#define CAB_COLOR "\x1B[31m"
#define TABLE_COLOR "\x1B[32m"
#define RIDER_COLOR "\x1B[34m"
#define RESET "\x1B[0m"
#define EMPTY_CAB 0
#define PREMIER 1
#define POOL 2
#define SINGLE_RIDER 1
#define DOUBLE_RIDER 2
#define WAITING 0
#define WAITING_FOR_PAYMENT 1
#define NOT_WAITING 1
#define NOT_WAITING_FOR_PAYMENT 0
#define IN_CAB 1
#define BUSY 1
#define FREE 0

sem_t riderPay;
pthread_mutex_t mutex;

int n_of_cabs, n_of_riders, n_of_servers;
struct Cab
{
    int type;
    int occupancy;
};
Cab Cabs[1000];
pthread_mutex_t CabMutex[1000];
struct Rider
{
    int arrivalTime;
    int cabType;
    int maxWaitTime;
    int RideTime;
    int cab;
    int status;
    sem_t payment;
    pthread_t thread_id;
};
Rider riders[1000];
pthread_mutex_t RiderMutex[1000];
struct Server
{
    int status;
    int rider;
    pthread_t thread_id;
};
Server servers[1000];

int randomm(int min, int max)
{
    return min + rand() % (max - min + 1);
}

void *rider_thread(void *args)
{
    int id_number = *(int *)args;
    sleep(riders[id_number].arrivalTime);
    riders[id_number].maxWaitTime = randomm(2, 5);
    riders[id_number].cabType = randomm(1, 2);
    riders[id_number].RideTime = randomm(2, 5);
    printf("\n\nRider %d has arrived\n\t at time - %d\n\t with max wait time - %d,\n\t ride time - %d\n\t and cab type - %s\n\n", id_number, riders[id_number].arrivalTime, riders[id_number].maxWaitTime, riders[id_number].RideTime, (riders[id_number].cabType == PREMIER) ? "PREMIER" : "POOL");

    time_t starttime = time(NULL);
    int prev = time(NULL);

    while (riders[id_number].status == WAITING)
    {
        int currtime = time(NULL);
        if ((currtime - starttime) > riders[id_number].maxWaitTime)
        {
            printf(RIDER_COLOR "Time exceeded max wait time for rider %d!\n" RESET, id_number);
            return NULL;
        }

        if (riders[id_number].cabType == PREMIER)
        {
            for (int i = 1; i <= n_of_cabs; i++)
            {
                pthread_mutex_lock(&(CabMutex[i]));
                if (Cabs[i].type == EMPTY_CAB)
                {
                    Cabs[i].type = PREMIER;
                    Cabs[i].occupancy = SINGLE_RIDER;
                    riders[i].status = NOT_WAITING;
                    riders[id_number].cab = i;
                    pthread_mutex_unlock(&(CabMutex[i]));
                    break;
                }
                pthread_mutex_unlock(&(CabMutex[i]));
            }
        }
        else if (riders[id_number].cabType == POOL)
        {
            for (int i = 1; i <= n_of_cabs; i++)
            {
                pthread_mutex_lock(&(CabMutex[i]));
                if (Cabs[i].type == POOL && Cabs[i].occupancy == SINGLE_RIDER)
                {
                    Cabs[i].occupancy = DOUBLE_RIDER;
                    riders[i].status = NOT_WAITING;
                    riders[id_number].cab = i;
                    pthread_mutex_unlock(&(CabMutex[i]));
                    break;
                }
                pthread_mutex_unlock(&(CabMutex[i]));
            }
        }
        else
        {
            for (int i = 1; i <= n_of_cabs; i++)
            {
                pthread_mutex_lock(&(CabMutex[i]));
                if (Cabs[i].type == EMPTY_CAB)
                {
                    Cabs[i].type = POOL;
                    Cabs[i].occupancy = SINGLE_RIDER;
                    riders[i].status = NOT_WAITING;
                    riders[id_number].cab = i;
                    pthread_mutex_unlock(&(CabMutex[i]));
                    break;
                }
                pthread_mutex_unlock(&(CabMutex[i]));
            }
        }
    }

    if (riders[id_number].cabType == PREMIER)
        printf(RIDER_COLOR "Rider %d has boarded premier cab with number %d\n" RESET, id_number, riders[id_number].cab);
    else
        printf(RIDER_COLOR "Rider %d has boarded pool cab with number  %d\n" RESET, id_number, riders[id_number].cab);

    sleep(riders[id_number].RideTime);
    int cab_used = riders[id_number].cab;
    pthread_mutex_lock(&(CabMutex[cab_used]));
    if (riders[id_number].cabType == PREMIER)
    {
        Cabs[cab_used].type = EMPTY_CAB;
        Cabs[cab_used].occupancy = EMPTY_CAB;
    }
    else
    {
        if (Cabs[cab_used].occupancy == DOUBLE_RIDER)
        {
            Cabs[cab_used].occupancy = SINGLE_RIDER;
        }
        else
        {
            Cabs[cab_used].type = EMPTY_CAB;
            Cabs[cab_used].occupancy = EMPTY_CAB;
        }
    }

    printf(RIDER_COLOR "Rider %d finished with cab number %d\n", id_number, riders[id_number].cab);
    riders[id_number].status = NOT_WAITING;
    sem_post(&riderPay);
    pthread_mutex_unlock(&(CabMutex[cab_used]));
    sem_wait(&(riders[id_number].payment));

    return NULL;
}

void update_server(int id_number, int rider)
{
    servers[id_number].rider = rider;
    servers[id_number].status = NOT_WAITING_FOR_PAYMENT;
}

void *server_thread(void *args)
{
    int id_number = *(int *)args;
    while (1)
    {
        sem_wait(&riderPay);
        servers[id_number].status = BUSY;
        for (int i = 1; i <= n_of_riders; i++)
        {
            pthread_mutex_lock(&(RiderMutex[i]));
            if (riders[i].status == WAITING_FOR_PAYMENT)
            {
                printf(RIDER_COLOR "RIder %d making payment on server %d\n" RESET, i, id_number);
                update_server(id_number, i);
                pthread_mutex_unlock(&(RiderMutex[i]));
                break;
            }
            pthread_mutex_unlock(&(RiderMutex[i]));
        }
        if (servers[id_number].rider > 0)
        {
            sleep(2);
            sem_post(&(riders[servers[id_number].rider].payment));
            printf(RIDER_COLOR "Thread associated with Rider %d has finished\n" RESET, servers[id_number].rider);
            update_server(id_number, -1);
        }
    }
    return NULL;
}

void *Server_Init(void *args)
{
    for (int i = 1; i <= n_of_servers; i++)
    {
        servers[i].status = FREE;
        servers[i].rider = -1;
        pthread_create(&(servers[i].thread_id), NULL, server_thread, &i), usleep(50);
    }
    return NULL;
}

void *Rider_Init(void *args)
{
    for (int i = 1; i <= n_of_riders; i++)
    {
        riders[i].arrivalTime = randomm(2, 5);
        sem_init(&(riders[i].payment), 0, 0);
        riders[i].status = WAITING;
    }
    return NULL;
}

void Cab_Init()
{
    for (int i = 0; i < n_of_cabs; i++)
    {
        Cabs[i].type = 0;
        Cabs[i].occupancy = 0;
    }
}

int main()
{
    printf("Enter number of cabs / riders / payment servers\n");
    scanf("%d %d %d", &n_of_cabs, &n_of_riders, &n_of_servers);
    srand(time(NULL));
    sem_init(&riderPay, 0, 0);

    for (int i = 1; i < 1000; i++)
    {
        pthread_mutex_init(&CabMutex[i], NULL);
        pthread_mutex_init(&RiderMutex[i], NULL);
    }

    pthread_t Rider_Init_tid;
    pthread_create(&Rider_Init_tid, NULL, Rider_Init, NULL);
    usleep(50);
        
    pthread_t Server_Init_tid;
    pthread_create(&Server_Init_tid, NULL, Server_Init, NULL);
    usleep(50);

    for (int i = 1; i <= n_of_riders; i++)
        pthread_create(&(riders[i].thread_id), NULL, rider_thread, &i), usleep(50);

    for (int i = 1; i <= n_of_riders; i++)
        pthread_join(riders[i].thread_id, 0);
    printf("SIMULATION OVER\n");

    return 0;
}
