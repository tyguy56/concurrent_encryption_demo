#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "encrypt-module.h"

char *in_buf;
char *out_buf;
/*
sem_t read_sem;
sem_t *process_sem;
sem_t *out_process_sem;
sem_t read_finished;
sem_t encrypt_sem;
sem_t encrypt_finished_sem;
*/

sem_t inMutex;
sem_t outMutex;
sem_t encryptFull, encryptEmpty, readEmpty, writeFull, inFull, outFull;

int buffer_size;
int reader_index;
int encrypt_index;
// int read_done;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t out_buffer_not_full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t out_lock = PTHREAD_MUTEX_INITIALIZER;

/* Thread function prototypes */
void *runReaderThread(void *arg);        /* reader thread runner */
void *runWriterThread(void *arg);        /* writer thread runner */
void *runEncryptionThread(void *arg);    /* encryption thread runner */
void *runInputCounterThread(void *arg);  /* input counter thread runner */
void *runOutputCounterThread(void *arg); /* output counter thread runner */

int main(int argc, char *argv[])
{
    // read_done = 0;
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
    sem_init(&encryptFull, 0, 0);
    sem_init(&encryptEmpty, 0, buffer_size);
    sem_init(&readEmpty, 0, buffer_size);
    sem_init(&writeFull, 0, 0);
    sem_init(&inFull, 0, 0);
    sem_init(&outFull, 0, 0);
    sem_init(&inMutex, 0, 1);
    sem_init(&outMutex, 0, 1);
    /*
        sem_init(&read_finished, 0, 0);

        sem_init(&encrypt_finished_sem, 0, 0);

        process_sem = malloc(sizeof(sem_t) * buffer_size);
        for (int i = 0; i < buffer_size; i++)
        {
            sem_init(&process_sem[i], 0, 0);
        }

        out_process_sem = malloc(sizeof(sem_t) * buffer_size);
        for (int i = 0; i < buffer_size; i++)
        {
            sem_init(&out_process_sem[i], 0, 0);
        }
    */
    // 5) create other threads
    printf("Creating READER thread...\n");
    pthread_create(&reader, NULL, runReaderThread, NULL); // start the reader thread in the calling process

    printf("Creating INPUT COUNTER thread...\n");
    pthread_create(&input_counter, NULL, runInputCounterThread, NULL); // start the input counter thread in the calling process

    printf("Creating ENCRYPTION thread...\n");
    pthread_create(&encryption, NULL, runEncryptionThread, NULL); // start the encryption thread in the calling process

    printf("Creating OUTPUT COUNTER thread...\n");
    pthread_create(&output_counter, NULL, runOutputCounterThread, NULL); // start the output counter thread in the calling process

    printf("Creating WRITER thread...\n");
    pthread_create(&writer, NULL, runWriterThread, NULL); // start the writer thread in the calling process

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

    // 7) log character counts
    log_counts();

    sem_destroy(&encryptFull);
    sem_destroy(&encryptEmpty);
    sem_destroy(&readEmpty);
    sem_destroy(&writeFull);
    sem_destroy(&inFull);
    sem_destroy(&outFull);
    sem_destroy(&inMutex);
    sem_destroy(&outMutex);

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
    int value;

    while ((c = read_input()) != EOF)
    {
        // printf("locking read empty\n");
        // fflush(stdout);
        sem_wait(&readEmpty);
        // printf("locking in mutex\n");
        // fflush(stdout);
        sem_wait(&inMutex);
        in_buf[reader_index] = c;
        // printf("read:%c\n", in_buf[reader_index]);
        // fflush(stdout);
        sem_post(&inMutex);
        sem_post(&encryptFull);
        sem_post(&inFull);
        reader_index = (reader_index + 1) % buffer_size;
    }
    // printf("end of reader\n");
    // fflush(stdout);
    sem_post(&inFull);
    sem_post(&encryptFull);
    reader_index = (reader_index + 1) % buffer_size;
    in_buf[reader_index] = EOF;
    /*
        pthread_mutex_lock(&lock);
        while ((c = read_input()) != EOF)
        {

            sem_getvalue(&process_sem[reader_index], &value);
            if (value != 0)
            {
                pthread_cond_wait(&buffer_not_full, &lock);
            }
            in_buf[reader_index] = c;
            sem_post(&process_sem[reader_index]);
            // sem_post(&process_sem[reader_index]);
            reader_index = ((1 + reader_index) % buffer_size);
        }
        pthread_mutex_unlock(&lock);

        sem_post(&read_finished);
        // sem_post(&process_sem[reader_index]);
        sem_post(&process_sem[reader_index]);
    */
    return NULL;
}

void *runInputCounterThread(void *arg)
{
    int read_done = 0;
    (void)arg;
    int idx = 0;
    int value;

    while (1)
    {
        sem_wait(&inFull);
        sem_wait(&inMutex);
        char c = in_buf[idx];
        count_input(c);
        // printf("in count:%c\n", c);
        sem_post(&inMutex);
        sem_post(&readEmpty);
        idx = (idx + 1) % buffer_size;
        // printf("EOF?:%d\n", c);
        // fflush(stdout);
        if (in_buf[idx] == EOF)
        {
            break;
        }
    }
    /*
    while (1)
    {
        sem_wait(&process_sem[idx]);
        sem_getvalue(&process_sem[idx], &value);
        sem_getvalue(&read_finished, &read_done);
        printf("value:%d encrypt_index:%d reader_index:%d index:%d count:%c\n", value, encrypt_index, reader_index, idx, in_buf[idx]);

        if (idx == reader_index && read_done)
        {
            break;
        }
        count_input(in_buf[idx]);
        if (value == 0)
        {
            pthread_cond_signal(&buffer_not_full);
        }
        idx = ((1 + idx) % buffer_size);
    }
    */
    return NULL;
}

void *runEncryptionThread(void *arg)
{
    int read_done = 0;
    int out_value = 0;
    (void)arg;
    encrypt_index = 0;
    int value;
    while (1)
    {
        sem_wait(&encryptFull);
        sem_wait(&inMutex);
        char c = in_buf[encrypt_index];
        sem_post(&inMutex);
        sem_post(&readEmpty);
        c = encrypt(c);
        sem_wait(&encryptEmpty);
        sem_wait(&outMutex);
        out_buf[encrypt_index] = c;
        // printf("encrypt out:%c\n", c);
        sem_post(&outMutex);
        sem_post(&outFull);
        sem_post(&writeFull);
        encrypt_index = (encrypt_index + 1) % buffer_size;
        // printf("EOF?:%d\n", c);
        // fflush(stdout);
        if (in_buf[encrypt_index] == EOF)
        {
            out_buf[encrypt_index] = EOF;
            break;
        }
    }
    /*
    pthread_mutex_lock(&out_lock);
    while (1)
    {
        sem_wait(&process_sem[encrypt_index]);
        sem_getvalue(&process_sem[encrypt_index], &value);
        sem_getvalue(&read_finished, &read_done);
        if (encrypt_index == reader_index && read_done)
        {
            break;
        }
        sem_getvalue(&out_process_sem[encrypt_index], &out_value);
        if (out_value != 0)
        {
            pthread_cond_wait(&out_buffer_not_full, &out_lock);
        }
        out_buf[encrypt_index] = encrypt(in_buf[encrypt_index]);
        // printf("encrypt:%c\n", out_buf[encrypt_index]);
        if (value == 0)
        {
            pthread_cond_signal(&buffer_not_full);
        }
        sem_post(&out_process_sem[encrypt_index]);
        sem_post(&out_process_sem[encrypt_index]);
        encrypt_index = ((1 + encrypt_index) % buffer_size);
    }
    pthread_mutex_unlock(&out_lock);
    sem_post(&encrypt_finished_sem);
    sem_post(&out_process_sem[encrypt_index]);
    sem_post(&out_process_sem[encrypt_index]);
    */
    return NULL;
}

void *runOutputCounterThread(void *arg)
{
    int encrypt_done = 0;
    (void)arg;
    int idx = 0;
    int value;

    while (1)
    {
        sem_wait(&outFull);
        sem_wait(&outMutex);
        char c = out_buf[idx];
        count_output(c);
        sem_post(&outMutex);
        sem_post(&encryptEmpty);
        idx = (idx + 1) % buffer_size;
        if (out_buf[idx] == EOF)
        {
            break;
        }
    }
    /*
    while (1)
    {
        sem_wait(&out_process_sem[idx]);
        sem_getvalue(&encrypt_finished_sem, &encrypt_done);

        if (idx == encrypt_index && encrypt_done)
        {
            break;
        }
        count_output(out_buf[idx]);
        sem_getvalue(&out_process_sem[idx], &value);
        if (value == 0)
        {
            pthread_cond_signal(&out_buffer_not_full);
        }
        idx = ((1 + idx) % buffer_size);
    }
    */
    return NULL;
}

void *runWriterThread(void *arg)
{
    int encrypt_done = 0;
    (void)arg;
    int idx = 0;
    int value;

    while (1)
    {
        sem_wait(&writeFull);
        sem_wait(&outMutex);
        char c = out_buf[idx];
        write_output(c);
        sem_post(&outMutex);
        sem_post(&encryptEmpty);
        idx = (idx + 1) % buffer_size;
        if (out_buf[idx] == EOF)
        {
            break;
        }
    }
    /*
    while (1)
    {
        sem_wait(&out_process_sem[idx]);
        sem_getvalue(&encrypt_finished_sem, &encrypt_done);

        if (idx == encrypt_index && encrypt_done)
        {
            break;
        }
        char c = out_buf[idx];
        // printf("%c\n", c);
        write_output(c);
        sem_getvalue(&out_process_sem[idx], &value);
        if (value == 0)
        {
            pthread_cond_signal(&out_buffer_not_full);
        }
        idx = ((1 + idx) % buffer_size);
    }
    */
    return NULL;
}