#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#define BUSY 1
#define FREE 0
#define DARK_GREEN "\x1B[1;32m"
#define RESET "\x1B[0m"
#define EMPTY_CAB 0
#define PREMIER 1
#define POOL 2
#define SINGLE 1
#define DOUBLE 2
#define WAITING 0
#define IN_CAB 1

pthread_mutex_t mutex;
sem_t riderPay;
int n_of_cabs, n_of_riders, n_of_servers;
struct server *Servers;
struct cab *Cabs;
struct rider *Riders;

struct server
{
    int id_number;
    int status;
    int rider_id;
    pthread_t thread_id;
};

struct cab
{
    int type;
    int status;
};

struct rider
{
    int id_number;
    int cab;
    int maxWaitTime;
    int RideTime;
    int cab_no;
    int status;
    sem_t payment;
    pthread_t thread_id;
};

struct rider *shareMemRider(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct rider *)shmat(shm_id, NULL, 0);
}

struct cab *shareMemCab(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct cab *)shmat(shm_id, NULL, 0);
}

struct server *shareMemServer(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct server *)shmat(shm_id, NULL, 0);
}
pthread_mutex_t Rider_mutex[10005];
pthread_mutex_t Cab_mutex[10005];

void *rider_thread(void *args);
void *server_thread(void *args);

int main()
{
    printf("Enter number of cabs, riders, and payment servers:\n");
    scanf("%d %d %d", &n_of_cabs, &n_of_riders, &n_of_servers);
    Riders = shareMemRider((n_of_riders + 1) * sizeof(struct rider));
    Cabs = shareMemCab((n_of_cabs + 1) * sizeof(struct cab));
    Servers = shareMemServer((n_of_servers + 1) * sizeof(struct server));

    srand(time(0));
    sem_init(&riderPay, 0, 0);
    for (int i = 0; i < 10005; i++)
    {
        pthread_mutex_init(&Rider_mutex[i], NULL), pthread_mutex_init(&Cab_mutex[i], NULL);
    }
    for (int i = 1; i <= n_of_cabs; i++)
    {
        Cabs[i].type = EMPTY_CAB;
        Cabs[i].status = EMPTY_CAB;
    }

    for (int i = 1; i <= n_of_riders; i++)
    {
        Riders[i].id_number = i;
        Riders[i].maxWaitTime = rand() % 5 + 1;
        Riders[i].RideTime = rand() % 5 + 1;
        Riders[i].cab = rand() % 2 + 1;
        sem_init(&(Riders[i].payment), 0, 0);
        Riders[i].status = WAITING;
    }
    for (int i = 1; i <= n_of_riders; i++)
        pthread_create(&(Riders[i].thread_id), NULL, rider_thread, &Riders[i]);
    for (int i = 1; i <= n_of_servers; i++)
    {
        Servers[i].status = FREE;
        Servers[i].id_number = i;
        Servers[i].rider_id = 0;
    }

    for (int i = 1; i <= n_of_servers; i++)
        pthread_create(&(Servers[i].thread_id), NULL, server_thread, &Servers[i]);
    for (int i = 1; i <= n_of_riders; i++)
        pthread_join(Riders[i].thread_id, 0);

    return 0;
}

void *rider_thread(void *args)
{
    struct rider *Rider = (struct rider *)args;
    int arrival = rand() % 3;
    sleep(arrival);
    printf("\n\n\x1B[33;1mRider %d \x1B[0mhas arrived with details:\n\x1B[33;1mArrival time - %d\nCab type - %d\nMax Wait Time %d\nRide Time %d\n\n\x1B[0m", Rider->id_number, arrival, Rider->cab, Rider->maxWaitTime, Rider->RideTime);
    int got_ride = 0, time_exceed = 0;

    clock_t t = clock();

    while (!got_ride)
    {
        clock_t time_taken = clock() - t;
        double times = ((double)time_taken) / CLOCKS_PER_SEC;
        if (times > ((double)Rider->maxWaitTime))
        {
            time_exceed = 1;
            break;
        }

        int flag = 0;
        for (int i = 1; i <= n_of_cabs; i++)
        {
            pthread_mutex_lock(&(Cab_mutex[i]));

            if (Rider->cab == 1)
            {
                if (Cabs[i].type == 0)
                {
                    //printf("1");
                    Cabs[i].type = 1;
                    got_ride = 1;
                    Rider->cab_no = i + 1;
                    pthread_mutex_unlock(&(Cab_mutex[i]));
                    break;
                }
            }

            else if (Rider->cab == 2)
            {
                if (Cabs[i].type == 2 && Cabs[i].status == 1)
                {
                    //printf("2");
                    Cabs[i].status = 2;
                    flag = 1;
                    got_ride = 1;
                    Rider->cab_no = i;
                    pthread_mutex_unlock(&(Cab_mutex[i]));
                    break;
                }
            }

            pthread_mutex_unlock(&(Cab_mutex[i]));
        }

        if (Rider->cab == 2 && flag == 0)
        {
            for (int i = 0; i < n_of_cabs; i++)
            {
                pthread_mutex_lock(&(Cab_mutex[i]));

                if (Cabs[i].type == 0)
                {
                    Cabs[i].type = 2;
                    Cabs[i].status = 1;
                    got_ride = 1;
                    Rider->cab_no = i;
                    pthread_mutex_unlock(&(Cab_mutex[i]));
                    break;
                }

                pthread_mutex_unlock(&(Cab_mutex[i]));
            }
        }
    }

    if (time_exceed == 1)
    {
        printf("\e[31;1mTIMEOUT FOR RIDER %d!\x1B[0m\n", Rider->id_number);
        return NULL;
    }

    if (Rider->cab == 1)
        printf("\x1B[1;34mRIDER %d HAS BOARDED PREMIER CAB  %d\x1B[0m\n", Rider->id_number, Rider->cab_no);

    else
        printf("\x1B[1;34mRIDER %d HAS BOARDED POOL CAB %d\x1B[0m\n", Rider->id_number, Rider->cab_no);

    sleep(Rider->RideTime);

    int no = Rider->cab_no;
    pthread_mutex_lock(&(Cab_mutex[no]));
    if (Rider->cab == 2)
    {
        if (Cabs[no].status == 1)
        {
            Cabs[no].type = 0;
            Cabs[no].status = 0;
        }

        else
            Cabs[no].status = 1;
    }

    else
        Cabs[no].type = 0;
    printf("\x1B[35;3mRIDER %d FINISHED JOURNEY WITH CAB %d!\x1B[0m\n", Rider->id_number, Rider->cab_no);
    Rider->status = 1;
    sem_post(&riderPay);
    pthread_mutex_unlock(&(Cab_mutex[no]));

    sem_wait(&(Rider->payment));

    return NULL;
}

void *server_thread(void *args)
{
    struct server *temp_Server = (struct server *)args;
    while (1)
    {
        sem_wait(&riderPay);
        temp_Server->status = BUSY;
        for (int i = 1; i <= n_of_riders; i++)
        {
            pthread_mutex_lock(&(Rider_mutex[i]));
            if (Riders[i].status == 1)
            {
                temp_Server->rider_id = i;
                printf("\x1B[32mRIDER %d PAYING ON SERVER %d\x1B[0m\n", i, temp_Server->id_number);
                Riders[i].status = 0;
                pthread_mutex_unlock(&(Rider_mutex[i]));
                break;
            }

            pthread_mutex_unlock(&(Rider_mutex[i]));
        }

        if (temp_Server->rider_id)
        {
            sleep(2);
            printf("\x1B[1;32mRIDER %d DONE!\x1B[0m\n", temp_Server->rider_id);
            sem_post(&(Riders[temp_Server->rider_id].payment));
            temp_Server->rider_id = 0;
            temp_Server->status = FREE;
        }
    }

    return NULL;
}