#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>


#define CONNREQID 30
#define SIGNAL_ALLOCATED 1
#define SIGNAL_WRITE_DONE 2
#define SIGNAL_DONE 3
#define SIGNAL_REQ 30

typedef struct {
    long msg_type;
    pid_t cpid;
    int r1;
    int c1;
    int r2;
    int c2;
} reqmsg;

typedef struct {
    long msg_type;
    int size_data[4];
} sizemsg;

typedef struct {
    long msg_type;
    int msg_signal;
} ctlmsg;

void *thread_func(void *threaddata)
{   
    reqmsg *reqdataptr = (reqmsg*)threaddata;
    int r1 = reqdataptr->r1;
    int c1 = reqdataptr->c1;
    int r2 = reqdataptr->r2;
    int c2 = reqdataptr->c2;

    char storage_id[50];
    sprintf(storage_id,"/matrixmult%d", reqdataptr->cpid);
    int sharedfd = shm_open(storage_id, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (sharedfd == -1)
	{
        perror("open");
        exit(10);
	}
    int memsize = ((r1 * c1) + (r2 * c2) + (r1 * c2)) * sizeof(int);

    int res = ftruncate(sharedfd, memsize);
	if (res == -1)
	{
		perror("ftruncate");
        exit(20);
	}

    void *addr = mmap(NULL, memsize, PROT_WRITE | PROT_READ, MAP_SHARED, sharedfd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
        exit(30);
	}

    // Mailbox for outgoing (s->c)
    
    int outkey = ftok(".",reqdataptr->cpid);
    int outmqid = msgget(outkey, 0666 | IPC_CREAT);

    // Mailbox for incoming (c->s)
    
    int inkey = ftok(".",reqdataptr->cpid + 10000);
    int inmqid = msgget(inkey, 0666 | IPC_CREAT);

    // Message to say allocated.

    ctlmsg signalmsg;
    signalmsg.msg_type = SIGNAL_ALLOCATED;
    signalmsg.msg_signal = SIGNAL_ALLOCATED;
    msgsnd(outmqid, &signalmsg, sizeof(ctlmsg), 0);
    
    msgrcv(inmqid, &signalmsg, sizeof(ctlmsg), 2, 0);
    int *ptr = addr;

    // multiply

    int *m1 = ptr;
    int *m2 = ptr + (r1 * c1);
    int *mr = ptr + (r1 * c1 + r2 * c2);

    puts("");
    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            int sum = 0;
            for (int k = 0; k < r2; k++){
                sum = sum + m1[i * c1 + k] * m2[k * c2 + j];
            }
            mr[i * c2 + j] = sum;
        }

    }

    signalmsg.msg_type = 1;
    signalmsg.msg_signal = SIGNAL_DONE;
    msgsnd(outmqid, &signalmsg, sizeof(ctlmsg), 0);

    free(threaddata);

    return NULL;
}


int main(void)
{

    key_t key;
    int msgid;
    int running = 1;
    int ret;
    reqmsg request;

    key = ftok(".", CONNREQID);
    msgid = msgget(key, 0666 | IPC_CREAT);
    while (running) {
        // There is a memory leak here.
        msgrcv(msgid, &request, sizeof(request), SIGNAL_REQ, 0);
        reqmsg *reqmsgptr = (reqmsg*)malloc(sizeof(reqmsg));
        *reqmsgptr = request;
        pthread_t thread_id;
        if ((ret = pthread_create(&thread_id, NULL, thread_func,(void*) reqmsgptr)) != 0)
        {
            printf("Error from pthread: %d\n", ret);
            exit(1);
        }
    }
    // Handle SIGINT here to gracefully exit - did not have time to implement :(
    return 0;
}