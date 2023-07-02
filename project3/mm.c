/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20180223",
    /* Your full name*/
    "Haejin Lim",
    /* Your email address */
    "hj0816hj@sogang.ac.kr",
};

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp

/* Given block ptr bp, set, compute address of next and previous free blocks */
#define SET_NEXT_FREE(bp, next) (*(int *)(bp) = (next))
#define SET_PREV_FREE(bp, prev) (*(int *)(bp+WSIZE) = (prev))

#define NEXT_FREE(bp) (*(int *)(bp))
#define PREV_FREE(bp) (*(int *)(bp+WSIZE))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */  
static char *free_listp = 0;  /* Pointer to first free block */ 

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void push_free_list(void *bp);
static void remove_free_list(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    free_listp = 0;

    if ((heap_listp = mem_sbrk(4*WSIZE-mem_heapsize())) == (void *)-1)
        return -1;
    
     /* Create the initial empty heap */
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);                    

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
        //if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      
    
    /* Ignore spurious requests */
    if (size == 0) return NULL;

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
    return bp;
}

/*
 * mm_free - Freeing a block
 */
void mm_free(void *ptr)
{
    if (ptr == 0) 
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    ptr = coalesce(ptr);
    push_free_list(ptr);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */    
        remove_free_list(NEXT_BLKP(bp));
        
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));        
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        remove_free_list(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
        remove_free_list(NEXT_BLKP(bp));
        remove_free_list(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
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

    size_t asize;      /* Adjusted block size */
    if (size <= DSIZE)                                          
        asize = 2*DSIZE;                                        
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    // if current block is available for realloc
    if(GET_SIZE(HDRP(ptr))>=asize) {
        place(ptr, asize);
        return ptr;
    }

    // if coalesce with next block if available
    if(!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) && GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= asize){
        remove_free_list(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr))), 1));
        place(ptr, asize);
        return ptr;
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) return 0;

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                       

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* Coalesce if the previous block was free */
    bp = coalesce(bp);
    push_free_list(bp);
    return bp;                                          
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    void *nbp;
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {
        remove_free_list(bp);
    
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        nbp = NEXT_BLKP(bp);
        PUT(HDRP(nbp), PACK(csize-asize, 0));
        PUT(FTRP(nbp), PACK(csize-asize, 0));
        push_free_list(nbp);

    }
    else {
        remove_free_list(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
    /* Best-fit search */
    void *bp, *min_remain=NULL, *min_16_remain=NULL, *max_16_remain=NULL;

    for (bp = free_listp; bp!=NULL ; bp = NEXT_FREE(bp)) {

        if (asize == GET_SIZE(HDRP(bp))) {
            return bp;
        }
        else if (asize < GET_SIZE(HDRP(bp))){
            if (!min_remain || GET_SIZE(HDRP(bp))<GET_SIZE(HDRP(min_remain))) min_remain = bp;
            if(GET_SIZE(HDRP(bp))-asize >= 2*DSIZE){
                if (!min_16_remain || GET_SIZE(HDRP(bp))<GET_SIZE(HDRP(min_16_remain))) min_16_remain = bp;
                if (!max_16_remain || GET_SIZE(HDRP(bp))>GET_SIZE(HDRP(max_16_remain))) max_16_remain = bp;
            }
        }
    }

    if(min_remain && GET_SIZE(HDRP(min_remain))-asize<=WSIZE) return min_remain;
    if(max_16_remain && asize*3<=GET_SIZE(HDRP(max_16_remain))) return max_16_remain;
    if(min_16_remain) return min_16_remain;
    if(min_remain) return min_remain;

    return NULL; /* No fit */
}

static void push_free_list(void *bp){    
    if(free_listp) SET_PREV_FREE(free_listp, bp);
    SET_NEXT_FREE(bp, free_listp);
    SET_PREV_FREE(bp, NULL);
    free_listp = bp;
}

static void remove_free_list(void *bp){
    if(GET_ALLOC(HDRP(bp))) return; // if bp is not a free block, cannot remove

    if(PREV_FREE(bp)) SET_NEXT_FREE(PREV_FREE(bp), NEXT_FREE(bp));
    else free_listp = NEXT_FREE(bp); // bp was first free block

    if(NEXT_FREE(bp)) SET_PREV_FREE(NEXT_FREE(bp), PREV_FREE(bp));
}