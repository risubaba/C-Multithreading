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

void *Robot_prepare_food(void *thr)
{
    //for the table code start Robot_loop again when vessels_prepared reaches 0 ..... no need to keep code in while (1) loop
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    Robots[id_number].time_to_prepare = rand() % (5 - 2 + 1) + 2;
    // Robots[id_number].vessels_prepared = -1 ;
    sleep(Robots[id_number].time_to_prepare);
    Robots[id_number].vessels_prepared = rand() % (10 - 1 + 1) + 1;
    Robots[id_number].capacity_of_vessel = rand() % (50 - 25 + 1) + 25;
    printf("Robot %d prepared %d vessels with capacity %d each\n",id_number,Robots[id_number].vessels_prepared,Robots[id_number].capacity_of_vessel);
    return NULL;
}
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
void *Table_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    printf("Table number %d\n", id_number);
    if (Tables[id_number].current_capacity == 0)
    {
        for (int i = 1; i <= n_of_robots; i = (i + 1) % (n_of_robots + 1) + (i == n_of_robots))
        {
            // printf("Going through i == %d\n",i);
            // if (Robots[i].vessels_prepared > 0)
            pthread_mutex_lock(&mutex2);
            if (Robots[i].vessels_prepared > 0)
            {
                Tables[id_number].current_capacity = Robots[i].capacity_of_vessel;
                // pthread_mutex_lock(&mutex);
                readyqueue[sizeofqueue] = id_number;
                sizeofqueue++;
                // pthread_mutex_unlock(&mutex);
                printf("Table %d was refilled by robot %d and has %d servings left\n", id_number, i, Robots[i].capacity_of_vessel);
                Robots[i].vessels_prepared--;
                struct id_number temp;
                temp.id = i;
                if (Robots[i].vessels_prepared == 0)
                    pthread_create(&Robots[i].thread_id, NULL, (void *)Robot_prepare_food, &temp), usleep(20);
                pthread_mutex_unlock(&mutex2);
                break;
            }
            pthread_mutex_unlock(&mutex2);
        }
    }
    return NULL;
}

int main()
{
    srand(time(NULL));
    printf("Enter number of robots / tables / students\n");
    scanf("%d %d %d", &n_of_robots, &n_of_tables, &n_of_students);
    Robots = shareMemRobot((n_of_robots + 1) * sizeof(struct robot));
    Tables = shareMemTable((n_of_tables + 1) * sizeof(struct table));
    Students = shareMemStudent((n_of_students + 1) * sizeof(struct student));
    for (int i = 1; i <= n_of_tables; i++)
        Tables[i].current_capacity = 0;
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

    for (int i = readypointer; i < sizeofqueue; i++)
        printf("Table %d has %d servings", readyqueue[i], Tables[readyqueue[i]].current_capacity);
    while (n_of_students > 0)
    {
        if (readypointer < sizeofqueue)
        {
            printf("+++++++++++++++++++++ %d\n",Tables[readyqueue[readypointer]].current_capacity);
            int student_to_serve = min(Tables[readyqueue[readypointer]].current_capacity, min(rand() % (10 - 1 + 1) + 1, n_of_students));
            printf("Serving %d students at Table number %d\n", student_to_serve, readyqueue[readypointer]);
            n_of_students -= student_to_serve;
            Tables[readyqueue[readypointer]].current_capacity -= student_to_serve;
            if (Tables[readyqueue[readypointer]].current_capacity == 0)
            {
                readypointer++;
            }
        }
    }
}