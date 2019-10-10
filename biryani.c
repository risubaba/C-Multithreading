#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
struct robot *Robots;
struct table *Tables;
struct student *Students;
int n_of_robots, n_of_tables, n_of_students;
int readyqueue[10000];
int readypointer = 0;
int sizeofqueue = 0;
//rand syntax is rand() + (high-low+1) + low
//srand() should not be called multiple times in a single program
//usleep(20) to reduce delay due to compiler parallelization/optimization ??
struct robot
{
    int time_to_prepare;
    int vessels_prepared;
    int capacity_of_vessel;
    pthread_t thread_id;
    char mode;
};

struct table
{
    int current_capacity;
    int current_slots;
    pthread_t thread_id;
    char mode;
};

struct student
{
    pthread_t thread_id;
    int served;
    char mode;
};

struct id_number
{
    int id;
};

int min(int a, int b)
{
    return a < b ? a : b;
}

struct robot *shareMemRobot(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct robot *)shmat(shm_id, NULL, 0);
}

struct table *shareMemTable(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct table *)shmat(shm_id, NULL, 0);
}

struct student *shareMemStudent(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (struct student *)shmat(shm_id, NULL, 0);
}

void biryani_ready(int id_number)
{
    while (Robots[id_number].vessels_prepared > 0)
    {
        ;
    }
    printf("Biryani finished for Robot %d\n", id_number);
    fflush(stdout);
}

void *Robot_prepare_food(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    while (1)
    {
        Robots[id_number].time_to_prepare = rand() % (5 - 2 + 1) + 2;
        sleep(Robots[id_number].time_to_prepare);
        Robots[id_number].vessels_prepared = rand() % (10 - 1 + 1) + 1;
        Robots[id_number].capacity_of_vessel = rand() % (50 - 25 + 1) + 25;
        printf("Robot %d prepared %d vessels,each with capacity %d\n", id_number, Robots[id_number].vessels_prepared, Robots[id_number].capacity_of_vessel);
        fflush(stdout);
        biryani_ready(id_number);
    }
    return NULL;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void ready_to_serve_table(int number_of_slots, int id_number)
{
    readyqueue[sizeofqueue] = id_number;
    int curr = sizeofqueue;
    while (Tables[readyqueue[curr]].current_slots != number_of_slots)
    {
        ;
    }
    readyqueue[curr] = -1;
}

void *Table_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    // printf("Table number %d\n", id_number);
    if (Tables[id_number].current_capacity == 0)
    {
        for (int i = 1; i <= n_of_robots; i = (i + 1) % (n_of_robots + 1) + (i == n_of_robots))
        {
            // printf("Going through i == %d\n",i);
            // if (Robots[i].vessels_prepared > 0)
            pthread_mutex_lock(&mutex2);
            if (Robots[i].vessels_prepared > 0 && Tables[id_number].current_capacity == 0)
            {
                Tables[id_number].current_capacity = Robots[i].capacity_of_vessel;
                // pthread_mutex_lock(&mutex);
                // sizeofqueue++;
                // pthread_mutex_unlock(&mutex);
                readyqueue[sizeofqueue] = id_number;
                sizeofqueue++;
                // pthread_mutex_lock(&mutex);
                printf("Table %d was refilled by robot %d and has %d servings left\n", id_number, i, Robots[i].capacity_of_vessel);
                fflush(stdout);
                Robots[i].vessels_prepared--;
                // ready_to_serve_table(number_of_slots,id_number);
            }
            pthread_mutex_unlock(&mutex2);
            if (Tables[id_number].current_capacity > 0)
            {
                int number_of_slots = min(Tables[readyqueue[readypointer]].current_capacity, min(rand() % (10 - 1 + 1) + 1, n_of_students));
                Tables[readyqueue[readypointer]].current_slots = 0;
                ready_to_serve_table(number_of_slots, id_number);
                Tables[readyqueue[readypointer]].current_capacity -= number_of_slots;
            }
        }
    }
    return NULL;
}

void student_in_slot(int index)
{
    Tables[readyqueue[index]].current_slots++;
    usleep(30);
}

void wait_for_slot(int id_number)
{
    int i = 0;
    while (1)
    {
        for (; i < sizeofqueue;)
        {
            printf("Value of i is %d when sizeofqueue is %d\n", i, sizeofqueue);
            fflush(stdout);
            if (readyqueue[i] == -1)
            {
                i = rand() % (sizeofqueue);
                break;
            }
            printf("Student %d served at table %d\n", id_number, readyqueue[i]);
            fflush(stdout);
            student_in_slot(i);
            return;
        }
    }
}

void *Student_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    // printf("Entered wait for student %d\n", id_number);
    wait_for_slot(id_number);
    return NULL;
}

int main()
{
    srand(time(NULL));
    for (int i=0;i<100;i++)
    printf("%d\n",rand()%50);
    printf("Enter number of robots / tables / students\n");
    scanf("%d %d %d", &n_of_robots, &n_of_tables, &n_of_students);
    Robots = shareMemRobot((n_of_robots + 1) * sizeof(struct robot));
    Tables = shareMemTable((n_of_tables + 1) * sizeof(struct table));
    Students = shareMemStudent((n_of_students + 1) * sizeof(struct student));
    for (int i = 1; i <= n_of_tables; i++)
        Tables[i].current_capacity = 0;
    for (int i = 0; i < 10000; i++)
        readyqueue[i] = -1;
    for (int i = 1; i <= n_of_robots;)
    {
        struct id_number temp;
        temp.id = i;
        pthread_create(&Robots[i].thread_id, NULL, (void *)Robot_prepare_food, &temp);
        i++;
        usleep(20);
    }
    for (int i = 1; i <= n_of_tables;)
    {
        struct id_number temp;
        temp.id = i;
        pthread_create(&Tables[i].thread_id, NULL, (void *)Table_loop, &temp);
        i++;
        usleep(20);
    }
    // fflush(stdout);

    // for (int i = readypointer; i < sizeofqueue; i++)
    //     printf("Table %d has %d servings", readyqueue[i], Tables[readyqueue[i]].current_capacity);
    // while (n_of_students > 0)
    // {
    //     if (readypointer < sizeofqueue)
    //     {
    //         // printf("+++++++++++++++++++++ %d\n", Tables[readyqueue[readypointer]].current_capacity);
    //         int student_to_serve = min(Tables[readyqueue[readypointer]].current_capacity, min(rand() % (10 - 1 + 1) + 1, n_of_students));
    //         // printf("Serving %d students at Table number %d\n", student_to_serve, readyqueue[readypointer]);
    //         n_of_students -= student_to_serve;
    //         Tables[readyqueue[readypointer]].current_capacity -= student_to_serve;
    //         if (Tables[readyqueue[readypointer]].current_capacity == 0)
    //         {
    //             readypointer++;
    //         }
    //     }
    // }
    srand(time(NULL));
    for (int iiii = 1; iiii <= n_of_students;)
    {
        struct id_number temp;
        temp.id = iiii;
        pthread_create(&Students[iiii].thread_id, NULL, (void *)Student_loop, &temp);
        iiii++;
        usleep(30000);
    }
    // while (1)
    // ;
    for (int i = 1; i <= n_of_students; i++)
    {
        pthread_join(Students[i].thread_id, NULL);
        printf("Student %d finished eating\n", i);
        fflush(stdout);
    }
}