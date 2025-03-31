#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

// Define message parameters
#define MIN_SIZE 1              // Starting message size (bytes)
#define MAX_SIZE 1048576        // Maximum message size (1MB)
#define NUM_REPETITIONS 100     // Number of NUM_REPETITIONS for timing
#define NUM_WARMUP 10           // Number of NUM_WARMUP iterations

/* File output */
#define RESULTS_FILENAME "pingpong_results.csv"

int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    FILE *fp = NULL;

    // Check for at least 2 processes
    if (size < 2) {
        if (rank == 0) {
            printf("This program requires at least 2 processes\n");
        }
        MPI_Finalize();
        exit(0);
    }



    // Calculate round-robin communication partners
    int send_to = (rank + 1) % size;
    int recv_from = (rank - 1 + size) % size;
    
    if (rank == 0) {
        printf("Pingpong Benchmark (Round-Robin Implementation)\n");
        printf("Number of processes: %d\n", size);
        printf("Message Size (bytes)\tLatency (us)\tBandwidth (MiB/s)\n");

        fp = fopen(RESULTS_FILENAME, "w");
        if (fp == NULL) {
            printf("Error opening file for writing results.\n");
            MPI_Finalize();
            return 1;
        }
        fprintf(fp, "Message Size (bytes),Latency (us),Bandwidth (MiB/s)\n");
    }
    
    // Test different message sizes
    for (int message_size = MIN_SIZE; message_size <= MAX_SIZE; message_size *= 2) {
        char* message = malloc(message_size);
        
        // Initialize the message with some data
        for (int i = 0; i < message_size; i++) {
            message[i] = 'a';
        }
        
        // NUM_WARMUP
        for (int i = 0; i < NUM_WARMUP; i++) {
            if (rank == 0) {
                MPI_Send(message, message_size, MPI_CHAR, send_to, 0, MPI_COMM_WORLD);
                MPI_Recv(message, message_size, MPI_CHAR, recv_from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                MPI_Recv(message, message_size, MPI_CHAR, recv_from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(message, message_size, MPI_CHAR, send_to, 0, MPI_COMM_WORLD);
            }
        }
        
        // Synchronize all processes before timing
        MPI_Barrier(MPI_COMM_WORLD);
        
        // Start timing
        double start_time = MPI_Wtime();
        
        for (int i = 0; i < NUM_REPETITIONS; i++) {
            if (rank == 0) {
                MPI_Send(message, message_size, MPI_CHAR, send_to, 0, MPI_COMM_WORLD);
                MPI_Recv(message, message_size, MPI_CHAR, recv_from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else {
                MPI_Recv(message, message_size, MPI_CHAR, recv_from, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(message, message_size, MPI_CHAR, send_to, 0, MPI_COMM_WORLD);
            }
        }
        
        // End timing
        double end_time = MPI_Wtime();
        
        // Calculate and output results (from process 0)
        if (rank == 0) {
            double elapsed_time = (end_time - start_time) * 1000000; // Convert to microseconds
            double time_per_iteration = elapsed_time / NUM_REPETITIONS;
            double bandwidth = (message_size/(1024.0 * 1024.0)) / (time_per_iteration/1000000); // MiB/s
            
            printf("%20.d\t%12.2f\t%17.2f\n", message_size, time_per_iteration, bandwidth);

            // Write to file
            fprintf(fp, "%d,%f,%f\n", message_size, time_per_iteration, bandwidth);
        }
        
        free(message);
    }

    if (rank == 0 && fp != NULL) {
        fclose(fp);
        printf("Results saved to %s/%s\n", getenv("HOME"), RESULTS_FILENAME);
    }
    
    MPI_Finalize();
    return 0;
}
