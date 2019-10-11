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
pthread_mutex_t studentmutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex4 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t Robot_mutex[10005];
pthread_mutex_t Table_mutex[10005];

void biryani_ready(int id_number)
{
    pthread_mutex_lock(&Robot_mutex[id_number]);
    Robots[id_number].vessels_prepared = rand() % 2 + 1;
    Robots[id_number].capacity_of_vessel = rand() % 5 + 1;
    printf("Robot %d prepared %d vessels,each with capacity %d\n", id_number, Robots[id_number].vessels_prepared, Robots[id_number].capacity_of_vessel);
    fflush(stdout);
    pthread_mutex_unlock(&Robot_mutex[id_number]);
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
        biryani_ready(id_number);
    }
    return NULL;
}

void ready_to_serve_table(int number_of_slots, int id_number)
{
    // printf("Slots assigned to %d ********* %d\n\n", id_number, number_of_slots);
    // fflush(stdout);
    while (Tables[id_number].current_slots != 0)
    {
        pthread_mutex_lock(&Table_mutex[id_number]);
        pthread_mutex_unlock(&Table_mutex[id_number]);
    }
    pthread_mutex_lock(&studentmutex);
    sleep(1);
    printf("Table %d finished serving %d slots at index\n\n", id_number, number_of_slots);
    Tables[id_number].current_capacity -= number_of_slots;
    n_of_students -= number_of_slots;
    if (n_of_students == 0)
        exit(0);
    pthread_mutex_unlock(&studentmutex);
}

void student_in_slot(int index)
{
    Tables[index].current_slots--;
    printf("Unfilled slots for table %d are %d at index %d\n", index, Tables[index].current_slots, index);
    usleep(100);
}

void wait_for_slot(int id_number)
{
    int i = 1;
    time_t starttime = time(NULL);
    time_t prevtime = time(NULL);

    while (1)
    {
        // time_t currtime = time(NULL);
        // if ((currtime - starttime) % 5 == 0 & prevtime != currtime)
        // {
        //     printf("Waiting for student %d\n", id_number);
        //     printf("Slots at table are %d\n", Tables[i].current_slots);
        //     prevtime = currtime;
        //     if (n_of_students == 0) exit(0);
        // }
        for (; i <= n_of_tables;)
        {
            if (pthread_mutex_trylock(&Table_mutex[i])) {
                i = rand() % n_of_tables + 1;
                sleep(1);
                break;
            }
            // printf("Entering lock for student %d\n",id_number);
            if (Tables[i].current_slots == 0)
            {
                i = rand() % n_of_tables + 1;
                // printf("Exiting lock2 for student %d\n",id_number);
                sleep(1);
                pthread_mutex_unlock(&Table_mutex[i]);
                break;
            }
            student_in_slot(i);
            // printf("Exiting lock1 for student %d\n",id_number);
            pthread_mutex_unlock(&Table_mutex[i]);
            usleep(40);
            return;
        }
    }
}
void *Student_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;
    wait_for_slot(id_number);
    printf("Student %d finished eating === = = = = = = = = = = =\n", id_number);
    fflush(stdout);
    return NULL;
}

void *Table_loop(void *thr)
{
    struct id_number *temp = (struct id_number *)thr;
    int id_number = temp->id;

    for (int i = 1; i <= n_of_robots; i = (i + 1) % (n_of_robots + 1) + (i == n_of_robots))
    {

        pthread_mutex_lock(&Robot_mutex[i]);
        if (Robots[i].vessels_prepared > 0 && Tables[id_number].current_capacity == 0)
        {
            sleep(1);
            Tables[id_number].current_capacity = Robots[i].capacity_of_vessel;
            printf("Table %d was refilled by robot %d and has %d servings left\n", id_number, i, Robots[i].capacity_of_vessel);
            fflush(stdout);
            Robots[i].vessels_prepared--;
        }
        pthread_mutex_unlock(&Robot_mutex[i]);

        while (Tables[id_number].current_capacity > 0)
        {
            pthread_mutex_lock(&studentmutex);
            int number_of_slots = min(Tables[id_number].current_capacity, min(rand() % (10) + 1, n_of_students));
            pthread_mutex_unlock(&studentmutex);
            sleep(1);
            Tables[id_number].current_slots = number_of_slots;
            printf("Table %d has %d slots\n", id_number, Tables[id_number].current_slots);
            ready_to_serve_table(number_of_slots, id_number);
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
    time_t starttime = time(NULL), prevtime = time(NULL);
    time_t interval = 5;
    // exit(0);
    srand(time(NULL));
    for (int iiii = 1; iiii <= n_of_students;)
    {
        struct id_number temp;
        temp.id = iiii;
        pthread_create(&Students[iiii].thread_id, NULL, (void *)Student_loop, &temp);
        iiii++;
        usleep(30000);
    }
    for (int i = 1; i <= n_of_students; i++)
    {
        pthread_join(Students[i].thread_id, NULL);
        // printf("Student %d finished eating\n", i);
        // fflush(stdout);
    }
    printf("STUDENTS SERVED\n");
    while (1)
        ;
}
//to be done

//done
//decrease n_of_students when a student is served
//changed biryani_ready()