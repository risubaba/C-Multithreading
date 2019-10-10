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

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t szofqueuemutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Robot_mutex[10005];
pthread_mutex_t Table_mutex[10005];

void biryani_ready(int id_number)
{
    while (Robots[id_number].vessels_prepared > 0)
    {
        pthread_mutex_lock(&Robot_mutex[id_number]);
        pthread_mutex_unlock(&Robot_mutex[id_number]);
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
        Robots[id_number].time_to_prepare = rand() % (4) + 2;
        sleep(Robots[id_number].time_to_prepare);
        Robots[id_number].vessels_prepared = rand() % (10) + 1;
        Robots[id_number].capacity_of_vessel = rand() % (26) + 25;
        printf("Robot %d prepared %d vessels,each with capacity %d\n", id_number, Robots[id_number].vessels_prepared, Robots[id_number].capacity_of_vessel);
        fflush(stdout);
        biryani_ready(id_number);
    }
    return NULL;
}

void ready_to_serve_table(int number_of_slots, int id_number)
{
    readyqueue[sizeofqueue - 1] = id_number;
    int curr = sizeofqueue - 1;
    pthread_mutex_unlock(&szofqueuemutex);
    printf("Slots assigned to %d ********* %d in readyqueue at index %d\n", id_number, number_of_slots, curr);
    fflush(stdout);
    while (Tables[readyqueue[curr]].current_slots != number_of_slots)
    {
        // printf("%d\n",Tables[readyqueue[curr]].current_slots);
        // fflush(stdout);
        pthread_mutex_lock(&Table_mutex[id_number]);
        pthread_mutex_unlock(&Table_mutex[id_number]);
    }
    pthread_mutex_lock(&mutex);
    readyqueue[curr] = -1;
    pthread_mutex_unlock(&mutex);
    printf("Table %d finished serving %d slots at index\n", id_number, number_of_slots);
}

void student_in_slot(int index)
{
    pthread_mutex_lock(&Table_mutex[readyqueue[index]]);
    Tables[readyqueue[index]].current_slots++;
    printf("filled slots for table %d are %d at index %d\n", readyqueue[index], Tables[readyqueue[index]].current_slots, index);
    pthread_mutex_unlock(&Table_mutex[readyqueue[index]]);
    usleep(50);
}

void wait_for_slot(int id_number)
{
    int i = 0;
    while (1)
    {
        for (; i < sizeofqueue;)
        {
            // printf("%d\n",i);
            // printf("Value of i is %d when sizeofqueue is %d\n", i, sizeofqueue);
            fflush(stdout);
            pthread_mutex_lock(&mutex4);
            if (readyqueue[i] == -1)
            {
                i++;
                // i = rand() % (sizeofqueue);
                pthread_mutex_unlock(&mutex4);
                usleep(40);

                break;
            }
            // printf("Student %d served at table %d index\n", id_number, readyqueue[i]);
            fflush(stdout);
            student_in_slot(i);
            pthread_mutex_lock(&mutex);
            pthread_mutex_unlock(&mutex);
            pthread_mutex_unlock(&mutex4);
            usleep(40);
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
    printf("Student %d finished eating\n",id_number);
    fflush(stdout);
    return NULL;
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
            pthread_mutex_lock(&Robot_mutex[i]);
            if (Robots[i].vessels_prepared > 0 && Tables[id_number].current_capacity == 0)
            {
                Tables[id_number].current_capacity = Robots[i].capacity_of_vessel;
                printf("Table %d was refilled by robot %d and has %d servings left\n", id_number, i, Robots[i].capacity_of_vessel);
                fflush(stdout);
                Robots[i].vessels_prepared--;
            }
            pthread_mutex_unlock(&Robot_mutex[i]);

            while (Tables[id_number].current_capacity > 0)
            {
                int number_of_slots = min(Tables[id_number].current_capacity, min(rand() % (10) + 1, n_of_students));
                Tables[id_number].current_slots = 0;
                pthread_mutex_lock(&szofqueuemutex);
                sizeofqueue++;
                printf("Number of slots %d at index with sizeofqueue %d\n", number_of_slots, sizeofqueue);
                fflush(stdout);
                ready_to_serve_table(number_of_slots, id_number);
                Tables[id_number].current_capacity -= number_of_slots;
            }
        }
    }
    return NULL;
}

int main()
{

    for (int i = 0; i < 10005; i++)
        pthread_mutex_init(&Robot_mutex[i], NULL), pthread_mutex_init(&Table_mutex[i], NULL);
    srand(time(NULL));
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
    //     for (int i = 0; i < sizeofqueue; i++)
    //     {
    //         if (readyqueue[i] == -1)
    //             continue;
    //         student_in_slot(i);
    //         n_of_students--;
    //     }
    // }
    // uncomment if students dont work
    // srand(time(NULL));
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
        // printf("Student %d finished eating\n", i);
        // fflush(stdout);
    }
}