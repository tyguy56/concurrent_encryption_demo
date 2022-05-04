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
 *      input_mutex:        used to block threads from consuming the character in the input buffer the processes is working on
 *      output_mutex:       used to block threads from consuming the character in the output buffer the processes is working on
 *      encrypt_full:    used to block and signal the encryption thread that a new character has been placed in the input buffer
 *      encryptEmpty:   used to block and signal the encryption thread that the output and writer threads have space for a new character
 *      readEmpty:      used to block and signal the reader thread that the input buffer has space for a new character
 *      write_full:      used to block and signal the writer thread that the output buffer has a new character from the encryption thread
 *      in_buffer_full:         used to block and signal the input counter thread that the input buffer has a new character from the reader thread (solves cold start issue)
 *      out_full:        used to block and signal the output counter thread that the output buffer has a new character from the encryption thread
 *      reset_req_sem:  used to block the reader thread from executing until the input and output buffers are equal
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

sem_t input_mutex;
sem_t output_mutex;
sem_t encrypt_full;
sem_t encrypt_empty_output;
sem_t encrypt_empty_writer;
sem_t read_empty_input;
sem_t read_empty_encryption;
sem_t write_full;
sem_t in_buffer_full;
sem_t out_full;
sem_t reset_req_sem;

int buffer_size;
int reader_index;
int encrypt_index;

/* Thread function prototypes */
void *readerThread();        /* reader thread runner */
void *writerThread();        /* writer thread runner */
void *encryptionThread();    /* encryption thread runner */
void *inputCounterThread();  /* input counter thread runner */
void *outputCounterThread(); /* output counter thread runner */

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

    in_buf = malloc(sizeof(char) * buffer_size);
    out_buf = malloc(sizeof(char) * buffer_size);

    // 4) initalize shared variables
    sem_init(&encrypt_full, 0, 0);
    sem_init(&encrypt_empty_output, 0, buffer_size);
    sem_init(&encrypt_empty_writer, 0, buffer_size);
    sem_init(&read_empty_input, 0, buffer_size);
    sem_init(&read_empty_encryption, 0, buffer_size);
    sem_init(&write_full, 0, 0);
    sem_init(&in_buffer_full, 0, 0);
    sem_init(&out_full, 0, 0);
    sem_init(&input_mutex, 0, 1);
    sem_init(&output_mutex, 0, 1);
    sem_init(&reset_req_sem, 0, 1);

    // 5) create other threads
    printf("Creating READER thread...\n");
    pthread_create(&reader, NULL, readerThread, NULL); // start the reader thread in the calling process

    printf("Creating INPUT COUNTER thread...\n");
    pthread_create(&input_counter, NULL, inputCounterThread, NULL); // start the input counter thread in the calling process

    printf("Creating ENCRYPTION thread...\n");
    pthread_create(&encryption, NULL, encryptionThread, NULL); // start the encryption thread in the calling process

    printf("Creating OUTPUT COUNTER thread...\n");
    pthread_create(&output_counter, NULL, outputCounterThread, NULL); // start the output counter thread in the calling process

    printf("Creating WRITER thread...\n");
    pthread_create(&writer, NULL, writerThread, NULL); // start the writer thread in the calling process

    // 6) wait for all threads to complete

    pthread_join(reader, NULL);
    pthread_join(input_counter, NULL);
    pthread_join(encryption, NULL);
    pthread_join(output_counter, NULL);
    pthread_join(writer, NULL);

    // 7) log character counts
    log_counts();

    sem_destroy(&encrypt_full);
    sem_destroy(&encrypt_empty_output);
    sem_destroy(&encrypt_empty_writer);
    sem_destroy(&read_empty_input);
    sem_destroy(&read_empty_encryption);
    sem_destroy(&write_full);
    sem_destroy(&in_buffer_full);
    sem_destroy(&out_full);
    sem_destroy(&input_mutex);
    sem_destroy(&output_mutex);

    return 0;
}
/**
 * runs the readerThread responsible for reading from the file and putting characters into the input buffer
 *
 * @return void*
 */

void *readerThread()
{
    char c;
    reader_index = 0;

    do
    {
        c = read_input();
        sem_wait(&reset_req_sem);
        sem_wait(&read_empty_input);
        sem_wait(&read_empty_encryption);
        sem_wait(&input_mutex);
        in_buf[reader_index] = c;
        sem_post(&input_mutex);
        sem_post(&encrypt_full);
        sem_post(&in_buffer_full);
        sem_post(&reset_req_sem);
        reader_index = (reader_index + 1) % buffer_size;
    }while (c != EOF);
    return NULL;
}
/**
 * runs the input counter thread responsible for count the occurances of characters put into the input buffer
 *
 * @return void*
 */

void *inputCounterThread()
{
    int idx = 0;

    while (1)
    {
        sem_wait(&in_buffer_full);
        sem_wait(&input_mutex);
        char c = in_buf[idx];
        if (in_buf[idx] == EOF)
        {
            sem_post(&read_empty_input);
            sem_post(&input_mutex);
            break;
        }
        count_input(c);
        sem_post(&read_empty_input);
        sem_post(&input_mutex);
        idx = (idx + 1) % buffer_size;
    }
    return NULL;
}

/**
 * runs the encryption thread responsible for encrypting characters in the input buffer and placing the encrypted characters into the output buffer
 *
 * @return void*
 */
void *encryptionThread()
{
    encrypt_index = 0;
    while (1)
    {
        sem_wait(&encrypt_full);
        sem_wait(&input_mutex);
        char c = in_buf[encrypt_index];
        if (in_buf[encrypt_index] == EOF)
        {
            out_buf[encrypt_index] = EOF;
            sem_post(&read_empty_encryption);
            sem_post(&write_full);
            sem_post(&out_full);
            sem_post(&input_mutex);
            sem_post(&output_mutex);
            break;
        }
        c = encrypt(c);
        sem_wait(&encrypt_empty_output);
        sem_wait(&encrypt_empty_writer);
        sem_wait(&output_mutex);
        out_buf[encrypt_index] = c;
        sem_post(&read_empty_encryption);
        sem_post(&write_full);
        sem_post(&out_full);
        sem_post(&input_mutex);
        sem_post(&output_mutex);
        encrypt_index = (encrypt_index + 1) % buffer_size;
    }
    return NULL;
}

/**
 * output counter thread responsible for counting each occurance of a character in the output buffer
 *
 * @return void*
 */

void *outputCounterThread()
{
    int idx = 0;

    while (1)
    {
        sem_wait(&out_full);
        sem_wait(&output_mutex);
        char c = out_buf[idx];
        if (out_buf[idx] == EOF)
        {
            sem_post(&encrypt_empty_output);
            sem_post(&output_mutex);
            break;
        }
        count_output(c);
        sem_post(&encrypt_empty_output);
        sem_post(&output_mutex);
        idx = (idx + 1) % buffer_size;
    }
    return NULL;
}

/**
 * writer thread responsible for taking each character in the output buffer and writing it to the output file
 *
 * @return void*
 */

void *writerThread()
{
    int idx = 0;

    while (1)
    {
        sem_wait(&write_full);
        sem_wait(&output_mutex);
        char c = out_buf[idx];
        if (out_buf[idx] == EOF)
        {
            sem_post(&encrypt_empty_writer);
            sem_post(&output_mutex);
            sem_post(&output_mutex);
            break;
        }
        write_output(c);
        sem_post(&encrypt_empty_writer);
        sem_post(&output_mutex);
        idx = (idx + 1) % buffer_size;
    }
    return NULL;
}

/**
 * function that takes and blocks the reader thread and counts the input and output logs
 *
 */

void reset_requested()
{
    sem_wait(&reset_req_sem);
    log_counts();
}

/**
 * function responsible for checking if the input and output buffers are equal before letting the reader thread resume
 *
 */

void reset_finished()
{
    while (1)
    {
        sem_wait(&encrypt_empty_output);
        sem_wait(&encrypt_empty_writer);
        sem_post(&reset_req_sem);
        break;
        
    }
}
