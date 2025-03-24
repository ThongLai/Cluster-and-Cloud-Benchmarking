/* ************************************************************* */
/*                                                               */
/* mat_mat.c                                                     */
/*                                                               */
/* Modified from Andrew Pineda and John Grondalski's original    */
/*                                                               */
/* This program benchmarks parallel matrix-matrix multiplication */
/* measuring latency and bandwidth for different matrix sizes    */
/* ************************************************************* */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN_MATRIX_SIZE 2        // Starting matrix size
#define MAX_MATRIX_SIZE 1024     // Maximum matrix size

/* File output */
#define RESULTS_FILENAME "mat_mat_results.csv"

int main(int argc, char **argv) {
    int ierr, rank, size, root = 0;
    double start_time, end_time, total_time;
    FILE *fp = NULL;

    
    /* Initiate MPI. */
    ierr = MPI_Init(&argc, &argv);
    ierr = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    ierr = MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (rank == root) {
        printf("Matrix Multiplication Benchmark\n");
        printf("Number of processes: %d\n", size);
        printf("Matrix Size (nxn)\tData Size (bytes)\tLatency (us)\tBandwidth (MiB/s)\n");

        fp = fopen(RESULTS_FILENAME, "w");
        if (fp == NULL) {
          printf("Error opening file for writing results.\n");
          MPI_Finalize();
          return 1;
        }
        fprintf(fp, "Matrix Size (nxn),Data Size (bytes),Latency (us),Bandwidth (MiB/s)\n");
    }
    
    /* Test different matrix sizes */
    for (int n = MIN_MATRIX_SIZE; n <= MAX_MATRIX_SIZE; n *= 2) {
    // for (int n = 2; n <= 12; n += 1) {
        /* Determine rows per process and remainder */
        int rows_per_proc = n / size;
        int remainder = n % size;
        
        /* Calculate my rows - distribute remainder over first processes */
        int my_rows = rows_per_proc + (rank < remainder ? 1 : 0);
        
        /* Calculate displacements for scatterv/gatherv */
        int *sendcounts = (int*)malloc(size * sizeof(int));
        int *displs = (int*)malloc(size * sizeof(int));
        
        int disp = 0;
        for (int i = 0; i < size; i++) {
            sendcounts[i] = (rows_per_proc + (i < remainder ? 1 : 0)) * n;
            displs[i] = disp;
            disp += sendcounts[i];
        }
        
        /* Allocate matrices */
        float *A = NULL;
        float *B = NULL; 
        float *C = NULL;
        float *Bpart = (float*)malloc(my_rows * n * sizeof(float));
        float *Apart = (float*)malloc(my_rows * n * sizeof(float));
        
        if (rank == root) {
            A = (float*)malloc(n * n * sizeof(float));
            B = (float*)malloc(n * n * sizeof(float));
            C = (float*)malloc(n * n * sizeof(float));
            
            /* Initialize B and C */
            for(int j = 0; j < n; j++) {
                for(int k = 0; k < n; k++) {
                    B[j*n + k] = (float)(j + k);
                }
            }
            
            for(int j = 0; j < n; j++) {
                for(int k = 0; k < n; k++) {
                    C[j*n + k] = (float)(10*j + k);
                }
            }
        } else {
            C = (float*)malloc(n * n * sizeof(float));
        }
        
        /* Sync all processes before timing */
        MPI_Barrier(MPI_COMM_WORLD);
        
        /* Start timing */
        start_time = MPI_Wtime();
        
        /* Scatter matrix B by rows using Scatterv for proper distribution */
        ierr = MPI_Scatterv(B, sendcounts, displs, MPI_FLOAT, 
                          Bpart, my_rows * n, MPI_FLOAT, 
                          root, MPI_COMM_WORLD);
        
        /* Broadcast C */
        ierr = MPI_Bcast(C, n*n, MPI_FLOAT, root, MPI_COMM_WORLD);
        
        /* Do the matrix multiplication for my rows */
        for(int i = 0; i < my_rows; i++) {
            for(int j = 0; j < n; j++) {
                Apart[i*n + j] = 0.0;
                for(int k = 0; k < n; k++) {
                    Apart[i*n + j] += Bpart[i*n + k] * C[k*n + j];
                }
            }
        }
        
        /* Gather matrix A */
        ierr = MPI_Gatherv(Apart, my_rows * n, MPI_FLOAT, 
                          A, sendcounts, displs, MPI_FLOAT, 
                          root, MPI_COMM_WORLD);
        
        /* End timing */
        end_time = MPI_Wtime();

        
        /* Output results */
        if (rank == root) {
            total_time = end_time - start_time;
            
            /* Calculate metrics */
            double latency = total_time * 1000000.0; // Convert to microseconds
            
            /* Calculate data size: B (scattered) + C (broadcast) + A (gathered) */
            int data_size_bytes = my_rows * n * sizeof(float) +  // B scatter portion
                                    n * n * sizeof(float) +         // C broadcast
                                    my_rows * n * sizeof(float);    // A gather portion
            double bandwidth = (data_size_bytes/(1024 * 1024)) / total_time;

            printf("%8dx%d\t\t%17.d\t%12.2f\t%17.2f\n", n, n, data_size_bytes, latency, bandwidth);
            fprintf(fp, "%d,%d,%f,%f\n", n, data_size_bytes, latency, bandwidth);
        }
        
        /* Free allocated memory */
        free(sendcounts);
        free(displs);
        free(Apart);
        free(Bpart);
        free(C);
        
        if (rank == root) {
            free(A);
            free(B);
        }
    }
    
    if (rank == root && fp != NULL) {
        fclose(fp);
        printf("Results saved to %s/%s\n", getenv("HOME"), RESULTS_FILENAME);
    }

    MPI_Finalize();
    return 0;
}
