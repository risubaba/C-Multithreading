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
    int current_no_of_students;
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

void *Robot_loop(void *thr)
{
    //for the table code start Robot_loop again when vessels_prepared reaches 0 ..... no need to keep code in while (1) loop
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    Robots[id_number].time_to_prepare = rand() % (5 - 2 + 1) + 2;
    sleep(Robots[id_number].time_to_prepare);
    Robots[id_number].vessels_prepared = rand() % (10 - 1 + 1) + 1;
    Robots[id_number].capacity_of_vessel = rand() % (50 - 25 + 1) + 25;
    return NULL;
}

void *Table_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    if (Tables[id_number].current_capacity == 0)
    {
        for (int i = 1; i <= n_of_robots; i = (i + 1) % n_of_robots)
        {
            if (Robots[i].vessels_prepared > 0)
            {
                Tables[id_number].current_capacity = Robots[i].capacity_of_vessel;
                Robots[i].vessels_prepared--;
                struct id_number temp;
                temp.id = i;
                pthread_create(&Robots[i].thread_id, NULL, (void *)Robot_loop, &temp);
                usleep(20);
                break;
            }
        }
    }
    return NULL;
}

int main()
{
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
        i++;
        pthread_create(&Robots[i].thread_id, NULL, (void *)Robot_loop, &temp);
        usleep(20);
    }
    for (int i = 1; i <= n_of_tables;)
    {
        struct id_number temp;
        temp.id = i;
        i++;
        pthread_create(&Tables[i].thread_id, NULL, (void *)Table_loop, &temp);
        usleep(20);
    }
}