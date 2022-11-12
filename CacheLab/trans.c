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
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
	REQUIRES(M > 0);
    	REQUIRES(N > 0);
	int i, j, k, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

	if(M==60 && N==68)
	{
		for(j = 0; j < 56; j+= 8)
		{
			for (i = 0; i < N; i++) 
			{
				tmp0 = A[i][j];
				tmp1 = A[i][j+1];
				tmp2 = A[i][j+2];
				tmp3 = A[i][j+3]; 
				tmp4 = A[i][j+4]; 
				tmp5 = A[i][j+5]; 
				tmp6 = A[i][j+6]; 
				tmp7 = A[i][j+7]; 
				
				B[j][i] = tmp0;
				B[j+1][i] = tmp1;
				B[j+2][i] = tmp2;
				B[j+3][i] = tmp3;
				B[j+4][i] = tmp4;
				B[j+5][i] = tmp5;
				B[j+6][i] = tmp6;
				B[j+7][i] = tmp7;
			}
		}
		for(i = 0; i < N; i++)
		{
			tmp0 = A[i][j];
			tmp1 = A[i][j+1];
			tmp2 = A[i][j+2];
			tmp3 = A[i][j+3];
			B[j][i] = tmp0;
			B[j+1][i] = tmp1;
			B[j+2][i] = tmp2;
			B[j+3][i] = tmp3;
		}
	}
	else if((M==32 && N==32))
		for (j = 0; j < M; j += 8) 
		{
			for (i = 0; i < N; i++) 
			{
				tmp0 = A[i][j];
				tmp1 = A[i][j+1];
				tmp2 = A[i][j+2];
				tmp3 = A[i][j+3]; 
				tmp4 = A[i][j+4]; 
				tmp5 = A[i][j+5]; 
				tmp6 = A[i][j+6]; 
				tmp7 = A[i][j+7]; 
				
				B[j][i] = tmp0;
				B[j+1][i] = tmp1;
				B[j+2][i] = tmp2;
				B[j+3][i] = tmp3;
				B[j+4][i] = tmp4;
				B[j+5][i] = tmp5;
				B[j+6][i] = tmp6;
				B[j+7][i] = tmp7;
			}
		}	
	else//64 X 64
	{
		for (j = 0; j < M; j += 8)
		{
			for(k = 0; k < N; k += 8)
			{
				for(i = k; i< k+4; i++)//QIAN SI HANG
				{
					tmp0 = A[i][j];//part 1 & 2
					tmp1 = A[i][j+1];
					tmp2 = A[i][j+2];
					tmp3 = A[i][j+3]; 
					tmp4 = A[i][j+4]; 
					tmp5 = A[i][j+5]; 
					tmp6 = A[i][j+6]; 
					tmp7 = A[i][j+7]; 
					
					B[j][i] = tmp0;//part1 finished
					B[j+1][i] = tmp1;
					B[j+2][i] = tmp2;
					B[j+3][i] = tmp3;
					
					B[j][i+4] = tmp4;//part2 load tmply
					B[j+1][i+4] = tmp5;
					B[j+2][i+4] = tmp6;
					B[j+3][i+4] = tmp7;
				}
				
				for (i = j; i<j+4; i++)
				{
					tmp4 = A[k+4][i];//part3
					tmp5 = A[k+5][i];
					tmp6 = A[k+6][i];
					tmp7 = A[k+7][i];
					
					tmp0 = B[i][k+4];//part2 ^T
					tmp1 = B[i][k+5];
					tmp2 = B[i][k+6];
					tmp3 = B[i][k+7];
					
					B[i][k+4] = tmp4;//part3 store
					B[i][k+5] = tmp5;
					B[i][k+6] = tmp6;
					B[i][k+7] = tmp7;
					
					
					B[i+4][k] = tmp0;//part2^T store
					B[i+4][k+1] = tmp1;
					B[i+4][k+2] = tmp2;
					B[i+4][k+3] = tmp3;
					
					tmp0 = A[k+4][i+4];//part4
					tmp1 = A[k+5][i+4];
					tmp2 = A[k+6][i+4];
					tmp3 = A[k+7][i+4];
					
					B[i+4][k+4] = tmp0;//part4 store
					B[i+4][k+5] = tmp1;
					B[i+4][k+6] = tmp2;
					B[i+4][k+7] = tmp3;
				}
			}
		}
		
	}
    ENSURES(is_transpose(M, N, A, B)); 
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
	REQUIRES(M > 0);
    REQUIRES(N > 0);
	int i, j, k, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

	if((M==32 && N==32)||(M==60 && N==68))
		for (j = 0; j < M; j += 8) 
		{
			for (i = 0; i < N; i++) 
			{
				tmp0 = A[i][j];
				tmp1 = A[i][j+1];
				tmp2 = A[i][j+2];
				tmp3 = A[i][j+3]; 
				tmp4 = A[i][j+4]; 
				tmp5 = A[i][j+5]; 
				tmp6 = A[i][j+6]; 
				tmp7 = A[i][j+7]; 
				
				B[j][i] = tmp0;
				B[j+1][i] = tmp1;
				B[j+2][i] = tmp2;
				B[j+3][i] = tmp3;
				B[j+4][i] = tmp4;
				B[j+5][i] = tmp5;
				B[j+6][i] = tmp6;
				B[j+7][i] = tmp7;
			}
		}	
	else
	{
		for (j = 0; j < M; j += 8)
		{
			for(k = 0; k < N; k += 8)
			{
				for(i = k; i< k+4; i++)//QIAN SI HANG
				{
					tmp0 = A[i][j];//part 1 & 2
					tmp1 = A[i][j+1];
					tmp2 = A[i][j+2];
					tmp3 = A[i][j+3]; 
					tmp4 = A[i][j+4]; 
					tmp5 = A[i][j+5]; 
					tmp6 = A[i][j+6]; 
					tmp7 = A[i][j+7]; 
					
					B[j][i] = tmp0;//part1 store
					B[j+1][i] = tmp1;
					B[j+2][i] = tmp2;
					B[j+3][i] = tmp3;
					
					B[j][i+4] = tmp4;//part2 load tmply
					B[j+1][i+4] = tmp5;
					B[j+2][i+4] = tmp6;
					B[j+3][i+4] = tmp7;
				}
				
				for (i = j; i<j+4; i++)
				{
					tmp4 = A[k+4][i];//part3
					tmp5 = A[k+5][i];
					tmp6 = A[k+6][i];
					tmp7 = A[k+7][i];
					
					tmp0 = B[i][k+4];//part2 ^T
					tmp1 = B[i][k+5];
					tmp2 = B[i][k+6];
					tmp3 = B[i][k+7];
					
					B[i][k+4] = tmp4;//part3 store
					B[i][k+5] = tmp5;
					B[i][k+6] = tmp6;
					B[i][k+7] = tmp7;
					
					
					B[i+4][k] = tmp0;//part2^T store
					B[i+4][k+1] = tmp1;
					B[i+4][k+2] = tmp2;
					B[i+4][k+3] = tmp3;
					
					tmp0 = A[k+4][i+4];//part4
					tmp1 = A[k+5][i+4];
					tmp2 = A[k+6][i+4];
					tmp3 = A[k+7][i+4];
					
					B[i+4][k+4] = tmp0;//part4 store
					B[i+4][k+5] = tmp1;
					B[i+4][k+6] = tmp2;
					B[i+4][k+7] = tmp3;
				}
				
				
				for(i=k; i<k+4; i++)
				{
					tmp0 = A[i+4][j+4];
					tmp1 = A[i+4][j+5];
					tmp2 = A[i+4][j+6];
					tmp3 = A[i+4][j+7];
					
					B[j+4][i+4] = tmp0;
					B[j+5][i+4] = tmp1;
					B[j+6][i+4] = tmp2;
					B[j+7][i+4] = tmp3;
				}

			}
		}
		
	}
    ENSURES(is_transpose(M, N, A, B)); 
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
    registerTransFunction(trans, trans_desc);

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
