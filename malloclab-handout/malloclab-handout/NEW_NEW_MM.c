/* 
 * 1900012995 GuoDawei
 * Simple, 64-bit-specific not-clean(have some type conversion WARNING when using GDB, but not error) 
 * allocator based on two-way linked explicit & segreted free lists, 
 * nodes is free lists are sorted by its size,
 * first-fit placement(actually best-fit because of sorting), 
 * and boundary tag coalescing, as described in the CS:APP3e text.
 * 
 * Some analysis:
 * 1. sort + first-fit = best-fit
 * 2. delete a node from list is O(1)
 * 3. coalesce is O(1)
 *  
 * Functions:
 * 1. basic functions: mm_init, malloc, free, calloc, realloc
 * 2. heap-related functions: extend_heap, find_fit, coalesce
 * 3. list-related functions: sort_place, delete_from_list
 * 4. debugging: mm_checkheap, printheap, printlist
 * 
 * obviously, only place & coalesce will change the list 
 * by calling list-related functions.
 * malloc, free.. only calls place or coalesce
 * 
 * Some Details:
 * 1. use 14 segreted lists, for size of 1, 2, 3-4, 5-8, ... 2049-4096, 4096+
 * 2. list header pointers is global variables(but not array!!)
 * 3. immediately coalesce
 * 4. chunksize = 4096. Though I tried, I didn't get a better one.
 * 
 * Some optimization tricks:
 * 1. use 4-bytes relative address to store pointers in free blocks, not 8-bytes in 64-bits. 0->83
 * 2. delete footer in allocated blocks, and place the information in the next block.
 * which changed the alignment of allocated blocks from 16 to 8.  83->90
 * 3.add a special parameter in place() when placing the allocated block in a free block.
 * which makes the small allocated block lies in the front,
 * while the bigger one lies in the back. 90->99
 * 
 * BTW, I prefer printf&gdbmdriver than mm_checkheap, because the former can be
 * used anywhere but the latter only can be used in malloc or free(for me).
 * so I didnt do all check like writeup said in mm_checkheap.
 * 
 * Blocks must be aligned to doubleword (8 byte) 
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

#define DEBUGxxx
#define PRINTxxx
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  
#define MAX(x, y) ((x) > (y)? (x) : (y))  
/* It is a magic parameter, used in place(), for some special case (but not data-oriented!!!) */
#define magic		100

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7) /* aligned to 8 bytes */                  
#define GET_ALLOC(p) (GET(p) & 0x1)  

/* Read the second bit of header to know if the prev block is free */             
#define GET_SECOND(p) ((GET(p) & 0X2) >> 1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

/* PRED and SUCC (relative) pointers, which is 4 bytes*/
#define PRED_BLK(bp) (GET(bp))
#define SUCC_BLK(bp) (GET(bp + WSIZE))

/* macros to transfer the relative pointers(4 bytes) and the absolue address(8 bytes), 
 *st is where the heap starts (800000000).
 */
#define EIGHT_2_FOUR(bp, st) ((unsigned int) ((unsigned long)(bp) - (unsigned long)(st)))
#define FOUR_2_EIGHT(bp, st) ((unsigned long) ((unsigned long)(bp) + (unsigned long)(st)))

/* Global variables */
static char *heap_listp;  /* Pointer to first block */
static unsigned long HEAP_START;    /* address of the starting of heap */

/* 14 segreted free list header */
static char *SUCC_listp1;//1
static char *SUCC_listp2;//2
static char *SUCC_listp3;//3-4
static char *SUCC_listp4;//5-8
static char *SUCC_listp5;//9-16
static char *SUCC_listp6;//17-32
static char *SUCC_listp7;//33-64
static char *SUCC_listp8;//65-128
static char *SUCC_listp9;//129-256
static char *SUCC_listp10;//257-512
static char *SUCC_listp11;//513-1024
static char *SUCC_listp12;//1025-2048
static char *SUCC_listp13;//2049-4096
static char *SUCC_listp14;//4096-

/* 
 * store the addr of epilogue, 
 * useful to decide whether coalecse the newly extened 4096 block or not
 */
static char *epilogue_listp;

#ifdef NEXT_FIT
static char *rover;           /* Next fit rover */
#endif

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);

static void *find_fit(size_t asize);
static void *coalesce(void *bp);
void mm_checkheap(int lineno);

/* 
 * I changed the return type, it was void previously. 
 * Because idk how to pass the pointers argument in C, 
 * It seems never changed if I dont return it.
 */
static void *place(void *bp, size_t asize);

/* some functions I add, details below */
static void sort_place(void *bp);
static void delete_from_list(void *bp);
static void printlist();


/* a numb way to decide the header by size of block*/
static char* choose_a_list(size_t size)
{
    if(size <= 1)
        return SUCC_listp1;
    else if(size <= 2)
        return SUCC_listp2;
    else if(size <= 4)
        return SUCC_listp3;
    else if(size <= 8)
        return SUCC_listp4;
    else if(size <= 16)
        return SUCC_listp5;
    else if(size <= 32)
        return SUCC_listp6;
    else if(size <= 64)
        return SUCC_listp7;
    else if(size <= 128)
        return SUCC_listp8;
    else if(size <= 256)
        return SUCC_listp9;
    else if(size <= 512)
        return SUCC_listp10;
    else if(size <= 1024)
        return SUCC_listp11;
    else if(size <= 2048)
        return SUCC_listp12;
    else if(size <= 4096)
        return SUCC_listp13;
    else 
        return SUCC_listp14;
}

/*
 * put_to_list: choose the header by size, 
 * assign src to the header or the PRED_BLK(header), based on flag
 * I don't know how to use a function as left-value,
 * so I have to handle it inside a function
 * 
 * parameters:
 * size: the size of block
 * src: the source address
 * flag: a flag to decide what to assigned
 */
static void put_to_list(size_t size, unsigned long src, int flag)
{
    if(flag == 0)
    {
        if(size <= 1)
            SUCC_listp1 = src;
        else if(size <= 2)
            SUCC_listp2 = src;
        else if(size <= 4)
            SUCC_listp3 = src;
        else if(size <= 8)
            SUCC_listp4 = src;
        else if(size <= 16)
            SUCC_listp5 = src;
        else if(size <= 32)
            SUCC_listp6 = src;
        else if(size <= 64)
            SUCC_listp7 = src;
        else if(size <= 128)
            SUCC_listp8 = src;
        else if(size <= 256)
            SUCC_listp9 = src;
        else if(size <= 512)
            SUCC_listp10 = src;
        else if(size <= 1024)
            SUCC_listp11 = src;
        else if(size <= 2048)
            SUCC_listp12 = src;
        else if(size <= 4096)
            SUCC_listp13 = src;
        else 
            SUCC_listp14 = src;
    }
    else if(flag == 1)
    {
        if(size <= 1)
            PRED_BLK(SUCC_listp1) = src;
        else if(size <= 2)
            PRED_BLK(SUCC_listp2) = src;
        else if(size <= 4)
            PRED_BLK(SUCC_listp3) = src;
        else if(size <= 8)
            PRED_BLK(SUCC_listp4) = src;
        else if(size <= 16)
            PRED_BLK(SUCC_listp5) = src;
        else if(size <= 32)
            PRED_BLK(SUCC_listp6) = src;
        else if(size <= 64)
            PRED_BLK(SUCC_listp7) = src;
        else if(size <= 128)
            PRED_BLK(SUCC_listp8) = src;
        else if(size <= 256)
            PRED_BLK(SUCC_listp9) = src;
        else if(size <= 512)
            PRED_BLK(SUCC_listp10) = src;
        else if(size <= 1024)
            PRED_BLK(SUCC_listp11) = src;
        else if(size <= 2048)
            PRED_BLK(SUCC_listp12) = src;
        else if(size <= 4096)
            PRED_BLK(SUCC_listp13) = src;
        else 
            PRED_BLK(SUCC_listp14) = src;
    }
}

/* just initialization */
static void list_init()
{
    SUCC_listp1 = NULL;
    SUCC_listp2 = NULL;
    SUCC_listp3 = NULL;
    SUCC_listp4 = NULL;
    SUCC_listp5 = NULL;
    SUCC_listp6 = NULL;
    SUCC_listp7 = NULL;
    SUCC_listp8 = NULL;
    SUCC_listp9 = NULL;
    SUCC_listp10 = NULL;
    SUCC_listp11 = NULL;
    SUCC_listp12 = NULL;
    SUCC_listp13 = NULL;
    SUCC_listp14 = NULL;
}

/* initialize the heap and list, just from textbook */
int mm_init(void) 
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 0x3)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 0x3));     /* Epilogue header */
    epilogue_listp = heap_listp + 3*WSIZE;
#ifdef DEBUG                   
    printf("heap start at %lx\n", (unsigned long)(heap_listp));
#endif
    HEAP_START = (unsigned long)(heap_listp);
    list_init();
    heap_listp += (2*WSIZE); 
#ifdef DEBUG                   
    printf("heap start at %lx\n", (unsigned long)(heap_listp));
#endif
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}

/* extend heap by chunksize, bacisally from textbook 
 * add something about epilogue 
 */
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

    /* epilogue_alloc shows if the last block of heap is free */
    size_t epilogue_alloc = GET_SECOND((epilogue_listp));

    if(epilogue_alloc)
        PUT(HDRP(bp), PACK(size, 0x2));
    else    
        PUT(HDRP(bp), PACK(size, 0x0));

    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 0x1)); /* New epilogue header */ 
    epilogue_listp = HDRP(NEXT_BLKP(bp)); /* store the new epilogue */

    /* SOME SEGRETED POLICY */
#ifdef DEBUG
    printf("extend finished, size = %ld\n", size);
#endif
    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          
}

/* malloc a block by size, just changed the alignment of allocated block */
void *malloc(size_t size) 
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;     
    #ifdef DEBUG
        mm_checkheap(__LINE__);
    #endif 
    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)  /* at least 16 bytes */                                       
        asize = 2*DSIZE;                                    
    else
        asize = DSIZE * ((size + (WSIZE) + (DSIZE-1)) / DSIZE); /* DSIZE -> WSIZE alignment:16->8 */

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) { 
        bp = place(bp, asize);               
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  
    bp = place(bp, asize);       
    #ifdef DEBUG
        printf("1. Malloc: malloc a new block, size = %ld\n", asize);
    #endif
    return bp;
} 

/* free a block, update the header/footer of next block 
 *
 */
void free(void *bp)
{
    #ifdef DEBUG
        printf("the point of free=%lx\n",(unsigned long)(bp));
        mm_checkheap(__LINE__);
    #endif
    if (bp == 0) 
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }

    size_t prev_alloc = GET_SECOND(HDRP(bp));
    if(prev_alloc)
        PUT(HDRP(bp), PACK(size, 0x2));
    else
        PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0x0));
    /* change the second bit of next header to 0 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
    if(next_alloc)
        PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 1));
    else
        PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0));
    #ifdef DEBUG
        printf("2. Free: free a block, size = %ld\n", size);
    #endif

    coalesce(bp);
}

/* realloc, just from textbook, nothing changed */
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

/*
 * calloc - Allocate the block and set it to zero. 
 * It is a copy from mm-naive.
 */
void *calloc (size_t nmemb, size_t size)
{
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/*
 * place - place the allocated block(size = asize) in block where bp points to.
 * have to change list! so call delete_from_list and sort place
 * 
 * We changed the return type of this function, the original is void, not (void*).
 * because in some situation, we need to place the allocated block
 * in the back of free block, instead of the front.
 * The original place() didn't change bp, because bp always point to the start of the free block,
 * but now, we need to change it.
 * 
 * parameters:
 * bp: where the free block starts
 * asize: size of allocated target block
 * 
 * return:
 * starting address of allocated block
 */
static void *place(void *bp, size_t asize)
{
#ifdef DEBUG
    printf("7. Place: ");
#endif
    size_t csize = GET_SIZE(HDRP(bp));   
    delete_from_list(bp);
    /* an amazing optimazation, inspired by binary.rep
    * when malloc sequence like: small big small big...
    * it will cause greater fracture when free all the big one.
    * because those small allocated ones had divide the heap,
    * the heap will be like: 
    * small-alloc | big-free | small-alloc | big-free...
    * and the big free block cant coalesce and handle the more bigger malloc.
    * 
    * so the optimazation is to put small one in the front
    * put the bigger one in the back,
    * which skillfully avoid the situation above.
    * 
    * how to judge a block is small or big needs magic trick,
    * magic = 100 is good for me however.
    */ 
    if ((csize - asize) >= (2*DSIZE) && (asize <= magic)) { 
        size_t prev_alloc = GET_SECOND(HDRP(bp));
        if(prev_alloc)
        {
            PUT(HDRP(bp), PACK(asize, 0x3));
        }
        else
        {
            PUT(HDRP(bp), PACK(asize, 0x1));
        }
        //PUT(FTRP(bp), PACK(asize, 1));//已分配，不需要footer
        //bp = NEXT_BLKP(bp);
        PUT(HDRP(NEXT_BLKP(bp)), PACK(csize-asize, 0x2));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(csize-asize, 0));
        //printf("in place, bp = %lx\n", bp);
        sort_place(NEXT_BLKP(bp));
        #ifdef DEBUG
            printf("place, got a smaller block, size = %ld\n", csize - asize);
        #endif
    }
    else if((csize - asize) >= (2*DSIZE) && (asize > magic))
    {
        size_t prev_alloc = GET_SECOND(HDRP(bp));
        if(prev_alloc)
        {
            PUT(HDRP(bp), PACK(csize - asize, 0x2));
        }
        else
        {
            PUT(HDRP(bp), PACK(csize - asize, 0x0));
        }
        PUT(FTRP(bp), PACK(csize - asize, 0x0));
        sort_place(bp);

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(asize, 0x1));
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if(next_alloc)
        {
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x3));
        }
        else
        {
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x2));
        }
    }   

    else { 
        size_t prev_alloc = GET_SECOND(HDRP(bp));
        if(prev_alloc)
        {
            PUT(HDRP(bp), PACK(csize, 0x3));
        }
        else
        {
            PUT(HDRP(bp), PACK(csize, 0x1));
        }
        //PUT(FTRP(bp), PACK(csize, 1));
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        if(next_alloc)
        {
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x3));
        }
        else
        {
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x2));
        }
        
        #ifdef DEBUG
            printf("place, cant got a smaller block\n");
        #endif
    }
    //printf("place return bp = %lx\n", bp);
    return bp;
}

/*
 * delete_from_list - delete a node from free list
 * two-way list, so it has 4 types of situation: pred = NULL or not, succ = NULL or not
 * have to specify to avoid the fking SEGV.
 * and delete of two-way list is O(1).
 * 
 * parameters: 
 * bp: the starting address of free block
 * 
 */
static void delete_from_list(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    /* the choosing header */
    void *SUCC_listp = choose_a_list(size);
    #ifdef DEBUG
        //mm_checkheap(__LINE__);
    #endif
    if(!PRED_BLK(bp) && !SUCC_BLK(bp)) /* only one block */
    {
        put_to_list(size, NULL, 0);     /* list is empty */
        #ifdef DEBUG
            printf("deletecase1 :\n");
            printf("delete a free block from list, only one block\n");
            printf("succ_listp = %lx\n", SUCC_listp);
        #endif
    }
    else if(PRED_BLK(bp) && !SUCC_BLK(bp))  /* last block */
    {
        SUCC_BLK((void *)FOUR_2_EIGHT(PRED_BLK(bp), HEAP_START)) = NULL;    /*remember to transfer the addr */
        PRED_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, last block\n");
        #endif
    }
    else if(!PRED_BLK(bp) && SUCC_BLK(bp))  /* first block */
    {
        put_to_list(size, FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START), 0);
        PRED_BLK(FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START)) = NULL;
        SUCC_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, first block\n");
        #endif
    }
    else if(PRED_BLK(bp) && SUCC_BLK(bp))
    {
        SUCC_BLK(FOUR_2_EIGHT(PRED_BLK(bp), HEAP_START)) = SUCC_BLK(bp);
        PRED_BLK(FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START)) = PRED_BLK(bp);
        SUCC_BLK(bp) = NULL;
        PRED_BLK(bp) = NULL;
        #ifdef DEBUG
            printf("delete a free block from list, normal block\n");
        #endif
    }
    
}

/*
 * sort_place - place a node in free list
 * find its proper place by walking through the list.
 * make sure its pred is smaller and its succ is bigger/even.
 * first-fit in sorted lists is best-fit.
 * (sacrifice some thru to get util)
 * 
 * have to discuss if the list is empty/if the succ is NULL to avoid SEGV
 * 
 * parameters: 
 * bp: the starting address of free block
 * 
 */
static void sort_place(void *bp)
{
    #ifdef DEBUG
        printf("5. sort_place: \n");
    #endif
    size_t size = GET_SIZE(HDRP(bp));
    void* pre = choose_a_list(size);//8bytes
    if(!pre) /* empty */
    {
        put_to_list(size, bp, 0);
        PRED_BLK(bp) = NULL;
        SUCC_BLK(bp) = NULL;
        return;
    }
    if(GET_SIZE(HDRP(pre)) >= size)/* only one node and it's bigger */
    {
        PRED_BLK(bp) = NULL;
        SUCC_BLK(bp) = pre;
        //printf("succ(bp) = %lx\n", SUCC_BLK(bp));
        put_to_list(size, EIGHT_2_FOUR(bp, HEAP_START), 1);
        //PRED_BLK(SUCC_listp) = bp;
        put_to_list(size, bp, 0);
        return;
    }

    void* nex = FOUR_2_EIGHT(SUCC_BLK(pre), HEAP_START);
    /* find the first bigger block */
    while((unsigned int)nex)
    {
        //mm_checkheap(__LINE__);
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
    //printf("pre = %lx\n", (unsigned long)(pre));
    //printf("nex = %lx\n", (unsigned long)(nex));
    if(!(unsigned int)nex)  /* is last */
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

/*
 * coalesce - coalesce the prev/next empty block
 * call delete_from_list to delete the "coalesced" blocks
 * lastly, call sort_place to place the new bigger block
 * 
 * parameters: 
 * bp: the starting address of free block
 * 
 */
static void *coalesce(void *bp) 
{
#ifdef DEBUG
    printf("6. Coalesce: ");
#endif
    //printf("bp = %lx\n", bp);
    size_t prev_alloc = GET_SECOND(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    //printf("prev_alloc = %lx\n", prev_alloc);
    //printf("next_alloc = %lx\n", next_alloc);
    if (prev_alloc && next_alloc) {            /* Case 1 */
        //printf("case1: \n");
        #ifdef DEBUG
            printf("cant coalesce\n");
        #endif
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_from_list(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0x2));
        PUT(FTRP(bp), PACK(size, 0));
        /* the next block need not change */
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

        /* update the header/footer, so fking hard to debug :(*/
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
        if(next_alloc)
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x1));
        else
            PUT(HDRP(NEXT_BLKP(bp)), PACK(next_size, 0x0));

        if(GET_SECOND(HDRP(PREV_BLKP(bp))))
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x2));
        else
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x0));
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
        /* update header/footer */
        if(GET_SECOND(HDRP(PREV_BLKP(bp))))
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x2));
        else
            PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x0));
        //PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        #ifdef DEBUG
            printf("coalesce both\n");
        #endif
    }
    //mm_checkheap(__LINE__);
    sort_place(bp); /* add the new bigger free block to correct free list */
    return bp;
}

/*
 * find_fit - choose a free block for allocte
 * first-fit(= best-fit)
 * parameters: 
 * bp: the starting address of free block
 * 
 */
static void *find_fit(size_t asize)
{
#ifdef DEBUG
    printf("8. Find_Fit: target = %d, ", asize);
    //mm_checkheap(__LINE__);
#endif
    /* First-fit search */
    size_t size = asize;
    void *bp = choose_a_list(asize);
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
                //mm_checkheap(__LINE__);
                return bp;
            }
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
        /* to find in the bigger list */
        size = size << 1;
        bp = choose_a_list(size);
    }
    /* size > 4097 */
    bp = SUCC_listp14;
    while((unsigned int)bp)
        {
            #ifdef DEBUG
                printf("the point of finding bp =%lx (after )\n",(unsigned long)(bp));
                printf("the SUCC of finding bp = %lx (after )\n",(unsigned long)(SUCC_BLK(bp)));
            #endif
            if(asize < GET_SIZE(HDRP(bp)))
            {
                #ifdef DEBUG
                    printf("find a fit block, size = %ld\n", asize);
                #endif
                return bp;
            }
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
    //mm_checkheap(__LINE__);
#ifdef DEBUG
    printf("no fit, extend a new block\n");
#endif
    return NULL; /* No fit */
}

/* print the info of list */
void printlist()
{
    size_t sz[20] = {0,1,2,4,8,16,32,64,128,256,512,1024,2048,4096,4097};
    for(int i=1; i<=14; i++)
    {
        void *bp = choose_a_list(sz[i]);
        while(EIGHT_2_FOUR(bp, HEAP_START))
        {
            printf("case%d\n", sz[i]);
            printf("bp = %lx\n", bp);
            printf("bp's size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("succ_blk(bp) = %lx\n", SUCC_BLK(bp));
            if(GET_SIZE(HDRP(bp)) > sz[i] && i <= 13)
            {
                printf("bp = %lx\n", bp);
                printf("size = %ld\n", GET_SIZE(HDRP(bp)));
                printf("next_second = %lx\n", GET_SECOND(HDRP(NEXT_BLKP(bp))));
                exit(1);
            }
            //printf("succ_ptr(bp) = %lx\n", FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START));
            bp = FOUR_2_EIGHT(SUCC_BLK(bp), HEAP_START);
        }
    }
}
/* print the info of heap */
void printheap()
{
    void* bp = heap_listp + 2*WSIZE;
    size_t pre_alloc = 0;
    while(GET_SIZE(HDRP(bp)) != 0)
    {
        #ifdef DEBUG
            printf("bp = %lx\n", bp);
            printf("alloc = %lx\n", GET_ALLOC(HDRP(bp)));
            printf("prev_alloc = %lx\n", GET_SECOND(HDRP(bp)));
            printf("size = %ld\n", GET_SIZE(HDRP(bp)));
        #endif
        pre_alloc = GET_ALLOC(HDRP(bp));
        bp = NEXT_BLKP(bp);
    } 
    printf("last bp = %lx\n", bp);
    printf("bp's size - %ld\n", GET_SIZE(HDRP(bp)));
    printf("bp's alloc = %lx\n", GET_ALLOC(HDRP(bp)));
    printf("bp's second = %lx\n\n", GET_SECOND(HDRP(bp)));
}

/* mm_checkheap - check the heap and freelist, for debugging
 * 
 * it check 80% content of what to check in writeup.
 * others I didn't use, and I didnt think is necesarry.
 * 1. check prologue
 * 2. check size aligned to 8
 * 3. check smallest size > 16
 * 4. check foot and header
 * 5. check second bit of header
 * 6. check epilogue
 * 7. check element in free list, check if it is in the correct list
 *  
 * parameters: lineno - the line where this function is called, 
 * which is passed by ____LINE_____.
 * 
 * 
 */ 
void mm_checkheap(int lineno)
{
    void* bp = heap_listp;
    if(GET_ALLOC(HDRP(bp)) != 1)
    {
        printf("no prelogue in %ld\n", lineno);
        exit(1);
    }
    size_t pre_alloc = -1;
    bp = NEXT_BLKP(bp);
    bp = NEXT_BLKP(bp);
    while(GET_SIZE(HDRP(bp)) != 0)
    {
        if((unsigned int)bp % 8)
        {
            printf("bp= %lx\n", bp);
            printf("size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("bp not aligned in %ld\n", lineno);
            exit(1);
        }
        if(GET_SIZE(HDRP(bp)) < 16)
        {
            printf("bp= %lx\n", bp);
            printf("size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("block too small in %ld\n", lineno);
            exit(1);
        }
        
        if(GET_ALLOC(HDRP(bp)) != GET_SECOND(HDRP(NEXT_BLKP(bp))))
        {
            printf("bp = %lx\n", bp);
            printf("size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("next_second = %lx\n", GET_SECOND(HDRP(NEXT_BLKP(bp))));
            printf("ft != hd in %ld\n", lineno);
            exit(1);
        }
        size_t alloc = GET_ALLOC(HDRP(bp));
        if(alloc == 0 && pre_alloc == 0)
        {
            printf("bp = %lx\n", bp);
            printf("size = %ld\n", GET_SIZE(HDRP(bp)));
            printf("next_second = %lx\n", GET_SECOND(HDRP(NEXT_BLKP(bp))));
            printf("no coalesce in %ld\n", lineno);
            exit(1);
        }
        pre_alloc = alloc;
        bp = NEXT_BLKP(bp);
    }
    if(GET_SIZE(HDRP(bp)) != 0)
    {
        printf("no epilogue in %ld\n", lineno);
    }
    #ifdef PRINT
        printf("printheap!\n");
        printheap();
        printf("printlist!\n");
        printlist();
    #endif
}