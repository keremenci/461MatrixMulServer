#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <fcntl.h>

#define CONNREQID 30
#define SIGNAL_ALLOCATED 1
#define SIGNAL_WRITE_DONE 2
#define SIGNAL_DONE 3
#define SIGNAL_REQ 30

extern int errno;

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
    int msg_signal;
} ctlmsg;

int **readmatrix(size_t *rows, size_t *cols, const char *filename)
{
    if(rows == NULL || cols == NULL || filename == NULL)
        return NULL;

    *rows = 0;
    *cols = 0;

    FILE *fp = fopen(filename, "r");

    if(fp == NULL)
    {
        fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
        return NULL;
    }

    int **matrix = NULL, **tmp;

    char line[1024];
    
    // get dimensions
    fgets(line,sizeof line, fp);
    sscanf(line, "%d %d\n", rows, cols);

    // allocate matrix

    matrix = (int**)malloc(*rows * sizeof(int*));
    for (int i = 0; i < *rows; i++)
        matrix[i] = (int*)malloc(*cols * sizeof(int));

    if(matrix == NULL)
    {
        fclose(fp);
        return NULL;
    }
    char *scan = line;
    int offset = 0;
    int bruh = 0;
    for(int i = 0; i < *rows; ++i) {
        for(int j = 0; j < *cols; ++j) {
            fscanf(fp, "%d", matrix[i] + j);
        }
    }

    fclose(fp);

    return matrix;
}

int printmatrix(size_t *r, size_t *c, int **matrix) {
    for(size_t i = 0; i < *r; ++i)
    {
        for(size_t j = 0; j < *c; ++j)
            printf("%d ", matrix[i][j]);
        puts("");

    }
    puts("");
}

int main(int argc, char** argv) {
    char m1fn[32];
    char m2fn[32];
    key_t reqkey;
    int msqid;
    pid_t mypid = getpid();
    reqmsg request;


    printf("Please enter the filename for matrix 1: ");
    fgets(m1fn,32,stdin);
    m1fn[strcspn(m1fn, "\n")] = 0;

    printf("Please enter the filename for matrix 2: ");
    fgets(m2fn,32,stdin);
    m2fn[strcspn(m2fn, "\n")] = 0;
    size_t r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    int **m1 = readmatrix(&r1, &c1, m1fn);
    if (m1 == NULL) return 0;
    int **m2 = readmatrix(&r2, &c2, m2fn);
    if (m2 == NULL) return 0;

    if (c1 != r2) {
        fprintf(stderr, "Matrix dimensions %dx%d and %dx%d not compatible for multiplication.\n", r1, c1, r2, c2);
        // freeing memory
        for(size_t i = 0; i < r1; ++i)
            free(m1[i]);
        free(m1);

        for(size_t i = 0; i < r2; ++i)
            free(m2[i]);
        free(m2);
        return -1;
    }

    // Server outgoing mq (request)

    reqkey = ftok(".",CONNREQID);
    msqid = msgget(reqkey, 0666 | IPC_CREAT);
    request.msg_type = SIGNAL_REQ;
    request.cpid = mypid;
    request.r1 = r1;
    request.c1 = c1;
    request.r2 = r2;
    request.c2 = c2;
    msgsnd(msqid, &request, sizeof(request), 0);
    

    // Thread incoming mq (s->c)

    int inkey = ftok(".",mypid);
    int inmqid = msgget(inkey, 0666 | IPC_CREAT);

    // Thread outgoing mq (c->s)

    int outkey = ftok(".",mypid + 10000);
    int outmqid = msgget(outkey, 0666 | IPC_CREAT);

    ctlmsg signalmsg;
    msgrcv(inmqid, &signalmsg, sizeof(ctlmsg), 1, 0);

    //printmatrix(&r1, &c1, m1);
    //printmatrix(&r2, &c2, m2);

    // get shared memory file descriptor (NOT a file)
    char storage_id[150];
    sprintf(storage_id,"/matrixmult%d", mypid);
	int sharedfd = shm_open(storage_id, O_RDWR, S_IRUSR | S_IWUSR);
	if (sharedfd == -1)
	{
		perror("open");
		return 10;
	}
    int memsize = ((r1 * c1) + (r2 * c2) + (r1 * c2)) * sizeof(int);
	// map shared memory to process address space
	void *addr = mmap(NULL, memsize, PROT_READ | PROT_WRITE, MAP_SHARED, sharedfd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		return 30;
	}

    // Temporarily write matrices in mamadu way
    int *ptr = addr;    
    for (int i = 0; i < r1; i++){
        for (int j = 0; j < c1; j++) {
            *ptr = m1[i][j];
            ptr += 1;
        }
    }

    for (int i = 0; i < r2; i++){
        for (int j = 0; j < c2; j++) {
            *ptr = m2[i][j];
            ptr += 1;
        }
    }

    signalmsg.msg_type = SIGNAL_WRITE_DONE;
    signalmsg.msg_signal = SIGNAL_WRITE_DONE;
    msgsnd(outmqid, &signalmsg, sizeof(ctlmsg), 0);

    // wait for multiplication

    msgrcv(inmqid, &signalmsg, sizeof(ctlmsg), 1, 0);
    int *m = ptr;
    // print result
    for (int j = 0; j < r1 * c2; j++){
        printf("%d ", m[j]);
        if ((j + 1) % c2 == 0)
        {
            printf("\n");
        }
    }

    // freeing memory
    for(size_t i = 0; i < r1; ++i)
        free(m1[i]);
    free(m1);

    for(size_t i = 0; i < r2; ++i)
        free(m2[i]);
    free(m2);

    return 0;
}
