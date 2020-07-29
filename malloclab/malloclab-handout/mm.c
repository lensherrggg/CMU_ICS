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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


static void* heap_listp;
static int segregated_free_lists0;
static int segregated_free_lists1;
static int segregated_free_lists2;
static int segregated_free_lists3;
static int segregated_free_lists4;
static int segregated_free_lists5;
static int segregated_free_lists6;
static int segregated_free_lists7;
static int segregated_free_lists8;
static int segregated_free_lists9;
static int segregated_free_lists10;
static int segregated_free_lists11;
static int segregated_free_lists12;
static int segregated_free_lists13;
static int segregated_free_lists14;
static int segregated_free_lists15;

static void *segregated_free_lists(int index);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void insert_node(void *bp, size_t size);
static void delete_node(void *bp);
static void *place(void* bp, size_t asize); // 放置块并进行分割

/* 
 * segregated_free_lists - Fetch the identical linked list
 */
static void *segregated_free_lists(int index)
{
    switch(index) {
        CASE(0)  CASE(1)  CASE(2)  CASE(3)
        CASE(4)  CASE(5)  CASE(6)  CASE(7)
        CASE(8)  CASE(9)  CASE(10) CASE(11)
        CASE(12) CASE(13) CASE(14) CASE(15)
    }
    return NULL;
}

/* 
 * mm_init - Initialize the memory manager 
 */
int mm_init(void) 
{
    for (int i = 0; i < 16; i++) {
        PTR(segregated_free_lists(i), NULL);
    }

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE) == NULL) {
        return -1;
    }

    return 0;
}

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t size) 
{
    char *bp;
    size_t asize = ALIGN(size);

    if ((long)(bp = mem_sbrk(asize)) == -1) {
        return NULL;
    }             

    PUT(HDRP(bp), PACK(asize, 0));
    PUT(FTRP(bp), PACK(asize, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    insert_node(bp, asize);

    return coalesce(bp);
}

/*
 * insert_node - Find the correct place insert a free block. 
 */
static void insert_node(void *bp, size_t size)
{
    int index = 0;
    void *target_bp = NULL;
    void *prev_target_bp = NULL;
    int tempsize = size;

    /* find the list through the size of block */
    while (index < 15 && size > 1) {
        size >>= 1;
        index++;
    }

    size = tempsize;
    target_bp = (void *)*(unsigned int *)segregated_free_lists(index);
    while ((target_bp != NULL) && (size > GET_SIZE(HDRP(target_bp)))) {
        prev_target_bp = target_bp;
        target_bp = SUCC_BLKP(target_bp);
    }

    if (target_bp != NULL) {
        if (prev_target_bp != NULL) {
            /* [head]->...->prev_target_bp->bp->target_bp->... */
            PTR(PRED(bp), prev_target_bp);
            PTR(SUCC(prev_target_bp), bp);
            PTR(SUCC(bp), target_bp);
            PTR(PRED(target_bp), bp);
        }
        else {
            /* [head]=bp->target_bp->... */
            PTR(SUCC(bp), target_bp);
            PTR(PRED(target_bp), bp);
            PTR(PRED(bp), NULL);
            PTR(segregated_free_lists(index), bp);
        }
    }
    else {
        if (prev_target_bp != NULL) {
            /* insert at the end */
            PTR(PRED(bp), prev_target_bp);
            PTR(SUCC(prev_target_bp), bp);
            PTR(SUCC(bp), NULL);
        }
        else {
            PTR(PRED(bp), NULL);
            PTR(SUCC(bp), NULL);
            PTR(segregated_free_lists(index), bp);
        }
    }
}

/*
 * delete_node - Delete a block fron the segregated free list. 
 */
static void delete_node(void *bp) 
{
    if (bp == NULL) {
        return;
    }
    int index = 0;
    size_t size = GET_SIZE(HDRP(bp));
    while (index < 15 && size > 1) {
        size >>= 1;
        index++;
    }
    if (SUCC_BLKP(bp) != NULL) {
        /* Not the last */
        if (PRED_BLKP(bp) != NULL) {
            /* Not the head */
            PTR(PRED(SUCC_BLKP(bp)), PRED_BLKP(bp));
            PTR(SUCC(PRED_BLKP(bp)), SUCC_BLKP(bp));
        }
        else {
            PTR(PRED(SUCC_BLKP(bp)), NULL);
            PTR(segregated_free_lists(index), SUCC_BLKP(bp));
        }
    }
    else {
        if (PRED_BLKP(bp) != NULL) {
            PTR(SUCC(PRED_BLKP(bp)), NULL);
        }
        else {
            PTR(segregated_free_lists(index), NULL);
        }
    }
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size) 
{
    size_t asize;

    if (heap_listp == 0) {
        mm_init();
    }

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } 
    else {
        asize = ALIGN(size + DSIZE);
    }

    int index = 0;
    size_t target_size = asize;
    void *bp = NULL;

    while (index < 16) {
        if (target_size <= 1 && (void *)*(unsigned int*)(segregated_free_lists(index)) != NULL) {
            bp = (void *)*(unsigned int *)(segregated_free_lists(index));
            while (bp != NULL && asize > GET_SIZE(HDRP(bp))) {
                bp = SUCC_BLKP(bp);
            }
            if (bp != NULL) {
                break;
            }
        }
        target_size >>= 1;
        index++;
    }

    if (bp == NULL) {
        if ((bp = extend_heap(MAX(asize, CHUNKSIZE))) == NULL) {
            return NULL;
        }
    }

    bp = place(bp, asize);

    return bp;
} 

/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    insert_node(bp, size);
    coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert_node(bp, size);

    return bp;
}

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void *place(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));
    size_t left_size = block_size - asize;

    delete_node(bp);

    if (left_size < 2 * DSIZE) {
        PUT(HDRP(bp), PACK(block_size, 1));
        PUT(FTRP(bp), PACK(block_size, 1));
    }
    else if (asize >= 85) {
        PUT(HDRP(bp), PACK(left_size, 0));
        PUT(FTRP(bp), PACK(left_size, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        insert_node(bp, left_size);

        return NEXT_BLKP(bp);
    }
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(left_size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(left_size, 0));
        insert_node(NEXT_BLKP(bp), left_size);
    }

    return bp;
}

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    size_t asize;

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    }
    else {
        asize = ALIGN(size + DSIZE);
    }

    void *new_ptr = ptr;
    size_t available_size = 0;
    size_t total_size = 0;

    if (GET_SIZE(HDRP(ptr)) >= asize) {
        return ptr;
    }
    else if (!GET_SIZE(NEXT_BLKP(HDRP(ptr))) || (!GET_ALLOC(NEXT_BLKP(HDRP(ptr))) && !GET_SIZE(NEXT_BLKP(HDRP(ptr))))) {
        available_size = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        total_size = available_size;
        if (available_size < asize) {
            if (extend_heap(MAX(asize - available_size, CHUNKSIZE)) == NULL) {
                return NULL;
            }
            total_size = available_size + MAX(asize - available_size, CHUNKSIZE);
        }
        delete_node(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
    }
    else if (!GET_ALLOC(NEXT_BLKP(HDRP(ptr))) && (total_size = (GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr)))) >= asize)) {
        delete_node(NEXT_BLKP(ptr));
        PUT(HDRP(ptr), PACK(total_size, 1));
        PUT(FTRP(ptr), PACK(total_size, 1));
    }
    else {
        new_ptr = mm_malloc(asize);
        memcpy(new_ptr, ptr, GET_SIZE(HDRP(ptr)));
        mm_free(ptr);
    }

    return new_ptr;
}
