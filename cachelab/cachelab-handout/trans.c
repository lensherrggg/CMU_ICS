/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

char transpose_1_desc[] = "transpose 1";
void transpose_1(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int v1, v2, v3, v4, v5, v6, v7, v8;
    int bSize = 8;

    for (i = 0; i < M; i += bSize) {
        for (j = 0; j < N; j+= bSize) {
            for (k = i; k < i + bSize; k++) {
                v1 = A[k][j];
                v2 = A[k][j + 1];
                v3 = A[k][j + 2];
                v4 = A[k][j + 3];
                v5 = A[k][j + 4];
                v6 = A[k][j + 5];
                v7 = A[k][j + 6];
                v8 = A[k][j + 7];

                B[j + 0][k] = v1;
                B[j + 1][k] = v2;
                B[j + 2][k] = v3;
                B[j + 3][k] = v4;
                B[j + 4][k] = v5;
                B[j + 5][k] = v6;
                B[j + 6][k] = v7;
                B[j + 7][k] = v8;
            }
        }
    }
}

char transpose_2_desc[] = "transpose 2";
void transpose_2(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj, val0, val1, val2, val3, val4, val5, val6, val7;
	for(ii = 0; ii < N; ii += 8)
	{
		for(jj = 0; jj < M; jj += 8)
		{
			//For each row in the 8*4 block
			for(i = 0; i < 4; i++)
			{
				val0 = A[ii + i][jj + 0];
				val1 = A[ii + i][jj + 1];
				val2 = A[ii + i][jj + 2];
				val3 = A[ii + i][jj + 3];
				val4 = A[ii + i][jj + 4];
				val5 = A[ii + i][jj + 5];
				val6 = A[ii + i][jj + 6];
				val7 = A[ii + i][jj + 7];
				B[jj + 0][ii + i] = val0;
				B[jj + 1][ii + i] = val1;
				B[jj + 2][ii + i] = val2;
				B[jj + 3][ii + i] = val3;
				B[jj + 0][ii + 4 + i] = val4;
				B[jj + 1][ii + 4 + i] = val5;
				B[jj + 2][ii + 4 + i] = val6;
				B[jj + 3][ii + 4 + i] = val7;
				
			}
			//First copy the first 4 rows
			for(i = 0; i < 4; i++)//Do the fantastic transformation!
			{
				//get this row of the right-upper 4*4 block
				val0 = B[jj + i][ii + 4];
				val1 = B[jj + i][ii + 5];
				val2 = B[jj + i][ii + 6];
				val3 = B[jj + i][ii + 7];
				//update this row to its correct value
				val4 = A[ii + 4][jj + i];
				val5 = A[ii + 5][jj + i];
				val6 = A[ii + 6][jj + i];
				val7 = A[ii + 7][jj + i];

				
				B[jj + i][ii + 4] = val4;
				B[jj + i][ii + 5] = val5;
				B[jj + i][ii + 6] = val6;
				B[jj + i][ii + 7] = val7;

				//update the left lower 4*4 block of B
				B[jj + 4 + i][ii + 0] = val0;
				B[jj + 4 + i][ii + 1] = val1;
				B[jj + 4 + i][ii + 2] = val2;
				B[jj + 4 + i][ii + 3] = val3;
			}
			//update the right lower 4*4 block
			for(i = 4; i < 8; i++)
				for(j = 4; j < 8; j++)
					B[jj + j][ii + i] = A[ii + i][jj + j];
		}

	}
}

char transpose_3_desc[] = "transpose 3";
void transpose_3(int M, int N, int A[N][M], int B[M][N]) {
    int ii, jj, i, j, val0, val1, val2, val3, val4, val5, val6, val7;
	for(ii = 0; ii + 16 < N; ii += 16)
		for(jj = 0; jj + 16 < M; jj += 16)
		{
			for(i = ii; i < ii + 16; i++)
			{
				val0 = A[i][jj + 0];
				val1 = A[i][jj + 1];
				val2 = A[i][jj + 2];
				val3 = A[i][jj + 3];
				val4 = A[i][jj + 4];
				val5 = A[i][jj + 5];
				val6 = A[i][jj + 6];
				val7 = A[i][jj + 7];
				B[jj + 0][i] = val0;
				B[jj + 1][i] = val1;
				B[jj + 2][i] = val2;
				B[jj + 3][i] = val3;
				B[jj + 4][i] = val4;
				B[jj + 5][i] = val5;
				B[jj + 6][i] = val6;
				B[jj + 7][i] = val7;

				val0 = A[i][jj + 8];
				val1 = A[i][jj + 9];
				val2 = A[i][jj + 10];
				val3 = A[i][jj + 11];
				val4 = A[i][jj + 12];
				val5 = A[i][jj + 13];
				val6 = A[i][jj + 14];
				val7 = A[i][jj + 15];
				B[jj + 8][i] = val0;
				B[jj + 9][i] = val1;
				B[jj + 10][i] = val2;
				B[jj + 11][i] = val3;
				B[jj + 12][i] = val4;
				B[jj + 13][i] = val5;
				B[jj + 14][i] = val6;
				B[jj + 15][i] = val7;

			}
		}
	for(i = ii; i < N; i++)
		for(j = 0; j < M; j++)
			B[j][i] = A[i][j];
	for(i = 0; i < ii; i++)
		for(j = jj; j < M; j++)
			B[j][i] = A[i][j];
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (M == N) {
        if (N <= 32) {
            transpose_1(M, N, A[N][M], B[M][N]);
        } else {
            transpose_2(M, N, A[N][M], B[M][N]);
        }
    } else {
        transpose_3(M, N, A[N][M], B[M][N]);
    }
}


/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    // registerTransFunction(trans, trans_desc); 

    // registerTransFunction(transpose_1, transpose_1_desc);

    // registerTransFunction(transpose_2, transpose_2_desc);

    // registerTransFunction(transpose_3, transpose_3_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}