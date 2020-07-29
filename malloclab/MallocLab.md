# Malloc Lab

## 实验内容

要求自己设计动态内存分配器，实现malloc、free、realloc功能。实验采用空间利用率和效率两个指标评价动态内存分配器的性能，需要找到两者之间的平衡。

## 实验步骤

### 思路和准备

实验前仔细研读教材9.9动态内存分配的内容，尤其是9.9.12给出的简单的动态内存分配器的实现。

本次实验采用分离的空闲链表管理空闲块，分配方式选择分离适配，这种情况下首次适配搜索空间利用率接近最优适配搜索，还能保持不错的效率。

作业指导中要求“You are not allowed to define any global or static compound data structures such as arrays, structs, trees, or lists in your mm.c program. However, you are allowed to declare global scalar variables such as integers, floats, and pointers in mm.c”可以采用定义若干个全局int变量，强制转换成指针作为个空闲链表的头指针。

在mm.h中定义宏变量

```c
#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))
#define GET_ADDRESS(p) (*(void **)(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

/* Given free block ptr bp, compute ptr to address of its predessor and successor */
#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* 获取有效字节，即总的size - 头尾指针 */
#define GET_PAYLOAD(bp) (GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

```

首先按照教材给出的方案完成各个函数

#### mm_init

```c
/* 
 * mm_init - Initialize the memory manager 
 */

int mm_init(void) 
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);                      

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) 
        return -1;
    return 0;
}
```

这里的注释很清楚，同时每一句我们也可以在上图的allocator看到对应结构，比如第一个WSIZE的padding部分，序言块的 header 和 footer，以及特殊的结尾块。包括我们将heap_listp 更新，指向序言块的 footer 部分（实际上我们这里也可以理解为它指向序言块的payload部分，也就是0），同时我们用到extendheap将heap 默认快到 1<<12 / WSIZE 的大小。

####extend_heap

```c
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
    return coalesce(bp);                                          
}
```

extend_heap 是以 words 为单位，所以之前我们传递的是 CHUNKSIZE/WSIZE 的，然后这里也体现出了结尾块的作用。回到图中，因为我们有结尾块，它是一个WSIZE但是PACK(0,1)的块，我们使用sbrk分配了一个双字节对齐的块之后，bp是指向我们分配的地址的开端，但是因为有了这个特殊的结尾块，它可以变成新的空闲块的头部，这样保证我们的bp还是指向payload处，而我们的 FTRP(bp) 也是刚好，原本的 bp + 整块大小，那是结束处，然后我们减去 DSIZE，两个WSIZE的，一部分做此 block 的 footer，另一做下一个block的header，下一block的header刚好就可以特殊化为新的结束块。这里也就体现了书上说的“结尾块是一种消除合并时边界条件的技巧。”同时检查是否会有前一块为空闲块的状况，如有，则需要合并。

####mm_free

```c
/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp)
{
    if (bp == 0) 
        return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}
```

mm_free 非常显而易见，找到这个块的size，然后把它标记为未分配，同时检查看是否需要合并。

#### coalesce

```c
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
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}
```

coalesce 这个函数也非常明显的对应书上的4种状况：

- case 2 是后面一块free，那么我们改变size之后，对应的FTRP也变了，这一句会把整个一大块全变成free
- case 3 和 case 4 也是类似case 2需要合并，不过同时bp也改变，指向前面的free block
- 这四种情况，同时体现出了‘书上所说的序言块和结尾块标记为分配允许我们忽略潜在的麻烦边界情况’，的确，试想一下如果序言块和结尾块并不是这样做，那么我们每次必须要考虑特殊状况，比如如果在头或者尾要怎么处理

####mm_malloc

```c
/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size) 
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

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
    return bp;
} 
```

最小块的大小是16字节： 8字节用来满足对齐要求，而另外8个字节用来放头部和脚部。对于超过8字节的请求，一般的规则是加上开销字节，然后向上摄入到最接近8的整数倍。

#### find_fit和place

```c
/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{

    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */

}
```

```c
/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   

    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
```

place 会计算整个空闲块的大小，如果依旧有剩余空间会再分割空闲块，否则会直接就把这一块给 mm_malloc 用。

#### mm_realloc

```c
/*
 * mm_realloc - Naive implementation of realloc
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

    return newptr;
}
```

#### helper functions

```c
/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose) {
            printblock(bp);
        }
        checkblock(bp);
    }

    if (verbose) {
        printblock(bp);
    }
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}
```

结果：

```shell
$ ./mdriver -a -g -v -t traces/
Using default tracefiles in traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.008934   637
 1       yes   99%    5848  0.007700   759
 2       yes   99%    6648  0.015755   422
 3       yes   99%    5380  0.013285   405
 4       yes   66%   14400  0.000091159116
 5       yes   93%    4800  0.006061   792
 6       yes   92%    4800  0.005888   815
 7       yes   55%    6000  0.029426   204
 8       yes   51%    7200  0.024637   292
 9       yes   27%   14401  0.062841   229
10       yes   34%   14401  0.002272  6337
Total          74%   89572  0.176890   506

Perf index = 44 (util) + 34 (thru) = 78/100
correct:11
perfidx:78
```

完整代码：

mm.h

```c
#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))
#define GET_ADDRESS(p) (*(void **)(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

/* Given free block ptr bp, compute ptr to address of its predessor and successor */
#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* 获取有效字节，即总的size - 头尾指针 */
#define GET_PAYLOAD(bp) (GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

```

mm.c

```c
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

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size); // 找到合适的块
static void place(void* bp, size_t asize); // 放置块并进行分割
static void placefree(void* bp); // 将空闲链表放置在链表开头
static void deletefree(void* bp); // 删除空闲节点
static void *recoalesce(void *bp, size_t needsize); // 针对realloc实现的coalesce函数
static int findlink(size_t size); // 寻找对应下标


/* 
 * mm_init - Initialize the memory manager 
 */
int mm_init(void) 
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }

    return 0;
}


/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);                                        
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size) 
{
    size_t asize;
    size_t extendsize;
    char *bp;

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
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }

    place(bp, asize);
    return bp;
} 

/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp)
{
    if (bp == 0) {
        return;
    }

    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

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
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else {
        size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    return bp;
}

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) {
            return bp;
        }
    }

    return NULL;
}


/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) > 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    if (size == 0) {
        mm_free(ptr);
        return 0;
    }

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);
    if (!newptr) {
        return 0;
    }
    oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize) {
        oldsize = size;
    }

    memcpy(newptr, ptr, oldsize);

    mm_free(ptr);

    return newptr;
}

/*
 * mm_checkheap - Check the heap for correctness
 */
void mm_checkheap(int verbose)
{
    checkheap(verbose);
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose) {
            printblock(bp);
        }
        checkblock(bp);
    }

    if (verbose) {
        printblock(bp);
    }
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}
```





对于速度（thru）而言，我们需要关注malloc、free、realloc每次操作的复杂度。对于内存利用率（util）而言，我们需要关注*internal fragmentation* （块内损失）和 *external fragmentation* （块是分散不连续的，无法整体利用），即我们free和malloc的时候要注意整体大块利用（例如合并free块、realloc的时候判断下一个块是否空闲）。

采用9.9.13和9.9.14的Explicit Free Lists + Segregated Free Lists + Segregated Fits进行优化

块的结构如下，其中低三位由于内存对齐的原因总会是0，A代表最低位为1，即该块已经allocated：

<img src="/Users/rui/Library/Application Support/typora-user-images/image-20200726112614473.png" alt="image-20200726112614473" style="zoom:50%;" />

堆的起始和结束结构如下：

![image-20200726112643394](/Users/rui/Library/Application Support/typora-user-images/image-20200726112643394.png)

Free list的结构如下，每条链上的块按大小由小到大排列，这样我们用“first hit”策略搜索链表的时候就能获得“best hit”的性能，例如第一条链，A是B的successor，B是A的predecessor，A的大小小于等于B；不同链以块大小区分，依次为{1}{2}{3~4}{5~8}...{1025~2048}... ：

![image-20200726112710738](/Users/rui/Library/Application Support/typora-user-images/image-20200726112710738.png)

以下是具体实现

进行宏定义

```c
#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))
#define GET_ADDRESS(p) (*(void **)(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

/* Given free block ptr bp, compute ptr to address of its predessor and successor */
#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* 获取有效字节，即总的size - 头尾指针 */
#define GET_PAYLOAD(bp) (GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Modify ptr by pointer address */
#define PTR(p,ptr) (*(unsigned int *)(p)=(unsigned int)(ptr))

/* Get the predecessor and sucessor pointer by the free block pointer */
#define PRED_BLKP(bp) (*(char **)(bp))
#define SUCC_BLKP(bp) (*(char **)(bp + WSIZE))

/* Select a segregated_free_list */
#define CASE(i) case i: return &segregated_free_lists##i;
```

定义变量和函数

```c
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
static void *find_fit(size_t size); // 找到合适的块
static void *place(void* bp, size_t asize); // 放置块并进行分割
```

首先是取相应的链表

```c
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

```

这样定义后以后要将更新后的指针ptr写入segregated_free_listi 只需要写：

```c
PTR(segregated_free_lists(i),ptr);
```

要读取segregated_free_listsi的值只需要写：

```c
(void *)*(unsigned int *)segregated_free_lists(i)
```

#### extend_heap

```c
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
```

#### insert_node

先通过size找到正确的链表。由于链表以2的幂次分类，因此只需要对size>>1即可找到正确的链表。然后在链表中找到应该插入的位置，`prev_target_bp`和`target_bp`之间。共有四种可能的情况：

1. `prev_target_bp`和`target_bp`均不是头和尾
2. `prev_target_bp`是链表头，`target_bp`不是链表尾
3. `prev_target_bp`不是链表头，`target_bp`是链表尾
4. `prev_target_bp`是链表头，`target_bp`是链表尾，即链表尾空。

```c
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

```

#### delete_node

有四种情况：

1. 要删除的块不是头也不是尾
2. 要删除的块是头，不是尾
3. 要删除的块不是头，是尾
4. 要删除的块既是头也是尾，即该链表仅一个块。

由于delete_node函数的形参是指向空闲块的指针bp，需要用PREV_BLKP(bp)和NEXT_BLKP(bp)得到前驱和后继的情况，而不是PRED(bp)和SUCC(bp)，否则将出现Segmentation fault。

```c
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

```

#### coalesce

通过free或者分割产生了一个空闲块时，需要判断它能不能与其前后的块进行合并。共4种情况。教材P569有详细说明，P601有代码示例。

需要注意合并后需要把该空闲块和被合并的块从各自链表中删除，把合并后的块加入正确的链表。代码如下

```c
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

```

一定要先delete再进行合并。如果按照合并后的bp去delete会由于size发生变化而删除错误的块。

#### place

place函数需要将请求放在空闲块中，当空闲块剩下的空间>=最小块(2 * DSIZE)时才将剩下的空间分割出来插入空闲链表。为减少外部碎片，放置块的策略采用大小分开方法，即较小块从空闲块起始位置开始分配，较大块从另一端分配，这样连续free小块或大块时能够合并成较大的空闲块。根据实验结果将大小快分界定位85bytes。

```c
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

```

#### mm_init

```c
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

```



#### mm_malloc

关键点在于定位到刚好可能有合适空闲块的链表，然后从小到大一次寻找，选取首次找到的块为合适的。如果该链表没有合适的块则在系啊一个链表中寻找。若遍历完没找到合适的块则申请新的堆空间，最后放置在合适的空闲块中。

```c
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

```

#### mm_free

```c
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

```

#### mm_realloc

mm_realloc的要求如下：

1. 如果ptr为NULL相当于mm_malloc(size)

2. 如果size为0则相当于使用mm_free(ptr)

3. 如果ptr补位NULL，根据原来块的大小和要求的size做出反应。如果size小于块大小则返回原来的块，若size大于块大小则想办法找到合适的空间返回新块

   找到合适空间的策略如下：

   1. 若该块之后是结尾块，则直接申请新的堆空间，与原来的块组成新块然后返回

   2. 如果该块后面是一个空闲块，且两块相加大于size，则把两块合成新块然后返回
   3. 若该块之后是一个空闲块，且两块相加小于size，且空闲块后就是结尾块，则直接申请新的堆空间，与原来的两块组成新块然后返回
   4. 其他情况mm_alloc新块，把原来块的内容复制过去

```c
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

```

结果：

```shell
$ ./mdriver -a -g -v -t traces/
Using default tracefiles in traces/
Measuring performance with gettimeofday().

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000715  7964
 1       yes   99%    5848  0.000583 10031
 2       yes   99%    6648  0.000808  8228
 3       yes   99%    5380  0.001130  4763
 4       yes   66%   14400  0.000532 27057
 5       yes   96%    4800  0.000923  5203
 6       yes   95%    4800  0.000796  6033
 7       yes   94%    6000  0.000275 21850
 8       yes   88%    7200  0.000408 17656
 9       yes   99%   14401  0.000323 44613
10       yes   86%   14401  0.000302 47701
Total          93%   89572  0.006793 13186

Perf index = 56 (util) + 40 (thru) = 96/100
correct:11
perfidx:96
```



完整代码：

```c
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

```

```c
#include <stdio.h>

extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */

#define MAX(x,y) ((x)>(y)?(x):(y))

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size)|(alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(val))
#define GET_ADDRESS(p) (*(void **)(p))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

/* Given free block ptr bp, compute ptr to address of its predessor and successor */
#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* 获取有效字节，即总的size - 头尾指针 */
#define GET_PAYLOAD(bp) (GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Modify ptr by pointer address */
#define PTR(p,ptr) (*(unsigned int *)(p)=(unsigned int)(ptr))

/* Get the predecessor and sucessor pointer by the free block pointer */
#define PRED_BLKP(bp) (*(char **)(bp))
#define SUCC_BLKP(bp) (*(char **)(bp + WSIZE))

/* Select a segregated_free_list */
#define CASE(i) case i: return &segregated_free_lists##i;
```

