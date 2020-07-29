#include "cachelab.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>

typedef struct {
    int valid;
    int tag;
    int LRUcounter; // record the time that a block is used
} Line;

typedef struct {
    Line * lines;
} Set;

typedef struct {
    int s;
    int E;
    int b;
    Set * sets;
} Cache;

typedef struct {
    int hit;
    int miss;
    int eviction;
} Result;

Cache initializeCache(int s, int E, int b) {
    Cache newCache;
    newCache.s = s;
    newCache.E = E;
    newCache.b = b;
    int S = 1 << s;
    newCache.sets = (Set *) malloc(S * sizeof(Set));
    if (newCache.sets == NULL) {
        perror("failed to create sets in the cache!");
    }
    for (int i = 0; i < S; i++) {
        newCache.sets[i].lines = (Line *)malloc(E * sizeof(Line));
        if (newCache.sets[i].lines == NULL) {
            perror("failed to create lines in sets!");
        }
    }

    return newCache;
}

void release(Cache cache) {
    int S = 1 << cache.s;
    for (int i = 0; i < S; i++) {
        free(cache.sets[i].lines);
    }
    free(cache.sets);
}

/* 
 *  @targetSet    The target set we get according to the input command
 *  @tag          The tag bits
 *  @E            The number of lines in a set
 *  @results      The variable that we use to record the number of hits, misses and evictions
 *  @flag         Indicate whether or not we should print the trace info
 *  @flag2        Indicate whether or not we should put a line break at the end of the trace info string
 */
Result testHit(Set targetSet, int tag, int E, Result results, int flag, int flag2) {
    int hitflag = 0;
    for (int i = 0; i < E; i++) {
        if (targetSet.lines[i].tag == tag && targetSet.lines[i].valid == 1) {
            if (flag) {
                printf("hit ");
            }
            hitflag = 1;
            results.hit++;
            targetSet.lines[i].LRUcounter++;
            break;
        }
    }
    if (!hitflag) {
        if (flag) {
            printf("miss ");
        }
        results.miss++;
        int maxCounter = targetSet.lines[0].LRUcounter;
        int minCounter = targetSet.lines[0].LRUcounter;
        int lineIndex = 0;
        for (int i = 0; i < E; i++) {
            if (targetSet.lines[i].LRUcounter > maxCounter) {
                maxCounter = targetSet.lines[i].LRUcounter;
            }
            if (targetSet.lines[i].LRUcounter < minCounter) {
                minCounter = targetSet.lines[i].LRUcounter;
                lineIndex = i;
            }
        }
        targetSet.lines[lineIndex].LRUcounter = maxCounter + 1;
        targetSet.lines[lineIndex].tag = tag;
        if (targetSet.lines[lineIndex].valid) {
            if (flag) {
                printf("eviction ");
            }
            results.eviction++;
        } else {
            targetSet.lines[lineIndex].valid = 1;
        }
    }
    if (flag && !flag2) {
        printf("\n");
    }
    return results;
}

/*
 *  @fp      The file we should read indicated in the input command after -t
 *  @cache   The simulated cache
 *  @flag    Indicate whether or not we should print the trace info
 */
Result test(FILE* fp, Cache cache, int flag) {
    Result results = {0, 0, 0};
    char ch;
    long unsigned int address;
    int size, b = cache.b, s = cache.s, E = cache.E, S = 1 << s;
    while ((fscanf(fp, "%c %lx,%d[^\n]", &ch, &address, &size)) != -1) {
        if (ch == 'I') {
            continue;
        } else {
            int setIndex = (address >> b) & (S - 1);
            int tag = (address >> b) >> s;
            Set targetSet = cache.sets[setIndex];
            if (ch == 'L' || ch == 'S') {
                if (flag) {
                    printf("%c %lx,%d ", ch, address, size);
                }
                results = testHit(targetSet, tag, E, results, flag, 0);
            } else if (ch == 'M') {
                if (flag) {
                    printf("%c %lx,%d ", ch, address, size);
                }
                results = testHit(targetSet, tag, E, results, flag, 1);
                results = testHit(targetSet, tag, E, results, flag, 0);
            } else {
                continue;
            }
        }
    }

    return results;
}

int main(int argc, char *argv[]) {
    Result results = {0, 0, 0};
    FILE* fp = NULL;
    Cache simCache = {0, 0, 0, NULL};
    int s = 0, b = 0, E = 0, flag = 0;
    const char *command_options = "hvs:E:b:t:";
    char ch;
    while ((ch = getopt(argc, argv, command_options)) != -1) {
        switch(ch) {
            case 'h':
                printf(\
"Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n\
• -h: Optional help flag that prints usage info\n\
• -v: Optional verbose flag that displays trace info\n\
• -s <s>: Number of set index bits (S = 2s\n\
is the number of sets)\n\
• -E <E>: Associativity (number of lines per set)\n\
• -b <b>: Number of block bits (B = 2b\n\
is the block size)\n\
• -t <tracefile>: Name of the valgrind trace to replay\n");
                exit(EXIT_SUCCESS);
            case 'v':
                flag = 1;
                break;
            case 's':
                s = atol(optarg);
                break;
            case 'E':
                E = atol(optarg);
                break;
            case 'b':
                b = atol(optarg);
                break;
            case 't':
                if ((fp = fopen((const char *)optarg, "r")) == NULL) {
                    perror("Failed to open tracefile!");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (s == 0 || E == 0 || b == 0 || fp == NULL) {
        printf(\
"Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n\
• -h: Optional help flag that prints usage info\n\
• -v: Optional verbose flag that displays trace info\n\
• -s <s>: Number of set index bits (S = 2^s is the number of sets)\n\
• -E <E>: Associativity (number of lines per set)\n\
• -b <b>: Number of block bits (B = 2^b is the block size)\n\
• -t <tracefile>: Name of the valgrind trace to replay\n");
        exit(EXIT_FAILURE);
    }
    simCache = initializeCache(s, E, b);
    results = test(fp, simCache, flag);
    
    printSummary(results.hit, results.miss, results.eviction);
    release(simCache);
    return 0;
}
