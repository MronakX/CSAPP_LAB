/* 
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first-fit placement, and boundary tag coalescing, as described
 * in the CS:APP3e text. Blocks must be aligned to doubleword (8 byte) 
 * boundaries. Minimum block size is 16 bytes. 
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/*
 * If NEXT_FIT defined use next fit search, else use first-fit search 
 */
#define NEXT_FITx

#define DEBUG

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    
#define PUT_RP(p1, p2) (*(unsigned int *)(p1) = (unsigned int)(p2))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7) /* aligned to 8 bytes */                  
#define GET_ALLOC(p) (GET(p) & 0x1)                    

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

#define PRED_RP(bp) ((char*)(bp)) /*PRED pointer right behind the header, which is (char*)(bp) */
#define SUCC_RP(bp) ((char*)(bp) + WSIZE) /* SUCC pointer right behind the PRED, which is PRED + WSIZE */

#define GET_ADDR(bp) (*(char **)(bp))
#define PRED_BLK(bp) (GET(bp))
#define SUCC_BLK(bp) (GET(bp + WSIZE))

#define EIGHT_2_FOUR(bp, st) ((unsigned int) ((unsigned long)(bp) - (unsigned long)(st)))
#define FOUR_2_EIGHT(bp, st) ((unsigned long) ((unsigned long)(bp) + (unsigned long)(st)))
/* Global variables */
static char *heap_listp;  /* Pointer to first block */
static unsigned long HEAP_START;


#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
void mm_checkheap(int lineno);

static void sort_place(void *bp);
static void delete_from_list(void *bp);
static void printlist();


void mm_checkheap(int lineno)
{
    lineno = lineno;
}

static int choose_a_list(size_t size)
{
    if(size <= 1)
        return 1;
    else if(size <= 2)
        return 2;
    else if(size <= 4)
        return 3;
    else if(size <= 8)
        return 4;
    else if(size <= 16)
        return 5;
    else if(size <= 32)
        return 6;
    else if(size <= 64)
        return 7;
    else if(size <= 128)
        return 8;
    else if(size <= 256)
        return 9;
    else if(size <= 512)
        return 10;
    else if(size <= 1024)
        return 11;
    else if(size <= 2048)
        return 12;
    else if(size <= 4096)
        return 13;
    else 
        return 14;
}

int mm_init(void) 
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(18*WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    for(int i=1; i<=14; i++)
    {
        PUT(heap_listp + (i*WSIZE), 0);
    }
    PUT(heap_listp + (15*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (16*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (17*WSIZE), PACK(0, 1));     /* Epilogue header */
#ifdef DEBUG                   
    printf("heap start at %lx\n", (unsigned long)(heap_listp));
#endif
    HEAP_START = (unsigned long)(heap_listp);
    heap_listp += (16*WSIZE); 
    //printf("finished"); 
#ifdef DEBUG                   
    printf("heap start at %lx\n", (unsigned long)(heap_listp));
#endif
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;
#ifdef DEBUG
    printf("0. Extend_heap: ");
#endif
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* SOME SEGRETED POLICY */
#ifdef DEBUG
    printf("extend finished, size = %ld\n", size);
#endif
    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          
}

void *malloc(size_t size) 
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    
    char *bp;      
    printlist();
    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          
        asize = 2*DSIZE;                                        
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); 

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  
        place(bp, asize);                  
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  
    place(bp, asize);       
    #ifdef DEBUG
        printf("1. Malloc: malloc a new block, size = %ld\n", asize);
    #endif
    return bp;
} 

void free(void *bp)
{
    #ifdef DEBUG
        printf("the point of free=%lx\n",(unsigned long)(bp));
    #endif
    if (bp == 0) 
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    #ifdef DEBUG
        printf("2. Free: free a block, size = %ld\n", size);
    #endif

    coalesce(bp);
}

void *realloc(void *ptr, size_t size)   
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);
#ifdef DEBUG
    printf("3. Realloc: realloc a new block, oldsize = %ld, size = %ld\n", oldsize, size);
#endif
    return newptr;
}

static void place(void *bp, size_t asize)
{
#ifdef DEBUG
    printf("7. Place: ");
#endif
    size_t csize = GET_SIZE(HDRP(bp));   
    delete_from_list(bp);

    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        sort_place(bp);
        #ifdef DEBUG
            printf("place, got a smaller block, size = %ld\n", csize - asize);
        #endif
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
#ifdef DEBUG
    printf("place, cant got a smaller block\n");
#endif
    }
}


static void delete_from_list(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int i = choose_a_list(size);
    void *SUCC_listp = heap_listp - 16*WSIZE + i*WSIZE;


    if(!PRED_BLK(bp) && !SUCC_BLK(bp)) //only one block
    {
        //put_to_list(size, NULL, 0);
        PUT(SUCC_listp, 0);
        //printf("succ_listp = %lx\n", (unsigned long)(SUCC_listp));
        //printf("choose a list(size) = %lx\n", (unsigned long)(choose_a_list(size)));
        #ifdef DEBUG
            printf("deletecase1 :\n");
            printf("delete a free block from list, only one block\n");
            printf("succ_listp = %lx\n", SUCC_listp);
        #endif
    }
    else if(PRED_BLK(bp) && !SUCC_BLK(bp)) //last block
    {
        SUCC_BLK((void *)FOUR_2_EIGHT(PRED_BLK(bp), HEAP_START)) = NULL;
        PRED_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, last block\n");
        #endif
        //printlist();
    }
    else if(!PRED_BLK(bp) && SUCC_BLK(bp)) //first block
    {
        //printf("deletecase3 :\n");
        //put_to_list(size, FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START), 0);
        PUT(SUCC_listp, SUCC_BLK(bp));
        PRED_BLK(FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START)) = NULL;
        SUCC_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, first block\n");
        #endif
    }
    else if(PRED_BLK(bp) && SUCC_BLK(bp))
    {
        //printf("deletecase4 :\n");
        SUCC_BLK(FOUR_2_EIGHT(PRED_BLK(bp), HEAP_START)) = SUCC_BLK(bp);
        PRED_BLK(FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START)) = PRED_BLK(bp);
        SUCC_BLK(bp) = NULL;
        PRED_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, normal block\n");
        #endif
    }
    
}

static void sort_place(void *bp)
{
    #ifdef DEBUG
        printf("5. sort_place: \n");
    #endif
    size_t size = GET_SIZE(HDRP(bp));
    int i = choose_a_list(size);//8bytes
    void* pre = heap_listp - 16*WSIZE + i*WSIZE;
    printf("pre = %lx\n", pre);
    printf("get(pre) = %lx\n", GET(pre));
    if(!(unsigned int)GET(pre))//empty
    {
        PUT(pre, EIGHT_2_FOUR(bp, HEAP_START));
        PRED_BLK(bp) = NULL;
        SUCC_BLK(bp) = NULL;
        return;
    }
    if(GET_SIZE(HDRP(FOUR_2_EIGHT(GET(pre), HEAP_START))) >= size)
    {
        PRED_BLK(bp) = NULL;
        SUCC_BLK(bp) = GET(pre);
        PUT(pre, EIGHT_2_FOUR(bp, HEAP_START));
        return;
    }
    pre = FOUR_2_EIGHT(GET(pre), HEAP_START);
    void* nex = FOUR_2_EIGHT(SUCC_BLK(pre), HEAP_START);
    while((unsigned int)nex)
    {
        //printlist();
        //printf("FOUR_2_EIGHT(SUCC_BLK(pre), HEAP_START) = %lx\n",nex);
        if(GET_SIZE(HDRP(nex)) >= size)
        {
            break;
        }
        else
        {
            pre = nex;
            nex = FOUR_2_EIGHT(SUCC_BLK(nex), HEAP_START);
        }
    }
    printf("pre = %lx\n", (unsigned long)(pre));
    printf("nex = %lx\n", (unsigned long)(nex));
    if(!(unsigned int)nex)//last
    {
        //printf("placecase3:\n");
        SUCC_BLK(bp) = NULL;
        PRED_BLK(bp) = EIGHT_2_FOUR(pre, HEAP_START);
        SUCC_BLK(pre) = EIGHT_2_FOUR(bp, HEAP_START);
    }
    else
    {
        //printf("placecase4:\n");
        SUCC_BLK(bp) = EIGHT_2_FOUR(nex, HEAP_START);
        PRED_BLK(nex) = EIGHT_2_FOUR(bp, HEAP_START);
        PRED_BLK(bp) = EIGHT_2_FOUR(pre, HEAP_START);
        SUCC_BLK(pre) = EIGHT_2_FOUR(bp, HEAP_START);
    }
    
}

static void *coalesce(void *bp) 
{
#ifdef DEBUG
    printf("6. Coalesce: ");
#endif
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        //printf("case1: \n");
        #ifdef DEBUG
            printf("cant coalesce\n");
        #endif
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_from_list(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        #ifdef DEBUG
            printf("coalesce next\n");
        #endif
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        //printf("case3: \n");
        //delete_from_list(bp);
        delete_from_list(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        #ifdef DEBUG
            printf("coalesce prev\n");
        #endif
    }
    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        //printf("case4: \n");
        //delete_from_list(bp);
        delete_from_list(PREV_BLKP(bp));
        delete_from_list(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        #ifdef DEBUG
            printf("coalesce both\n");
        #endif
    }
    sort_place(bp);
    return bp;
}

static void *find_fit(size_t asize)
{
//#ifdef DEBUG
    //printf("8. Find_Fit: target = %d, ", asize);
//#endif
    /* First-fit search */
    size_t size = asize;
    int i = choose_a_list(asize);
    void *bp = heap_listp - 16*WSIZE + i*WSIZE;
    void *start = bp;
    bp = FOUR_2_EIGHT(GET(bp), HEAP_START);
    while(size <= 4096)
    {
        while((unsigned int)bp)
        {
            #ifdef DEBUG
                printf("the point of finding bp =%lx (after )\n",(unsigned long)(bp));
                printf("the SUCC of finding bp = %lx (after )\n",(unsigned long)(SUCC_BLK(bp)));
            #endif
            if(asize <= GET_SIZE(HDRP(bp)))
            {
                #ifdef DEBUG
                    printf("find a fit block, size = %ld\n", asize);
                #endif
                //printlist();
                return bp;
            }
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
        size = size << 1;
        start += WSIZE;
        bp = start;
        bp = FOUR_2_EIGHT(GET(bp), HEAP_START);
    }
    while((unsigned int)bp)
        {
            #ifdef DEBUG
                printf("the point of finding bp =%lx (after )\n",(unsigned long)(bp));
                printf("the SUCC of finding bp = %lx (after )\n",(unsigned long)(SUCC_BLK(bp)));
            #endif
            if(asize <= GET_SIZE(HDRP(bp)))
            {
                #ifdef DEBUG
                    //printf("find a fit block, size = %ld\n", asize);
                #endif
                return bp;
            }
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
    //printlist();
#ifdef DEBUG
    printf("no fit, extend a new block\n");
#endif
    return NULL; /* No fit */
}

void printlist()
{
    
    printf("\nprintlist!\n\n");
    size_t sz[20] = {0,1,2,4,8,16,32,64,128,256,512,1024,2048,4096,4097};
    for(int i=1; i<=14; i++)
    {
        int j = choose_a_list(sz[i]);
        void *bp = heap_listp - 16*WSIZE + j*WSIZE;
        printf("bp = %lx\n", bp);
        printf("get(bp) = %lx\n", GET(bp));
        bp = GET(bp);
        bp = FOUR_2_EIGHT(bp, HEAP_START);
        while((unsigned int)bp)
        {
            printf("case%d\n", sz[i]);
            printf("bp = %lx\n", bp);
            printf("bp's size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("succ_blk(bp) = %lx\n", SUCC_BLK(bp));
            //printf("succ_ptr(bp) = %lx\n", FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START));
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
    }
}