#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
int *brr;
void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

int *shareMem(size_t size)
{
    key_t mem_key = IPC_PRIVATE;
    int shm_id = shmget(mem_key, size, IPC_CREAT | 0666);
    return (int *)shmat(shm_id, NULL, 0);
}

typedef struct arg
{
    int l;
    int r;
    int *arr;
} arg;

int partition(int arr[], int l, int r)
{
    swap(&arr[(r + l) / 2], &arr[r - 1]);
    int pivot = arr[r - 1];
    // if (r-l<10){
    // printf("PIVOT ++++ %d\n",pivot);
    // }
    int j = l - 1;
    for (int i = l; i < r - 1; i++)
    {
        if (arr[i] < pivot)
        {
            j++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[j + 1], &arr[r - 1]);
    return j + 1;
}

void issorted(int arr[], int l, int r)
{
    for (int i = l; i < r - 1; i++)
        if (arr[i] > arr[i + 1])
        {
            // printf("%d %d", arr[i], arr[i + 1]);
            printf("Not Sorted\n");
            return;
        }
    printf("Sorted\n");
}

void smolsort(int arr[], int l, int r)
{
    for (int i = l; i < r; i++)
    {
        int key = arr[i];
        int ind = i;
        for (int j = i; j < r; j++)
        {
            if (key > arr[j])
            {
                ind = j;
                key = arr[j];
            }
        }
        swap(&arr[i], &arr[ind]);	Calculated weight	Grade	Range	Percentage	Feedback	
    }
}

void Process_quicksort(int arr[], int l, int r)
{
    if (r - l < 5)
    {
        smolsort(arr, l, r);
        return;
    }
    // printf("%d %d\n",l,r);
    int par = partition(arr, l, r);
    pid_t pid = fork();
    int status = 0;
    if (pid < 0)
    {
        printf("Error while forking\n");
        return ;
    }
    else if (pid == 0)
    {
        Process_quicksort(arr, l, par);
        _exit(0);
    }
    else
    {
        int status2;
        // printf("%d %d\n", l, (l + r) / 2);
        Process_quicksort(arr, par + 1, r);
        waitpid(pid, &status2, 0);
    }
}

void *Thread_quicksort(void *thr)
{
    struct arg *a = (struct arg *)thr;
    int l = a->l;
    int r = a->r;
    int *arr = a->arr;
    if (l > r)
        return NULL;
    if (r - l < 5)
    {
        smolsort(arr, l, r);
        return NULL;
    }
    pthread_t tidll, tidrr;
    struct arg ll, rr;
    int par = partition(arr, l, r);
    // printf("par is %d\n",par);
    ll.l = l, ll.r = par, ll.arr = arr;
    rr.l = par + 1, rr.r = r, rr.arr = arr;
    pthread_create(&tidll, NULL, (void *)Thread_quicksort, &ll);
    pthread_join(tidll, NULL);
    pthread_create(&tidrr, NULL, (void *)Thread_quicksort, &rr);
    pthread_join(tidrr, NULL);
    return NULL;
}

void Normal_quicksort(int arr[], int l, int r)
{
    if (r - l < 5)
    {
        smolsort(arr, l, r);
        return;
    }
    int par = partition(arr, l, r);
    Normal_quicksort(arr, l, par);
    Normal_quicksort(arr, par + 1, r);
}

int main()
{
    struct timespec ts;
    int n;
    scanf("%d", &n);
    int *arr = shareMem(4 * n);
    int *crr = shareMem(4 * n);
    brr = shareMem(4 * n);
    for (int i = 0; i < n; i++)
        arr[i] = n - i, crr[i] = arr[i];
    // scanf("%d", &arr[i]), crr[i] = arr[i];

    int l = 0, r = n;
    //quicksort(arr,l,r) sorts [l,r)

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    long double st = ts.tv_nsec / (1e9) + ts.tv_sec;
    Normal_quicksort(arr, l, r);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    long double en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("Time for Normal Quicksort is %Lfs\n",en-st);

    issorted(arr, l, r);
    for (int i = 0; i < n; i++)
        arr[i] = crr[i];

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    st = ts.tv_nsec / (1e9) + ts.tv_sec;
    Process_quicksort(arr, l, r);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("Time for Concurrent Quicksort is %Lfs\n",en-st);

    issorted(arr, l, r);

    for (int i = 0; i < n; i++)
        arr[i] = crr[i];
    pthread_t tid;
    struct arg thr;
    thr.l = 0;
    thr.r = n;
    thr.arr = arr;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    st = ts.tv_nsec / (1e9) + ts.tv_sec;
    pthread_create(&tid, NULL, (void *)Thread_quicksort, &thr);
    pthread_join(tid, NULL);
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    en = ts.tv_nsec / (1e9) + ts.tv_sec;
    printf("Time for Threaded Quicksort is %Lfs\n",en-st);

    issorted(arr, l, r);

    return 0;
}