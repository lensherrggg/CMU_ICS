# Cache Lab

## Part A

### 概述

根据实验要求需要完成csim.c文件，编译后能够实现如下功能：

```
Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>
• -h: Optional help flag that prints usage info
• -v: Optional verbose flag that displays trace info
• -s <s>: Number of set index bits (S = 2s
is the number of sets)
• -E <E>: Associativity (number of lines per set)
• -b <b>: Number of block bits (B = 2b
is the block size)
• -t <tracefile>: Name of the valgrind trace to replay
```

即要求能够手动实现设置cache的set数、line数及block大小，读取指定文件内的指令进行操作，指令内容例如：

```
I 0400d7d4,8
 M 0421c7f0,4
 L 04f6b868,8
 S 7ff0005c8,8
```

每行第一个字母代表操作，实验要求中如此表述： “I” denotes an instruction load, “L” a data load, “S” a data store, and “M” a data modify (i.e., a data load followed by a data store). 第二个数字代表一个16位地址，第三个数字表示该操作要对多少byte的内容进行，模拟中第三个数字没有实质性作用

### simulated-cache的数据结构

本实验不涉及真实的数据读写，因此无需考虑block细节，只需认为每个line有一个block。由于采用LRU策略进行block的evict，因此需要记录该line的被使用时间。line的数据结构如下：

```c
typedef struct {
    int valid;
    int tag;
    int LRUcounter; // record the time that a block is used
} Line;
```

各个变量默认设为0，其中LRUcounter记录该line的被使用时间，每被使用一次LRUcounter++，因此LRUcounter越大表明该Line最近被使用。

Set和Cache的结构相对简单

```c
typedef struct {
    struct Line * lines;
} Set;

typedef struct {
    int s;
    int E;
    int b;
    struct Set * sets;
} Cache;
```

其中Set包含一个指向Line数组的指针，Cache包含输入的s、b以及每个Set中Line数目E和指向一个Set数组的指针。这是为了得到Cache即得到s、E、b以减少函数的形参数目。

最后定义了Result结构体存放hit、miss和eviction数目

```c
struct Result {
    int hit;
    int miss;
    int eviction;
};
```

### 初始化simulated-cache和清除simulated-cache

使用malloc函数申请Set数组，首地址赋给cache.Set，然后对于第i个Set即cache.sets[i]，使用malloc申请Line数组，首地址给cache.sets[i].lines

代码如下：

```c
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
```

程序退出前释放malloc的空间，需要的函数如下：

```c
void release(Cache cache) {
    int S = 1 << cache.s;
    for (int i = 0; i < S; i++) {
        free(cache.sets[i].lines);
    }
  	free(cache.sets);
}
```

### main函数

main函数中需要实现从命令行读取h、v、s、E、b、t并相应地进行赋值，因此需要用到getopt函数，其原型为

```c
int getopt(int argc, char * const argv[], const char * optstring);
```

前两个参数为main函数传入的参数，即输入的命令行，第三个参数为optstring——选项字符串，标识哪些字母表示了操作，如"a : b : cd::e"（去掉空格），字母后带一个冒号（如a、b）表明该操作带参数，字母后的内容需要读取，存到内部变量extern char * optarg中，参数可以和选项连在一起也可用空格隔开，如`-a123`和`-a 123`都正确，不带冒号（如c、e）表示该操作不带参数，后面输入的内容仍看做操作符处理。带两个冒号（d）表示操作后参数可选，但如果带参数则参数与操作符不能有空格，如写作`-d123`正确，`-d 123`报错。读取了全部输入的命令后getopt()返回-1，使用getopt需要包含头文件

```c
#include <unistd.h>
#include <getopt.h>
```

这样，就可以按要求读取命令行，main代码如下：

```c
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
                printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n\
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
        printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n\
                • -h: Optional help flag that prints usage info\n\
                • -v: Optional verbose flag that displays trace info\n\
                • -s <s>: Number of set index bits (S = 2s\n\
                is the number of sets)\n\
                • -E <E>: Associativity (number of lines per set)\n\
                • -b <b>: Number of block bits (B = 2b\n\
                is the block size)\n\
                • -t <tracefile>: Name of the valgrind trace to replay\n");
        exit(EXIT_FAILURE);
    }
    simCache = initializeCache(s, E, b);
    // results = Test(fp, simCache, flag);
    
    printSummary(0, 0, 0);
    release(simCache);
    return 0;
}
```

其中atol是将ch的ascii转成long，-h是显示帮助信息，参数不全也将显示帮助信息。

### Result的产生

至此来到核心部分：Result的产生。先分析每个输入的指令应当被如何操作。若为I则不是数据操作，忽略；若为L或S，则需要进行一次hit-miss-eviction检测；如果为M，则相当于先L再S，需要进行两次hit-miss-eviction检测。

考虑hit-miss-eviction检测细节。首先需要对读取的地址进行分析。地址结构如下：

<img src="/Users/rui/Library/Application Support/typora-user-images/image-20200719094107682.png" alt="image-20200719094107682" style="zoom:50%;" />

低b位表示偏移量，本实验无需计算block便宜。中间s位用于确定对哪个Set进行操作，其余为tag位，用于在该Set中匹配对应的Line。如果匹配不上说明该数据不在Line中。从地址中得到set index和tag的方法如下：

```c
int setIndex = (address >> b) & (S - 1);
int tag = (address >> b) >> s;
```

因此Result框架如下：

```c
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
                    printf("%c %lx %d ", ch, address, size);
                }
                results = testHit(targetSet, tag, E, results, flag, 0);
            } else if (ch == 'M') {
                if (flag) {
                    printf("%c %lx %d ", ch, address, size);
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
```

其中`fscanf(fp, "%c %lx,%d[^\n]", &ch, &address, &size)`函数用于读取指令中的操作符、地址、被操作对象大小，分别存储在`ch` 、`address`、`size`中。`[^\n]`表示换行符不读取。

接下来是hit-miss-eviction检测部分，需要先检测是否hit，即value==1和tag匹配，Result.hit++，并且将该Line的LRUcounter++，表示最近访问过。如果hit了则退出检测，否则无论需不需要evict都要Result.miss++，然后遍历Set中的Line找到最近使用过的LRUcountermax和距离上次使用时间最长的LRUcountermin。对LRUcountermin的Line进行操作，若valid==0则直接将地址中tag赋给Line.tag，valid=1，Linecounter=LRUcountermax+1，表明是最近使用的，退出；如果valid==1则需要将它evict，Result.eviction++，将地址中的tag赋给Line.tag，Line.LRUcounter=LRUcountermax+1，代码如下：

```c
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
```

flag2是为了-v显示详细步骤是和给的csim-ref一致而加入的参数

Part A完整代码如下：

```c
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
                    printf("%c %lx %d ", ch, address, size);
                }
                results = testHit(targetSet, tag, E, results, flag, 0);
            } else if (ch == 'M') {
                if (flag) {
                    printf("%c %lx %d ", ch, address, size);
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
```



## Part B

### 概述

这部分需要写一个转置函数，传统的转置函数给了如下案例

```c
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
```

这种形式虽然正确但是会导致大量的不命中。

因此需要重写一个转置函数以尽可能少得触发不命中。由于涉及数据读写，需要考虑block。

最多使用12个变量。不得使用位运算，不得使用数组或malloc。不得改变原始数组A但可以改变转置数组B。

题中所给的高速缓存的架构是(s=5, E=1, b=5)，即Cache有32个Set，每个Set有1行Line，Line中的block大小为32byte，即每行Line可以存放8个int，整个Cache一共可以存放256个int。

想要降低不命中次数，需要提高函数的局部性，要么通过修改循环顺序改进空间局部性，要么通过分块技术提高时间局部性。以上文给出的最简单的算法为例

```c
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
```

该函数矩阵A每次循环的步长为1，空间局部性良好，B的步长为N，空间局部性较差。需要通过分块技术优化时间局部性。由于每个块大小为32byte可以存放8个元素，矩阵是行优先存储，因此相当于保存了`A[0][0]~A[0][7]`，我们希望能够充分利用该数据块，因此需要保存对应的`B[0][0]~B[7][0]`。以32x32矩阵为例，由于32x32矩阵中每行32个元素，则相邻两行之间间隔了3个高速缓存行。比如根据B的地址，元素保存在高速缓存中为如下形式：

| 组号 | 元素                |
| ---- | ------------------- |
| 0    | `B[0][0]~B[0][7]`   |
| 1    | `B[0][8]~B[0][15]`  |
| 2    | `B[0][16]~B[0][23]` |
| 3    | `B[0][24]~B[0][31]` |
| 4    | `B[1][0]~B[1][7]`   |
| 5    | `B[1][8]~B[1][15]`  |
| ...  | ...                 |

可以发现想要的`B[0][0]~B[0][7]`,`B[1][0]~B[1][7]`之间隔了3个高速缓存行。该高速缓存配置刚好能够保存8行，因此设置分块技术的块大小为8，此时高速缓存中就保存了`B[0][0]~B[0][7]`到`B[7][0]~B[7][7]`的块。则在内层循环中能够充分利用这些块再将其丢弃减少原始代码中由于缓存空间有限而被迫丢弃之后要用的块。

### 矩阵乘法的分块

对矩阵乘法进行分块，我们需要将这些矩阵分成多个子矩阵，然后利用数学原理，将子矩阵之间的运算变成标量（scalar）的运算。

比如，我们想要计算 `C = AB`，其中`A`，`B` ，`C` 是 8 × 8 的矩阵。然后我们将每个矩阵分割成 4 × 4 的子矩阵：

$$ \left[ \begin{matrix} C_{11} & C_{12} \\ C_{21} & C_{22} \end{matrix} \right] = \left[ \begin{matrix} A_{11} & A_{12} \\ A_{21} & A_{22} \end{matrix} \right] \left[ \begin{matrix} B_{11} & B_{12} \\ B_{21} & B_{22} \end{matrix} \right]  $$

其中

$$ C_{11} = A_{11}B_{11} + A_{12}B_{21} $$

$$ C_{12} = A_{11}B_{12} + A_{12}B_{22} $$

$$ C_{21} = A_{21}B_{11} + A_{22}B_{21} $$

$$ C_{22} = A_{21}B_{12} + A_{22}B_{22} $$

这个代码背后的基本思想是将A，C分成 ![[公式]](https://www.zhihu.com/equation?tex=1%5Ctimes+bsize) 的 行片段（row slivers），并把B 分成![[公式]](https://www.zhihu.com/equation?tex=bsize%5Ctimes+bsize) 的块。

1. 首先，最内部的 ![[公式]](https://www.zhihu.com/equation?tex=%28j%2Ck%29) 循环（就是最深和次深的两个循环），将A的行片段乘上B的块，然后将求和结果赋值给C的行片段。
2. `i`的循环，迭代了A和C的n个行片段，每次循环使用了B中相同的block（块大小是`bsize * bsize`）

下面的代码为矩阵乘法的分块版本

```c
void bijk(array A, array B, array C, int n, int bsize) {
  int i, j, k, kk, jj;
  double sum;
  int en = bsize * (n / bsize); /* Amount that fits evenly into blocks */

  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      C[i][j] = 0.0;

  for (kk = 0; kk < en; kk += bsize) {
    for (jj = 0; jj < en; jj += bsize) {
      for (i = 0; i < n; i++) {
        for (j = jj; j < jj + bsize; j++) {
          sum = C[i][j];
          for (k = kk; k < kk + bsize; k++) {
            sum += A[i][k] * B[k][j];
          }
          C[i][j] = sum;
        }
      }
    }
  }
}
```

下图为绘图解释。关键思路是，程序加载了B中的一个block，用完后，丢弃它。这种设计下：

- 对A的引用具有良好的空间局部性，因为每个行片段的迭代的步长为1；与此同时，还有很好的时间局部性，因为整个行片段的棉条被连续引用了bsize次。
- 对B的引用具有良好的时间局部性，因为整个bsize×bsize块被连续访问n次。
- 对C的引用具有良好的空间局部性，因为行片段的每个元素都是被连续写入的。 请注意，对C的引用没有良好的时间局部性，因为每个行片段只能访问一次。

<img src="/Users/rui/Library/Application Support/typora-user-images/image-20200719170035456.png" alt="image-20200719170035456" style="zoom:50%;" />

****

### 32x32矩阵转置

由前文分析，每个Line可以存储8个int，因此32x32矩阵各元素对应的Set编号如图

![image-20200720094107599](/Users/rui/Library/Application Support/typora-user-images/image-20200720094107599.png)

如果A按行读，B按列写，先读A<sub>00</sub>，Set[0]有cold miss，写给B<sub>00</sub>，Set[0]有conflict miss。再读A<sub>01</sub>，Set[4]有cold miss，写给B<sub>10</sub>，Set[0]hit...以此类推，存在大量Set被A使用了一次后就被B使用，造成很多conflicts miss和浪费。

```
 矩阵中缓存块`index`的分布：
    +--+--+--+--+
 0  | 0| 1| 2| 3|
    +--+--+--+--+
 1  | 4| 5| 6| 7|
    +--+--+--+--+
 2  | 8| 9|10|11|
    +--+--+--+--+
 3  |12|13|14|15|
    +--+--+--+--+
 4  |16|17|18|19|
    +--+--+--+--+
 5  |20|21|22|23|
    +--+--+--+--+
 6  |24|25|26|27|
    +--+--+--+--+
 7  |28|29|30|31|
    +--+--+--+--+
 8  | 0| 1| 2| 3|
    +--+--+--+--+
    ...

每一个小格子表示一个缓存块，格子中的数字是缓存块的index。
可以看到第0行和第8行缓存块的index是重复的。
```

为使得同一个矩阵在缓存中的内容不被替换，可以把分块大小设置为8x8

先按照8x8分块实现矩阵转置

```c
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    int ii, jj, i, j, temp;
    int blockSize = 8;
    int r = blockSize * (N / blockSize);
    int c = blockSize * (M / blockSize);

    for (ii = 0; ii < r; ii += blockSize) {
        for (jj = 0; jj < c; jj += blockSize) {
            for (i = ii; i < ii + blockSize; i++) {
                for (j = jj; j < jj + blockSize; j++) {
                    temp = A[i][j];
                    B[j][i] = temp;
                }
            }
        }
    }
}
```

测试前首先人工分析miss次数：

每个分块大小为8x8，A矩阵每次换行都有一次cold miss，行编号0-7共8行，虽然第8行与第0行冲突但分块为8x8因此不会导致conflictmiss，因此共8次miss。B矩阵在转置A矩阵第一行是每次都有cold miss，之后全部载入缓存不再出现miss，共8次

因此每个块的miss数为8。每个矩阵分为16块，共2个矩阵，因此总miss次数为8x16x2=256。

进行测试：

```shell
$ ./test-trans -M 32 -N 32

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:1710, misses:343, evictions:311

Summary for official submission (func 0): correctness=1 misses=343

TEST_TRANS_RESULTS=1:343
```

共343次miss，和分析相差较大。

查阅A、B矩阵在内存中的地址

```
A:55ed51a670a0, B:55ed51aa70a0
```

可以发现A和B最后16bits是一致的，由于缓存的index由倒数5到10个bit组成，因此A和B存在冲突。因为是转置操作，所以冲突发生在矩阵对角线上。

分析一下对角线缓存块的冲突，用`A[n]`表示第n个缓存快，`A[n][m]`表示第n个缓存块上第m个元素。

首先 `A`， `B` 固定的 miss 为 16。

在对角线元素复制时 `B[m][m] = A[m][m]`， 会发生 `A[m]`，`B[m]` 之间的冲突。复制前， `A[m]` 开始在缓存中，`B[m]` 不在。 复制时， `A[m]` 被 `B[m]` 取代。 下一个开始复制 `A[m]` 又被重新加载进入缓存取代 `B[m]`。这样就会产生 2 次多余的 miss。

最后一行和第一行情况有些不一样： 第一行 `B[m]` 被加载到缓存中是第一次，应该算在那 16 次中， 但是同样会发生 `A[0]` 的重新加载， 所以额外产生的 miss 次数为 1。 最后一行 `A[7]` 被取代， 但是复制已经完成，不需要再将 `A[7]` 加载进内存，所以额外的 miss 也为 1。

`B[m]` 被 `A[m]` 取代， 在下一行 `A[m+1]` 复制时需要重新加载 `B[m]` 进入缓存 ( 第一行除外 )， 所以会除了第一行每行又多了一次 miss。 所有总的额外的 miss 的数目为 `2*6+1*2+7=21`。 加上固定的 miss 次数， 对角块上的总的 miss 次数为 `37` 次。

具体的过程：

```
缓存中的内容 :
+-----------------------+-------------------+
| opt                   |  cache            |
+-----------------------+-------------------+
|before B[0][0]=tmp     | A[0]              |---+
+-----------------------+-------------------+   |
|after B[0][0]=tmp      | B[0]              |   |    
+-----------------------+-------------------+   |    A 的第一行复制到 B 的第一列 .
|after tmp=A[0][1]      | A[0]              |   |    最终缓存中剩下 A[0], B[1]...B[7].
+-----------------------+-------------------+   +--> A[0]被两次加载进入内存 , 
|after B[1][0]=tmp      | A[0] B[1]         |   |    总的 miss 次数是 10.               
+-----------------------+-------------------+   |    
|...                    |                   |   |    
+-----------------------+-------------------+   |
|after B[7][0]=tmp      | A[0] B[1..7]      |---+
+-----------------------+-------------------+
|after B[0][1]=tmp      | A[1] B[0] B[2..7] |---+
+-----------------------+-------------------+   |    A 的第二行复制到 B 的的二列 .
|after B[1][1]=tmp      | B[0..7]           |   |    其中发生的 miss 有 : 
+-----------------------+-------------------+   +--> A[0], B[0], A[1]与 B[1]的相互取代 . 
|...                    |                   |   |    总的 miss 次数为 4.
+-----------------------+-------------------+   |
|after B[7][1]=tmp      | A[1] B[0] B[2..]  |---+
+-----------------------+-------------------+        之后的三至七行与第二行类似 ,
|...                    |                   |------> miss 的次数都是 4.
+-----------------------+-------------------+
|after tmp=A[7][7]      | A[7] B[0..6]      |---+    最后一行 A[7]被 A[8]取代后 ,
+-----------------------+-------------------+   +--> 不需要重新加载 .
|after B[7][7]=tmp      | B[0..7]           |---+    总的 miss 数为 3. 
+-----------------------+-------------------+

所以对角块上的总的 miss 次数是 10+4*6+3=37.
```

对角分块有 4 个，普通的分块 12 个，所以总的 miss 数是 `4*37+16*12=340`，和实际结果相差 `3`。`3` 是一个固定的偏差，程序可能在这个过程中有三次额外的内存访问，在后面的根据算法定量分析结果和实际结果中都会有 `3` 次 miss 的偏差。

上面那种分块的实现存在过多冲突，A和B矩阵缓存快相互替换情况太多，可以考虑使用变量存下A的一行再复制给B，即使用本地变量作为缓存存储每个块的内容。本地变量数目不多时是存放在寄存器上的，因此可以减少访问内存。

```c
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
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

```

得到结果：

```shell
$ ./test-trans -M 32 -N 32

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:1766, misses:287, evictions:255

Summary for official submission (func 0): correctness=1 misses=287

TEST_TRANS_RESULTS=1:287
```

287次不命中。

### 64x64

这里同样使用分块技术进行优化，需要注意的是，当矩阵大小变为64x64时，矩阵中的每一行需要8个高速缓存行进行保存，使得高速缓存中只能保存4行的矩阵内容，如果我们还是使用块大小为8的分块技术，就会使得第5行和第1行冲突、第6行和第2行冲突等等，由此就会出现冲突不命中，所以我们只能设置块大小为4。

比如我们使用块大小为8，则不命中数目为4723，当修改块大小为4时，不命中次数为1891，当解决冲突不命中时，不命中次数为1667。

```c
void transpose_2(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int v1, v2, v3, v4;
    int bSize = 4;

    for (i = 0; i < M; i += bSize) {
        for (j = 0; j < N; j+= bSize) {
            for (k = i; k < i + bSize; k++) {
                v1 = A[k][j];
                v2 = A[k][j + 1];
                v3 = A[k][j + 2];
                v4 = A[k][j + 3];
                
                B[j][k] = v1;
                B[j + 1][k] = v2;
                B[j + 2][k] = v3;
                B[j + 3][k] = v4;
            }
        }
    }
}
```

```shell
$ ./test-trans -M 64 -N 64

Function 0 (2 total)
Step 1: Validating and generating memory traces
Step 2: Evaluating performance (s=5, E=1, b=5)
func 0 (Transpose submission): hits:6498, misses:1699, evictions:1667
```

满分答案：

```c
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
```



### 61x67

由于这里行和列的数目不同，以及每一行元素个数不是刚好保存填充完整的行，所以元素保存在缓存中会存在错位，可能会减少`B`的冲突不命中，所以可以使用较大的块。比如我们使用大小为17的块，结果为1950。

```c
void transpose_3(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, ii, jj, temp;
    int bSize = 17;

    for (ii = 0; ii < M; ii += bSize) {
        for (jj = 0; jj < N; j+= bSize) {
            for (i = ii; i < ii + bSize; i++) {
                for (j = jj; j < jj + bSize; j++) {
                    temp = A[i][j];
                    B[j][i] = temp;
                }
            }
        }
    }
}
```

