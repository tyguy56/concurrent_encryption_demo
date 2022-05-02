/**
 * @file encrypt-drive.c
 * @author Tyler Lund (tblund@iastate.edu)
 * @brief
 * Simple concurrent program responsible for taking a file in encrypting it, counting the characters before and
 * after encryption and then writting the output to a specificed output file. The code works by using a series of
 * pthreads and semaphores to lock individual processes. Initally the code requests the user for a specificed buffer
 * size which is used to create the input and output buffers which are in turn used to hold the characters as they are
 * being processesed by the varius threads. The code then initalizes several semaphores specified as follows
 *
 *      inMutex:        used to block threads from consuming the character in the input buffer the processes is working on
 *      outMutex:       used to block threads from consuming the character in the output buffer the processes is working on
 *      encryptFull:    used to block and signal the encryption thread that a new character has been placed in the input buffer
 *      encryptEmpty:   used to block and signal the encryption thread that the output and writer threads have space for a new character
 *      readEmpty:      used to block and signal the reader thread that the input buffer has space for a new character
 *      writeFull:      used to block and signal the writer thread that the output buffer has a new character from the encryption thread
 *      inFull:         used to block and signal the input counter thread that the input buffer has a new character from the reader thread (solves cold start issue)
 *      outFull:        used to block and signal the output counter thread that the output buffer has a new character from the encryption thread
 *
 * The code proceeds to create and execute the required five pthreads to work in conjunction waiting for each thread to rejoin, log counts and then destroy
 * the semaphores
 *
 * @version 0.1
 * @date 2022-05-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include "encrypt-module.h"

char *in_buf;
char *out_buf;

sem_t inMutex;
sem_t outMutex;
sem_t encryptFull, encryptEmpty, readEmpty, writeFull, inFull, outFull;

int buffer_size;
int reader_index;
int encrypt_index;

/* Thread function prototypes */
void *runReaderThread();        /* reader thread runner */
void *runWriterThread();        /* writer thread runner */
void *runEncryptionThread();    /* encryption thread runner */
void *runInputCounterThread();  /* input counter thread runner */
void *runOutputCounterThread(); /* output counter thread runner */

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
    sem_init(&encryptFull, 0, 0);
    sem_init(&encryptEmpty, 0, buffer_size);
    sem_init(&readEmpty, 0, buffer_size);
    sem_init(&writeFull, 0, 0);
    sem_init(&inFull, 0, 0);
    sem_init(&outFull, 0, 0);
    sem_init(&inMutex, 0, 1);
    sem_init(&outMutex, 0, 1);

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
    pthread_join(input_counter, NULL);
    pthread_join(encryption, NULL);
    pthread_join(output_counter, NULL);
    pthread_join(writer, NULL);

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
 * @return void*
 */

void *runReaderThread()
{
    char c;
    reader_index = 0;

    while ((c = read_input()) != EOF)
    {
        sem_wait(&readEmpty);
        sem_wait(&inMutex);
        in_buf[reader_index] = c;
        sem_post(&inMutex);
        sem_post(&encryptFull);
        sem_post(&inFull);
        reader_index = (reader_index + 1) % buffer_size;
    }
    sem_post(&inFull);
    sem_post(&encryptFull);
    reader_index = (reader_index + 1) % buffer_size;
    in_buf[reader_index] = EOF;
    return NULL;
}
/**
 * runs the input counter thread responsible for count the occurances of characters put into the input buffer
 *
 * @return void*
 */

void *runInputCounterThread()
{
    int idx = 0;

    while (1)
    {
        sem_wait(&inFull);
        sem_wait(&inMutex);
        char c = in_buf[idx];
        count_input(c);
        sem_post(&inMutex);
        sem_post(&readEmpty);
        idx = (idx + 1) % buffer_size;
        if (in_buf[idx] == EOF)
        {
            break;
        }
    }
    return NULL;
}

/**
 * runs the encryption thread responsible for encrypting characters in the input buffer and placing the encrypted characters into the output buffer
 *
 * @return void*
 */
void *runEncryptionThread()
{
    encrypt_index = 0;
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
        sem_post(&outMutex);
        sem_post(&outFull);
        sem_post(&writeFull);
        encrypt_index = (encrypt_index + 1) % buffer_size;
        if (in_buf[encrypt_index] == EOF)
        {
            out_buf[encrypt_index] = EOF;
            break;
        }
    }
    return NULL;
}

/**
 * output counter thread responsible for counting each occurance of a character in the output buffer
 *
 * @return void*
 */

void *runOutputCounterThread()
{
    int idx = 0;

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
    return NULL;
}

/**
 * writer thread responsible for taking each character in the output buffer and writing it to the output file
 *
 * @return void*
 */

void *runWriterThread()
{
    int idx = 0;

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
    return NULL;
}