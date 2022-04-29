#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "encrypt-module.h"

char *in_buf;
char *out_buf;
sem_t read_sem;
sem_t *process_sem;
sem_t read_finished;
int buffer_size;
int reader_index;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* Thread function prototypes */
void *runReaderThread(void *arg);        /* reader thread runner */
void *runWriterThread(void *arg);        /* writer thread runner */
void *runEncryptionThread(void *arg);    /* encryption thread runner */
void *runInputCounterThread(void *arg);  /* input counter thread runner */
void *runOutputCounterThread(void *arg); /* output counter thread runner */

int main(int argc, char *argv[])
{
    pthread_t reader, input_counter, encryption, output_counter, writer;

    // 1) check number of command arguments exiting if incorrect
    if (argc > 4)
    {
        printf("invalid number of arguments, forcing exit\n");
        exit(1);
    }

    // 2) call init with file names
    init(argv[1], argv[2], argv[3]);

    // 3) prompt users for output buffer sizes n and m
    printf("\noutput buffer size:");
    scanf("%d", &buffer_size);
    printf("\n");
    if (buffer_size == 0)
    {
        printf("error with buffer size\n");
        exit(2);
    }

    in_buf = malloc(sizeof(char) * buffer_size + 1);
    out_buf = malloc(sizeof(char) * buffer_size + 1);

    // 4) initalize shared variables
    sem_init(&read_sem, 0, 0);

    sem_init(&read_finished, 0, 0);

    process_sem = malloc(sizeof(sem_t) * buffer_size);
    for (int i = 0; i < buffer_size; i++)
    {
        sem_init(&process_sem[i], 0, 0);
    }

    // 5) create other threads
    printf("Creating READER thread...\n");
    pthread_create(&reader, NULL, runReaderThread, in_buf); // start the reader thread in the calling process

    printf("Creating WRITER thread...\n");
    pthread_create(&writer, NULL, runWriterThread, out_buf); // start the writer thread in the calling process

    printf("Creating ENCRYPTION thread...\n");
    pthread_create(&encryption, NULL, runEncryptionThread, in_buf); // start the encryption thread in the calling process

    printf("Creating INPUT COUNTER thread...\n");
    pthread_create(&input_counter, NULL, runInputCounterThread, in_buf); // start the input counter thread in the calling process

    printf("Creating OUTPUT COUNTER thread...\n");
    pthread_create(&output_counter, NULL, runOutputCounterThread, out_buf); // start the output counter thread in the calling process

    // 6) wait for all threads to complete

    pthread_join(reader, NULL);
    printf("killing reader \n");
    pthread_join(input_counter, NULL);
    printf("killing input_counter \n");
    pthread_join(encryption, NULL);
    printf("killing encryption \n");
    pthread_join(output_counter, NULL);
    printf("killing output_counter \n");
    pthread_join(writer, NULL);
    printf("killing writer OUTPUT \n");
    /* Destroy mutex and thread conditions before completion */

    // 7) log character counts
    log_counts();

    return 0;
}
/**
 * runs the readerThread responsible for reading from the file and putting characters into the input buffer
 *
 * @param arg
 * @return void*
 */

void *runReaderThread(void *arg)
{
    (void)arg;
    char c;
    reader_index = 0;

    pthread_mutex_lock(&lock);
    while ((c = read_input()) != EOF)
    {
        int value;
        sem_getvalue(&process_sem[reader_index], &value);
        if (value != 0)
        {
            // printf("im here :)\n");
            pthread_cond_wait(&buffer_not_full, &lock);
            // printf("im not here :(\n");
        }
        in_buf[reader_index] = c;
        sem_post(&process_sem[reader_index]);
        sem_post(&process_sem[reader_index]);
        sem_getvalue(&process_sem[reader_index], &value);
        // printf("sem %d value:%d\n", reader_index, value);
        reader_index = ((1 + reader_index) % buffer_size);
    }
    pthread_mutex_unlock(&lock);
    sem_post(&read_finished);
    return NULL;
}

void *runInputCounterThread(void *arg)
{
    (void)arg;
    int idx = 0;
    int read_done = 0;
    while (!read_done || (idx != reader_index))
    {
        sem_wait(&process_sem[idx]);
        count_input(in_buf[idx]);
        int value;
        sem_getvalue(&process_sem[idx], &value);
        if (value == 0)
        {
            pthread_cond_signal(&buffer_not_full);
        }
        idx = ((1 + idx) % buffer_size);
        sem_getvalue(&read_finished, &read_done);
    }
    return NULL;
}

void *runEncryptionThread(void *arg)
{
    (void)arg;
    int idx = 0;
    int read_done = 0;
    while (!read_done || (idx != reader_index))
    {
        sem_wait(&process_sem[idx]);
        encrypt(in_buf[idx]);
        /*
        TODO:
            check the output buffer semaphore to ensure we can place an encrypted character in the output buffer
        */
        idx = ((1 + idx) % buffer_size);
        int value;
        sem_getvalue(&process_sem[idx], &value);
        if (value == 0)
        {
            pthread_cond_signal(&buffer_not_full);
        }
        sem_getvalue(&read_finished, &read_done);
    }
    return NULL;
}

void *runOutputCounterThread(void *arg)
{
    (void)arg;
    return NULL;
}

void *runWriterThread(void *arg)
{
    (void)arg;
    return NULL;
}