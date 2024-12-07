# C4学习

## 为什么是C4

因为实际上实现这个编译器只使用了4个函数：

+ next（）	用于词法分析，获取下一个标记，且可以忽略空白字符
+ program（）        语法分析的入口，用来分析整个程序
+ expression（level）        用于解析一个表达式
+ eval（）        虚拟机的入口，用于解释目标代码

## 初步的框架

实际上这是一个解释器 编译器实在是写不出来

让我们看看一个编译器需要做些什么

+ 构建我们自己的虚拟机和指令集 这之后生成的目标代码将是我们的指令集
+ 构建我们自己的词法分析器
+ 构建一个语法分析器

 所以这里我们可以搭建一个初步的框架 之后再向里面填充内容

```C
#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<string.h>
#include<fcntl.h>

int token;      //设置一个标记
char *src,*old_src;     //设置一个指向源代码的指针
int poolsize;       //设置一个内存池用来存放数据
int line;       //用来追踪源代码的行号

void next(){
    token = *src++;
    return;
}

void expression(int level){
    //暂时为空
}

void program(){
    next();     //读取下一个标记
    while(token > 0){
        printf("Token is %c\n",token);
        next();
    }
}

int eval(){
    //暂时为空
    return 0;
}

int main(int argc,char **argv){
    int i,fd;

    argc--;
    argv++;

    poolsize = 256*1024;     //设置内存池的大小为256KB
    line = 1;

    if((fd = open(*argv,0)) < 0){
        printf("could not open(%s)\n",*argv);
        return -1;
    }

    if(!(src = old_src = malloc(poolsize))){
        printf("could not malloc(%d) for source area\n",poolsize);
        return -1;
    }

    //读取源代码文件
    if((i = read(fd,src,poolsize-1)) <= 0){
        printf("read() returned %d\n",i);
        return -1;
    }
    src[i] = 0;     //添加文件结尾字符
    close(fd);

    program();
    return eval();
}
```



## 搭建一个虚拟机

因为这是一个解释器，所以我们需要构建一台虚拟的电脑，设计自己的指令集

### 计算机的内部原理

我们需要关心计算机的三个基本原件：CPU，寄存器 和 内存

代码（汇编）以二进制的形式被存储在内存中，CPU从中一条一条的加载指令，并将结果保存在寄存器中

### 让我们谈谈内存

实际上我们要设计的是一个虚拟内存，系统将它映射在实际的内存中

但是这不重要，一般而言 我们将进程的内存分成几个段落：

+ 代码段（text）：用于存放代码（指令）
+ 数据段（data）：用于存放初始化过的数据，如 `int i = 10`就需要放在数据段中
+ 未初始化数据段（bss）：用于存放未初始化的数据，因为不关心其中的真正数值，所以单独存放以节省空间
+ 栈（stack）：用于处理函数调用的相关数据
+ 堆（heap）：用于程序动态分配内存

他们的位置关系大致如下

```
+------------------+
|       栈   |     |      高位
|    ...     v     |
|                  |
|                  |
|                  |
|                  |
|    ...     ^     |
|    堆	    |     |
+------------------+
| 未初始化数据段    |
+------------------+
| 数据段            |
+------------------+
| 代码段            |      低位
+------------------+
```

用于我们并不需要模拟完整的计算机 我们只用关心三个内容：代码段 数据段 栈

接下来我们需要引入一个指令 MSET 使我们能直接使用解释器中的内存

首先我们在全局变量添加以下代码

```C
int *text,      //代码段
    *old_text,      //用于保存使用过的代码
    *stack;     //栈
char *data;     //数据段
```

这里的类型虽然是 int 但实际上是 unsigned int 因为我们会在代码段（text）中存放指针/内存地址数据 它们是无符号的

而数据段（data）由于只存放字符串 所以是`char *`型

现在我们可以向 main 函数中加入初始化代码 为其真正的分配内存：

```C
int main() {
close(fd);
...

// 为虚拟机分配内存
if (!(text = old_text = malloc(poolsize))) {
printf("could not malloc(%d) for text area\n", poolsize);
return -1;
}
if (!(data = malloc(poolsize))) {
printf("could not malloc(%d) for data area\n", poolsize);
return -1;
}
if (!(stack = malloc(poolsize))) {
printf("could not malloc(%d) for stack area\n", poolsize);
return -1;
}

memset(text, 0, poolsize);
memset(data, 0, poolsize);
memset(stack, 0, poolsize);

...
program();
```

### 再看向寄存器

寄存器用于存放计算机的运行状态，真正的计算机中有各种寄存器，但我们只需要用到四种寄存器：

+ PC程序计数器：它用于存放一个内存地址，该地址存放着**下一条**要执行的语句
+ SP指针寄存器：永远指向当前的栈顶。注意由于栈是位于高位向地位增长的，所以入栈时SP的值需要减小
+ BP基址指针：用于指向栈的某个位置，再调用函数时会用到它
+ AX通用寄存器：在我们的虚拟机中，它用于存放一条指令执行后的结果

（寄存器只是用于保存执行中的状态）

现在我们再全局中加入寄存器的变量

```c
int *pc,*bp,*sp,ax,cycle;       //虚拟机中的寄存器
```

向main函数添加初始化代码 因为PC在初始化时应该指向目标代码的中的main函数 但是我们还没有编译代码 所以暂时不做处理

```C
memset(stack, 0, poolsize);
...

bp = sp = (int *)((int)stack + poolsize);
ax = 0;

...
program();
```

至此我们对寄存器已经完成了初始化

### 制定指令集

现在我们要为自己的虚拟机创建一个指令集

首先我们在全局变量引入一个枚举类型，作为我们所要支持的指令集

```c
//指令集
enum { LEA ,IMM ,JMP ,CALL ,JZ ,JNZ ,ENT ,ADJ ,LEV ,LI ,LC ,SI ,SC ,PUSH,
        OR ,XOR ,AND ,EQ ,NE ,LT ,GT ,LE ,GE ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD,
        OPEN ,READ ,CLOS ,PRTF ,MALC ,MSET ,MCMP ,EXIT};
```

这样的顺序安排是有意的 带参数的指令被放在前面 没有参数的指令放在后面

### 讲几个指令

#### MOV

MOV 是所有指令中最基础的一个，它用于将数据放入指定的寄存器或者内存地址中，有点像C语言中的赋值语句

X86的MOV指令有两个参数 分别是源地址和目标地址：`MOV dest,source`

表示将source内容放在dest中 它们可以是一个数，寄存器或者是一个内存地址

由于我们的虚拟机只有一个寄存器，且识别参数的类型是比较困难的，所以我们将MOV拆成五个指令

+ `IMM <num>`将 `<num>`放入寄存器 `ax`中
+ `LC` 将对应的地址中的字符载入 `ax` 中，要求 `ax`中存放地址
+ `LI` 将对应的地址中的整数载入 `ax` 中，要求 `ax`中存放地址
+ `SC` 将 `ax` 中的数据作为字符放入地址中，要求栈顶存放地址
+ `SI` 将 `ax` 中的数据作为整数放入地址中，要求栈顶存放地址

将MOV拆分后 只有IMM需要有参数 且不需要判断类型 大大的简化了实现的难度

在eval（）函数中加入以下代码

```C
int eval(){
    int op,*tmp;
    while(1){
        if(op == IMM){ax = *pc++;}      //向ax中添加立即数
        else if(op == LC){ax = *(char *)ax;}        //从ax中加载一个字符到ax
        else if(op == LI){ax = *(int *)ax;}     //从ax中加载一个整数到ax
        else if(op == SC){ax = *(char *)*sp++ = ax;}        //保存ax中的字符到当前地址，并且更新栈指针
        else if(op == SI){*(int *)*sp++ = ax;}      //保存ax中的整数到当前地址，并且更新栈指针
    }
    ...
    return 0;
}
```

其中的 *sp++ 的作用是退栈 即POP

**注意**：为什么SI/SC 指令中地址存放在栈中 ，而在LI/LC中地址存放在ax中

因为是默认计算结果是存放在 ax中的，而地址通常通过计算得到，所以执行LI/LC时直接从 ax中取值更加高效

另一个原因是我们的PUSH操作只能将 ax 的值放到栈上，而不能直接以值作为参数

#### PUSH

在X86中，PUSH的作用是将值放入寄存器中

而在我们的虚拟机里面 它的作用是将ax的值放入栈中 （以此来简化虚拟机的实现）

```c
        else if(op == PUSH){*--sp = ax;}        //将ax的值压入栈中
```

#### JMP

`JMP <addr>`  是跳转指令，无条件的将当前的 PC 寄存器设置为 `<addr>` 实现如下

```C
else if(op == JMP){pc = (int *)*pc;}        //跳跃到指定的地址
```

要记得pc寄存器指向的是 **下一条** 指令 所以此时它放置的是 JMP指令的参数，即 `&lt;addr&gt;` 的值

#### JZ/JNZ

为了实现 if 语句，我们需要条件判断相关的指令。在此我们只需要实现两个最简单的条件判断，即结果为零或者不为零情况下的跳转

```c
else if(op == JZ){pc = ax ? pc+1 : (int *)*pc;}         //如果ax为0就跳转
else if(op == JNZ){pc = ax ? (int *)*pc : pc+1;}        //如果ax不为0就跳转
```

### 子函数的调用

这是汇编中最难理解的部分 这里要引入 CALL END ADJ 以及 LEV

#### CALL/RET

`CALL <addr>`与 `RET` 指令，CALL的作用是跳转到地址 `<addr>`的子函数 RET则用于从子函数返回

**问**：为啥不直接使用JMP指令？

原因是当我们从子函数返回时，程序需要回到跳转之前的地方继续运行，因此事先需要将这个信息存储起来

反过来，子函数要返回时，就需要获取并恢复这个信息，因此实际我们把PC保存到栈中

```c
        else if(op == CALL){*--sp = (int)(pc+1);pc = (int *)*pc;}     //将下一条指令的位置存在栈顶 并且跳转到指定的地址
//      else if(op == RET){pc = (int *)*sp++;}                //从栈顶获取返回地址 使得执行流程回到调用子函数之前
```

这里将RET注释了 之后使用LEV指令来替代它

**问：**在实际调用函数中，不仅要考虑函数的地址，还要考虑如何传递参数和如何返回结果。这里我们约定，如果子函数有返回结果,那么就在返回时保存在ax ，它可以是一个值也可以是一个地址 ，那么参数的传递呢？

在此 各种编程语言关于如何调用子函数有不同的约定，例如C语言的调用标准是：

+ 由调用者将参数入栈
+ 调用结束时，由调用者将参数出栈
+ 参数逆序入栈

在我们的编译器中，编译器参数时顺序入栈

这里引入一个例子（C语言）

```C
int caller(int, int, int);

int caller(void)
{
	int i, ret;

	ret = callee(1, 2, 3);
	ret += 5;
	return ret;
}
```

会生成以下X86汇编代码

```ini
caller:
; make new call frame
push    ebp
mov     ebp, esp
sub     1, esp       ; save stack for variable: i
; push call arguments
push    3
push    2
push    1
; call subroutine 'caller'
call    caller
; remove arguments from frame
add     esp, 12
; use subroutine result
add     eax, 5
; restore old call frame
mov     esp, ebp
pop     ebp
; return
ret
```

上面这个代码在我们的虚拟机中运行会有几个问题

+ push ebp  但是我们的PUSH 无法指定寄存器
+ mov ebp,esp  我们的MOV指令也不可以满足
+ add esp,12 还是不可以

所以我们的指令太过于简单（如只能操作ax寄存器）所以用上面提到过的指令我们连函数调用都无法实现。但我们又不想扩充指令集（这样会好复杂）所以我们只能增加指令集（反正我们也不是真正的计算机）

#### ENT

`ENT <size>`指的是 enter ,用于实现 `make new call frame`的功能，即保存当前的栈指针，同时在栈上保留一定的空间，用来存放局部变量。对应的汇编代码为：

```ini
	;make new call frame
	push	ebp
	mov		ebp, esp
	sub		1, esp			;save stack for variable: i
```

实现如下：

```C
else if (op == ENT)  {*--sp = (int)bp; bp = sp; sp = sp - *pc++;} // 创建一个新的栈顶
```

#### ADJ

`ADJ <size>`用于实现 `remove arguments from frame`用于在函数调用结束后清理函数参数所占用的栈空间

本质上是因为我们的ADD指令功能有限。对应的汇编代码为：

```ini
	;remove arguments from frame
	add		esp, 12
```

实现如下：

```c
        else if(op == ADJ){sp = sp + *pc++;}                    //等同于 add esp, <size>
```

#### LEV 

本质上这个指令并不是必须的，只不过我们的指令集中没有POP指令，并且三条指令写起来比较麻烦而且浪费空间，所以用这个指令替代

其汇编代码是：

```ini 
	;restore old call frame
	mov		esp, ebp
	pop		ebp
	;return
	ret
```

具体的代码如下;

```C
else if(op == LEV){sp = bp;bp = (int *)*sp++;pc = (int *)*sp++;}//恢复调用帧和程序计数器
```

在这里LEV 指令的功能实际上包裹了RET 故可以舍弃RET指令了

#### LEA

上面的指令解决了调用帧的问题，但还有一个问题就是如何在子函数中获得传入的参数。

这里我们举例子来了解一下当参数调用时。栈中的调用帧是什么样的。我们继续使用之前的例子（现在用顺序调用参数）

```
sub_function(arg1, arg2, arg3);

|    ....       | 高位地址
+---------------+
| arg: 1        |    new_bp + 4
+---------------+
| arg: 2        |    new_bp + 3
+---------------+
| arg: 3        |    new_bp + 2
+---------------+
|return address |    new_bp + 1
+---------------+
| old BP        | <- new BP
+---------------+
| local var 1   |    new_bp - 1
+---------------+
| local var 2   |    new_bp - 2
+---------------+
|    ....       |  地位地址
```

所以为了获得第一个参数 我们需要 `new_bp + 4` 但就如上面所说 我们的ADD指令无法操作除了ax之外的寄存器

所以我们需要一个全新的指令 `LEA <offset>`

实现如下：

```C
else if(op == LEA){ax = (int)(bp + *pc++);}             //计算参数的地址并将其存储到ax中
```

以上就是我们为了实现函数调用所需要的指令了

### 运算符指令

我们为C语言中支持的运算符都提供对应的汇编指令。每个运算符都是二元的，即有两个参数，第一个参数放在栈顶，第二个参数放在ax中。这个顺序一定要注意 因为像 - 或者 / 之类的运算符是与参数顺序有关的。计算后会将栈顶的参数退栈，结果放在ax中。因此计算结束后，两个参数都无法取得了 （汇编的意义上，如果是在内存地址上就另当别论）

实现如下：

```C
else if(op == OR){ax = *sp++ | ax;}
else if(op == XOR){ax = *sp++ ^ ax;}
else if(op == AND){ax = *sp++ & ax;}
else if(op == EQ){ax = *sp++ == ax;}
else if(op == NE){ax = *sp++ != ax;}
else if(op == LT){ax = *sp++ < ax;}
else if(op == LE){ax = *sp++ <= ax;}
else if(op == GT){ax = *sp++ > ax;}
else if(op == GE){ax = *sp++ >= ax;}
else if(op == SHL){ax = *sp++ << ax;}
else if(op == SHR){ax = *sp++ >> ax;}
else if(op == ADD){ax = *sp++ + ax;}
else if(op == SUB){ax = *sp++ - ax;}
else if(op == MUL){ax = *sp++ * ax;}
else if(op == DIV){ax = *sp++ / ax;}
else if(op == MOD){ax = *sp++ % ax;}
```

### 内置函数

程序要想有用，除了核心的逻辑之外还需要输入输出，例如C语言中我们经常用到的printf函数就是用于输出。

但是printf函数本身的十分复杂，如果我们的编译器要达到自举，就一定要实现printf之类的函数，但它又与编译器并没有什么关系，因此我们继续实现新的指令，从虚拟机的角度予以支持。

编译器中我们需要用到的函数有 `exit` `open` `close` `read` `printf` `malloc` `memset` 代码如下：

```c
else if(op == EXIT){printf("exit(%d)",*sp);return *sp;}
else if(op == OPEN){ax = open((char *)sp[1],sp[0]);}
else if(op == CLOS){ax = close(*sp);}
else if(op == READ){ax = read(sp[2],(char *)sp[1],sp[0]);}
else if(op == PRTF){tmp = sp + pc[1];ax = printf((char *)tmp[-1],tmp[-2],tmp[-3],tmp[-4],tmp[-5],tmp[-6]);}
else if(op == MALC){ax = (int)malloc(*sp);}
else if(op == MSET){ax = (int)memset((char *)sp[2],sp[1], *sp);}
else if(op == MCMP){ax = memcmp((char *)sp[2],(char *)sp[1],*sp);}
```

这里的原理是，我们的电脑上已经有了这些函数的实现，因此编译编译器时，这这些函数的二进制代码就被编译进了我们的编译器，因此在我们的虚拟机上运行我们提供的这些指令时，这些函数就是可用的

最后再补一个内置函数的错误判断:

```c
else {
	printf("unknown instruction:%d\n",op);
	return -1;
}
```

### 内置函数原理

暂时先空着



## 词法分析器！！！

### 什么是词法分析器？

简单来说，词法分析器用来对源码字符串做简单预处理，以减少语法分析器的复杂程度。

词法分析器以源代码字符串为输入，输出作为标记流（token stream），即一连串的标记，每个标记通常包括：

（token，token value）即标记本身和标记的值

例如：源码中包含一个数字 ’998‘ ，词法分析器将输出（Number，998）即数字（数字，998）再例如：

```c
2 + 3 * (4 - 5)
= >
(Number, 2) ADD (Number, 3) Multiply Left-Bracket (Number, 4) Subtract (Number, 5) Right-Bracket
```

通过词法分析器的预处理，语法分析器的复杂程度会大大降低

### 词法分析器和编译器

要是深入词法分析器，你就会发现，它本质上也是编译器。我们编译器是以标记流为输入，输出汇编代码，而词法分析器则是以源码字符串为输入，输出标记流。

```
                   +-------+                      +--------+
-- source code --> | lexer | --> token stream --> | parser | --> assembly
                   +-------+                      +--------+
```

在这个前提之下，我们可以这样认为：

直接从源码编译成汇编是很难困难的 因为输入的字符串比较难处理。所以我们先编写一个较为简单的编译器（词法分析器）来将字符串转换为标记流，而标记流对于词法分析器而言就容易处理的多了。

### 词法分析器的实现

由于词法分析的工作很常见，但又枯燥且容易出错，所以人们已经开发了许多工具用来生成词法分析器，如lex,flex。这些工具允许我们通过正则表达式来识别标记。

**注意：**我们并不会一次性将所有源码全部转换成标记流，原因有二：

+ 字符串转换成标记流有时是有状态的，即与代码的上下文是有关联的
+ 保存所有的标识流没有意义且浪费空间

所以实际的处理方式是提供一个函数（即上文的`next()`），每次调用该函数则返回下一个标记

### 支持的标记

在全局中添加以下定义

```C
//标记和类别（将运算符放在最后，按照优先级顺序排列）
enum {
     Num = 128, Fun, Sys, Glo, Loc, Id,
     Char, Else, Enum, If, Int, Return, Sizeof, While,
     Assign, Cond, Lor, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Bral 
};
```

这些就是我们要支持的标识符号。例如：我们会将 = 解析为 Assign ；将 == 解析为 Eq ；将 ！= 解析为 Ne 等

所以这里我们会有这样的印象，一个标记（token）可能包含多个字符，且多数情况下如此。而词法分析器能减少语法分析器的复杂度的原因，正是因为它相当于通过一定的编码（更多的编码）来压缩了源码字符串

当然，上面这些标记是有顺序的，跟它们在C语言中的优先级有关。如 *(Mul) 的优先级要高于 +(Add)。

最后要注意的是还有一些字符，它们自己构成了标记，如右方括号 】 或波浪号 ~等，不处理它们的原因是：

+ 它们是单字符的，不是由多字符共同构成标记的
+ 它们并不涉及优先级关系

### 词法分析器的框架

即 `next（）` 函数的主体：

```C
void next(){
    char *last_pos;
    int hash;

    while (token = *src){
        ++src;
        //这部分用来解析标识位
    }
    return;
}
```

**问：**这里为什么要用while循环呢？这里涉及到编译器的一个问题，如何处理错误？

对于词法分析器 如果我们遇到一个不认识的字符怎么处理？一般有两种方法：

+ 指出错误发生的位置，并退出整个程序
+ 指出错误发生的位置，跳过当前错误并继续编译

这个while循环的作用就是跳过这些我们不识别的字符，同时我们也可以用来处理空白字符。我们知道，C语言中空格是用来作分隔的，并不作为语法的一部分。因此在实现中我们将它作为”不识别"的字符，这个while循环可以用来跳过它

### 换行符

```c
//这部分用来解析标识位
...

if (token == '\n') {
++line;
}
...
```

换行符和空格差不多但是要注意，每次遇到换行符，行号要+1

### 宏定义

C语言的宏定义以字符“#”开头，如# include <stdio.h> 这个编译器不支持宏定义，所以直接跳过

```C
else if(token = '#'){
    //跳过宏定义，此编译器不支持
    while(*src != 0 && *src != '\n'){
        src++;
    }
}
```

### 标识符与符号表

标识符（Identifier）可以理解为变量名。对于语法分析而言，我们并不关心一个变量名具体叫什么，而只关心这个变量名代表的唯一标识。例如 `int a` 定义了变量 `a`，而之后的语句 `a = 10` ，我们需要知道这两个a指向的是同一个变量

基于这个理由，词法分析器会把扫描到的标识符全部保存到一张表中，遇到新的标识符就去查这张表，如果标识符已经存在，就返回它的唯一标识。

实现如下：

```c
struct identifer {
    int token;
    int hash;
    char * name;
    int class;
    int type;
    int value;
    int Bclass;
    int Btype;
    int Bvalue;
}
```

这里解释具体含义：

+ token：该标识符返回的标记，理论上所有的变量返回的标记都是 Id，但实际上由于我们还将在符号表中加入关键字如 if，while等 ，它们都有对应的标记
+ hash：顾名思义，就是这个标识符的哈希值，用于标识符的快速比较
+ name：存放标识符本身的字符串
+ class：该标识符的类别，如数字，全局变量，局部变量等
+ type：标识符的类型，即如果他是个变量，变量是int还是char还是指针型
+ value：存放这个标识符的值，如标识符是函数，刚存放的函数的地址
+ BXXXX：C语言中标识符可以是全局也可以是局部的，当局部标识符的名字与全局标识符相同时，用作保存全局标识符的信息

由上可以看出，我们实现的词法分析器与传统意义上的词法分析器不太相同。传统意义上的符号表，之需要知道标识符的唯一标识即可，而我们还存放了一些只有语法分析器才会得到的信息，如 type

由于这个解释器希望能完成自举,而且我们定义的语法不支持struct ,故使用下列方式：

```
符号表:
----+-----+----+----+----+-----+-----+-----+------+------+----
 .. |token|hash|name|type|class|value|btype|bclass|bvalue| ..
----+-----+----+----+----+-----+-----+-----+------+------+----
    |<---     	 		  一个标识符		            --->|
```

即使用一个整型数组来保存相关的ID信息。每个ID占用数组中的9个空间，分析标识符的代码如下：

```C
int token_val;          //当前标记的值
int *current_id,        //当前标识符的ID信息
    *symbols;           //符号表

//符号表的条目
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, Idsize};

void next(){
    ……
        
    else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')){

            //解析标识符
            last_pos = src -1;
            hash = token;

            while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')){
                hash = hash* 147 + *src;
                src++;
            }

            //查找现有的标识符，线性查找
            current_id = symbols;
            while (current_id[Token]){
                if(current_id[Hash] == hash && !memcmp((char*)current_id[Name],last_pos,src-last_pos)){
                    //找到符合的便返回
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + Idsize;
            }

            //展示新的ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        }
    
    ……
        
}
```

查找已有标识符的方法是查找symbols表。

### 数字

数字中较为复杂的一点是需要支持十进制，十六进制以及八进制。逻辑较为直接，当然十六进制有点困难

我们可以使用以下方法尝试转换十六进制：

```c
token_val = token_val + (token&16) + (token >= 'A'9:0);
```

这里需要注意的是在ASCII码中，字符a对应的是十六进制是61，A是41，故通过（token & 16）可以得到个位数的值。

```C
else if(token >= '0' && token <= '9'){
    //处理数字，三种类型：十进制，八进制，十六进制
    token_val = token - '0';
    if (token_val>0){
        //十进制，开始于[0~9]
        while(*src >= '0' && *src <= '9'){
            token_val = token_val*10 + *src++ - '0';
        }
    }else{
        //从0开始
        if(*src == 'x' || *src == 'X'){
            //十六进制
            token = *++src;
            while((token >= '0'&& token <= '9' )||(token >= 'a'&&token <= 'f')||(token >= 'A'&&token <= 'F')){
                token_val = token_val * 16 + (token >= 'A'? 9:0);
                token = *++src;
            }
        }else{
            //八进制
            while(*src >= '0' && *src <= '7'){
                token_val = token_val*8 + *src++ - '0';
            }
        }
    }
    token = Num;
    return;
}
```

### 字符串

在分析时，如果分析到字符串，我们需要将它存放到之前所说的data段中。然后返回它在data段中的地址。另一个特殊的地方是我们需要支持转义符。例如用 `\n`表示换行符。由于本编译器的目的是打到自己编译自己，所以代码中并没有支持除 `\n` 的转义符。如 `\r` `\t` 等，但仍支持 `\a` 表示字符 a 的语法 如 `\"`表示 `"`

在分析时骂我们将同时分析单个字符如 'a' 和字符串如 'a string'。若得到的是单个字符，我们以 `Num` 的形式返回，相关代码如下：

```C
else if(token == '"' || token == '\''){
    //解析字符串
    last_pos = data;
    while(*src != 0 && *src != token){
        token_val = *src++;
        if(token_val == '\\'){
            //抛弃字符串
            token_val = *src++;
            if(token_val == 'n'){
                token_val = '\n';
            }
        }
        if(token == '"'){
            *data++ = token_val;
        }
    }
    src++;
    //如果是单个的字符，返回为Num标识
    if(token == '"'){
        token_val = (int)last_pos;
    }else{
        token = Num;
    }
    return;
}
```

### 注释

在我们的C语言中，只支持 `//`类型的注释，不支持 `/*comments*/`类型的注释

```C
else if(token == '/'){
    if(*src == '/'){
        //跳过内容
        while (*src != 0 && *src != '\n'){
            ++src ;
        }
    }else{
        //做除法操作
        token = Div;
        return;
    }
}
```

这里我们要额外介绍 `lookahead` 的概念，即提前看多个字符。上述代码中我们看到，除了跳过注释，我们还可能返回除号 `/(Div)` 标记

提前看字符的原理是：有一个或多个标记是以同样的字符开头的，因此只凭借当前的字符我们并无法确定具体应该解释成哪一个标记，所以只能向前查看字符，如本例需要向前查看一个字符若是 `/`则说明是注释不是除号

另外，我们用词法分析器将源码转换成标记流，能减小语法复杂度，原因之一就是减少了语法分析器需要向前看的字符个数。

### 其他

```C
else if(token == '='){
    //解析==和=
    if(*src == '='){
        src++;
        token = Eq;
    }else{
        token = Assign;
    }
    return;
}else if(token == '+'){
    //解析++和+
    if(*src == '+'){
        src++;
        token = Inc;
    }else{
        token = Add;
    }
    return;
}else if(token == '-'){
    //解析-- 和-
    if(*src == '-'){
        src ++;
        token = Dec;
    }else{
        token = Sub;
    }
    return;
}else if(token == '!'){
    //解析 !=
    if(*src == '='){
        src++;
        token = Ne;
    }
    return;
}else if(token == '<'){
    //解析 <=,<<,<
    if(*src == '='){
        src++;
        token =Le;
    }else if(*src == '<'){
        src++;
        token = Shl;
    }else{
        token = Lt;
    }
    return;
}else if(token == '>'){
    //解析 >=,>>,>
    if(*src == '='){
        src++;
        token = Ge;
    }else if(*src == '>'){
        src++;
        token = Shr;
    } else{
        token = Gt;
    }
    return;
}else if(token == '|'){
    //解析 | 和||
    if(*src == '|'){
        src ++;
        token = Lor;
    }else{
        token = Or;
    }
    return;
}else if(token == '&'){
    //解析 & 和 &&
    if(*src == '&'){
        src ++;
        token = Lan;
    }else{
        token = And;
    }
    return;
}else if(token == '^'){
    token = Xor;
    return;
}else if(token == '%'){
    token = Mod;
    return;
}else if(token == '*'){
    token = Mul;
    return;
}else if(token =='['){
    token = Brak;
    return;
}else if(token == '?'){
    token = Cond;
    return;
}else if(token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':'){
    //直接返回即可
    return;
}
```

代码较多，但核心就是向前看一个字符来确定标记

### 关键字与内置函数

虽然上面写完了词法分析器，但还有一个问题，那就是”关键字“，例如if,while,return等

它们不能作为普通的标识符，因为有特殊的含义

一般有两种处理方式：

+ 词法分析器中直接解析关键字
+ 在语法分析器中将关键字提前加入符号表

这里我们将它加入符号表，并提前为它们赋予必要的信息（正如前面的token字段）这样当源码中出现关键字是，它们会被解析成标识符，但由于符号表中有相关的信息，我们就知道它是关键词

内置的函数的行为也和关键字一样，不同的只是赋值的信息，在main函数中进行初始化如下：

```C
//变量的类型功能
enum { CHAR, INT, PTR };
int *idmain; //main函数功能

void main() {
...

src = "char else enum if int return sizeof while open read close printf malloc memset memcmp exit void main";

//增加关键词
i = Char;
while (i <= While) {
next();
current_id[Token] = i++;
}

//添加库
i = OPEN;
while (i <= EXIT) {
next();
current_id[Class] = Sys;
current_id[Type] = INT;
current_id[Value] = i++;
}

next(); current_id[Token] = Char; //处理void型
next(); idmain = current_id; 	  //追踪主函数

...
program();
}
```



## 语法分析

语法分析有点复杂，因此分为三个部分：

+ 变量定义
+ 函数定义
+ 表达式

在这里我们先讲递归下降作为前置知识

### 递归下降

#### 什么是递归下降

传统上，编写语法分析器有两种方法，一种是自顶向下，一种是自底向上。自顶向下是从非终结符开始，不断地对非终结符进行分解，直到匹配到输入的终结符；自底向上是不断地将终结符进行合并，直到合并成起始的非终结符。

其中自顶向下方法就是我们所说的递归下降。

#### 终结符与非终结符

首先介绍一下BNF范式（就是一种用来描述语法的语言）

其中由三种元素组成：

+ 项`<term>`: 表达式中的基本单元，由数字 变量 运算符（乘法除法）组成
+ 表达式`<expr>`:由项通过加法减法运算符连接起来的复杂结构
+ 因子`<factor>`:是表达式中的最小单位，是构成项的基本元素，可以是数字，变量，或者是括号内的表达式

```
<expr> ::= <expr> + <term>
		| <expr> - <term>
		| <term>

<expr> ::= <term> * <factor>
		| <term> / <factor>
		| <factor>
		
<expr> ::= ( <expr> )
		| Num
```

用<>，括起来就称作为 **非终结符** ，因为它们可以用 `::=` 右侧的式子代替。

`|` 表示选择，如 `<expr> + <term>` `<expr>` `<expr> - <term>` 或 `<term>` 中的一种。

而没有出现在 `::=` 左边的就称为 **终结符** ,一般的终结符对应于词法分析器输出的标记

#### 四则运算的递归下降

例如 我们对 `3 * (4 + 2)`进行语法分析。我们假设词法分析器已经正确的将其中的数字识别成了标记 `Num`

递归下降时从起始的非终结符开始（顶），本例中是`<expr>`，实际上可以自己指定，不指定的话一般认为是第一个出现的非终结符

```
<expr> => <expr>
       => <term>        * <factor>
       => <factor>     |
       => Num (3)      |
						=> ( <expr> )
						=> <expr>           + <term>
						=> <term>          |
						=> <factor>        |
						=> Num (4)         |
                                                => <factor>
                                                => Num (2)
```

可以看出，整个解析的过程是在不断对非终结符进行替换（向下），直到遇到了终结符（底）。而我们可以从解析的过程中看出，一些非终结符如`<expr>`被递归使用了

#### 为什么选择递归下降

从刚刚对四则运算的递归下降解析可以看出，整个解析的过程和语法的BNF表示是二分接近的，更为重要的是，我们可以很容易地直接将BNF表示转换成实际的代码。方法是为每个产生方式（即 `非终结符 ::= ...`）生成一个同名的函数。

这里会有一个疑问，就是上例中，当一个终结符有多个选择时，如何具体选择哪一个？如为什么用 `<expr> ::= <term> * <factor>` 而不是  `<expr> ::= <term>`?这里就涉及到上一章中的"向前看k个标记"的概念。我们向前看一个标记，发现是 `*` 而这个标记足够让我们决定使用哪个表达式了。

另外，递归下降方式对BNF方法本身有一定的要求，否则会有一些问题，如经典的"左递归"问题

#### 左递归

左递归的语法是没法直接使用递归下降的方法实现的，因此需要消除左递归，消除后文法如下：

```text
<expr> ::= <term> <expr_tail>
<expr_tail> ::= + <term> <expr_tail>
              | - <term> <expr_tail>
              | <empty>

<term> ::= <factor> <term_tail>
<term_tail> ::= * <factor> <term_tail>
              | / <factor> <term_tail>
              | <empty>

<factor> ::= ( <expr> )
          | Num
```

##### 消除左递归的方法

#### 四则运算的实现

在这里我们专注于语法分析部分的实现，具体实现很容易，就是上述的消除左递归后的文法直接转换而来的

```c
int expr();
int factor(){				//用来检查括号，如果不是括号那么应该是一各数字
    int value = 0;
    if(token == '('){
        match('(');
        value = expr();
        match(')');
    }else{
        value = token_val;	
        match(Num);
    }
    return value
}

int term_tail(int lvalue){	//用来处理项的尾部（乘法除法）
    if(token == '*'){
        match('*');
        int value = lvalue * factor();
        return term_tail(value);
    }else if(token == '/'){
        match('/');
        int value = lvalue / factor();
        return term_tail(value);
    }else{
        return lvalue
    }
}

int expr_tail(int lvalue){	//处理表达式的尾部（加法减法）
    if(token == '+'){
        match('+');
        int value = lvalue + term();
        return expr_tail(value);
    }else if(token == '-'){
        match('-');
        int value = lvalue - term();
        return expr_tail(value);
    }else{
        return lvalue;
    }
}

int expr(){
    int lvalue = term();
    return expr_tail(lvalue);
}
```

可以看到，有了BNF方法后，采用递归向下的方法实现编译器是很直观的。

我们把词法分析器的代码一并加上(这是一个简单的词法分析)

```c
#include <stdio.h>
#include <stdlib.h>

enum{Num};
int token;
int taken_val;
char *line = NULL;
char *src = NULL;

void next(){
    //跳过空白部分
    while (*src == ' ' || *src == '\t'){
        src++;
    }
    
    token = *src++;
    
    if(token >= '0' && token <= '9'){
        token_val = token -'0';
        token = Num;
        
        while (*src >= '0' && *src <= '9'){
            token_val = token_val*10 + *src - '0';
            src ++;
        }
        return;
    }
}

void match(int tk){
    if(token != tk){
        printf("expected token: %d(%c),got: %d(%c)\n",tk,tk,token,token);
        exit(-1);
    }
    next();
}
```

最后是main函数：

```c
int main(int argc,char *argv[]){
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line,&linecap,stdin)) > 0){
        src = line;
        next();
        printf("%d\n",expr());
    }
    return 0;
}
```

至此，我们完成了一个能够解析BNF格式的分析器。

接下来我们将运用到我们的编译器中。

### 变量定义

#### EBNF表示

EBNF是对前一章BNF的扩展，它的语法更容易理解，更容易实现

我们在下方展示

```C
program ::= {global_declaration}+
//程序由一个或多个全局声明组成
global_declaration ::= enum_decl | variable_decl | function_decl
//全局声明可以是枚举声明，变量声明或函数声明
enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
//枚举声明以 enum 开始，可选的跟一个标识符，，然后是一个{，接着是一个或者多个标识符，每个标识符后面可选地跟一个等号和数字，标识符之间用逗号分，最后是一个}结尾
variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
//变量声明以一个类型开始，后面可以有一个或多个星号（表示指针），然后是一个标识符，后面可以跟一个或者多个由逗号分隔的标识符，每个标识前也可以有一个或多个星号，最后以分号；结束
function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'
//函数声明以一个类型开始，后面可以有一个或者多个有星号，然后是一个标识符，接着是一对圆括号内的参数声明，然后是一个{，里面是函数体声明，最后是一个}
parameter_decl ::= type {'*'} id {',' type {'*'} id}
//参数声明是以一个类型开始，后面可以有一个或者多个星号，然后是一个标识符，后面可以跟一个或多个由逗号分隔的参数，每个参数前也可以有一个或多个星号
body_decl ::= {variable_decl}, {statement}
//函数体声明由零个或多个变量声明和零个或多个语句组成
statement ::= non_empty_statement | empty_statement
//语句可以是一个非空语句或一个空语句
non_empty_statement ::= if_statement | while_statement | '{' statement '}'
| 'return' expression | expression ';'
//非空语句可以是一个if语句、while语句、代码块、return语句或一个表达式后跟分号
if_statement ::= 'if' '(' expression ')' statement ['else' non_empty_statement]
//if语句以关键字if 开始，后面是一对圆括号内的表达式，然后是一个语句，可选的跟一个else关键字和另一个非空语句
while_statement ::= 'while' '(' expression ')' non_empty_statement
//while语句以关键字while开始，后面是一堆圆括号内的表达式，然后是一个非空语句
```

其中 `expression` 相关的内容我们放到后面解释，主要原因是我们的语言不支持跨函数递归，但是为了实现自举，实际上我们也不能使用递归（QAQ递归下降用不了了）

#### 解析变量的定义

这一章讲文法中的 `enum_decl` `variable_decl`部分

```C 
int basetype;           //声明类型
int expr_type;          //表达式类型

void global_declaration(){
// global_declaration ::= enum_decl | variable_decl | function_decl
//
// enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
//
// variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
//
// function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    int type;       //用来存储某个变量的实际类型
    int i;          //临时变量用来计数

    basetype = INT;

    //解析enum，需要被单独处理
    if(token = Enum){
        //enum [id]{ a = 10, b = 20 ,....}
        match(Enum);
        if(token != '{'){
            match(Id);//跳过[id]部分
        }
        if(token == '{'){
            //解析等式
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        basetype = CHAR;
    }

    //解析类型信息
    if(token == Int){
        match(Int);
    }else if(token == Char){
        match(Char);
        basetype = CHAR;
    }

    //解析由逗号分隔的变量声明
    while(token != ';' && token != '}'){
        type = basetype;
        //解析指针类型，注意可能会存在这样的形式"int ***** x"
        while(token == Mul){
            match(Mul);
            type = type + PTR;
        }

        if(token != Id){
            //非法的声明
            printf("%d:bad global declaration\n",line);
            exit(-1);
        }
        if(current_id[Class]){
            //当前标识符存在
            printf("%d: duplicate global declaration\n",line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        if(token == '('){
            current_id[Class] = Fun;
            current_id[Value] = (int)(text + 1);        //函数在内存中的地址
            function_declaration();
        }else{
            //变量声明
            current_id[Class] = Glo;                    //全局变量
            current_id[Value] = (int)data;              //分配内存地址
            data = data + sizeof(int);
        }

        if(token == ','){
            match(',');
        }
    }
    next();
}
```

我们讲解上面的部分细节：

**向前看标记**：其中 `if(token == xxx)`语句就是用来向前查看标记以确定使用哪一个生产方式，例如只要遇到 `enum` 我们就知道是需要解析枚举类型。而如果只解析枚举类型，如 `int identifier` 时我们并不能确定 `identifier` 是一个普通的变量还是一个函数，所以还需要继续查看后续的标记，如果遇到 `（`那么就是函数，反之则是标记

**变量类型的表示：**我们编译器支持指针类型，那意味着也支持指针的指针，如 `int *data`,那么我们如何表示指针类型呢？以下是我们支持的类型：

```C
// types of variable/function
enum { CHAR, INT, PTR };
```

所以一个类型首先要有基本类型，如 CHAR 和 INT ，当它是一个指向基本类型的指针时，如 int *data  我们就将它的类型加上 `PTR`即代码中的 `type = type + PTR`同理，如果是指针的指针，则再加上`PTR`

#### enum_declaration()

用于解析枚举类型的定义。主要的逻辑用于解析用逗号（，）分隔的变量，值得注意的是再编译器中如何保存枚举变量的信息。

 即我们将该变量的类别设置成了 Num ，这样他就变成全局的常量了，而注意到上节中，正常的全局变量的类别则是 Glo ,类别信息在后面的章节中解析 expression 会使用到

```c
void enum_declaration(){
    //解析enum [id]{ a = 1, b = 3,....}
    int i;
    i = 0;
    while(token != '}'){
        if(token != Id){
            printf("%d: bad enum identifier %d\n",line,token);
            exit(-1);
        }
        next();
        if(token == Assign){
            //比如{a=10}
            next();
            if(token != Num){
                printf("%d: bad enum initializer\n",line);
                exit(-1);
            }
            i = token_val;
            next();
        }
        
        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;
        
        if (token == ','){
            next();
        }
    }
}
```

#### 其他

其中的function_declaration 函数我们将放到下一章中。这里的match是一个辅助函数：

```C
void match(int tk){
    if(token == tk){
        next();
    }else{
        printf("%d: expected token: %d\n",line,tk);
        exit(-1);
    }
}
```

它将next函数封装起来，如果不是预期的标记就报错退出

### 函数定义

#### EBNF表示

EBNF方法中与函数定义相关的内容

```C
variable_decl ::= type {'*'} id { ',' {'*'} id } ';'

function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

parameter_decl ::= type {'*'} id {',' type {'*'} id}

body_decl ::= {variable_decl}, {statement}

statement ::= non_empty_statement | empty_statement

non_empty_statement ::= if_statement | while_statement | '{' statement '}'
| 'return' expression | expression ';'

if_statement ::= 'if' '(' expression ')' statement ['else' non_empty_statement]

while_statement ::= 'while' '(' expression ')' non_empty_statement
```

#### 解析函数的定义

上一章的代码中，我们已经知道什么时候开始解析函数的定义，相关的代码如下

```c
...
if (token == '(') {
current_id[Class] = Fun;
current_id[Value] = (int)(text + 1); 	//函数在内存中的地址
function_declaration();
} else {
...
```

即在这段代码之前，我们已经为当前的标识符（identifier）设置了正确的类型，上面这段代码为当前的标识符设置了正确的类别（Fun）,以及该函数代码段（text segment）中的位置，接下来开始解析函数定义相关的内容： `parameter_decl` 及 `body_decl`

#### 函数参数与汇编代码

现在我们要回忆如何将"函数"转换成对应的汇编代码，因为这决定了在解析时我们需要哪些相关的信息，考虑下列函数：

```c
int demo(int param_a,int *param_b){
    int local_1;
    char local_2;
    
    ...
}
```

那么它应该被转换成什么样的汇编代码呢？在思考这个问题之前；我们需要了解当 `demo` 函数被调用时，计算机的栈状态，如下（参照虚拟机）：

```
|    ....       |    高位
+---------------+
| arg: param_a  |    new_bp + 3
+---------------+
| arg: param_b  |    new_bp + 2
+---------------+
|return address |    new_bp + 1
+---------------+
| old BP        | <- new BP
+---------------+
| local_1       |    new_bp - 1
+---------------+
| local_2       |    new_bp - 2
+---------------+
|    ....       |    低位
```

这里最重要的一点是，无论是函数的参数（如 `param_a`）还是函数的局部变量（如 `local_l`）都是存放在计算机的栈上的。因此，与存放在数据段中的全局变量不同，在函数内访问它们是通过 `new_bp`指针和对应的位移量进行的。因此，在解析的过程中，我们需要知道参数的个数，各个参数的位移量。

#### 函数定义的解析

这相当于是整个函数定义的语法解析的框架，代码如下：

```C
void function_declaration(){
    //type func_name (...) {...}
    //              | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('});			（1）
    //（2）
    //解除所有变量的局部声明
    current_id = symbols;
    while(current_id[Token]){
        if(current_id[Class] == Loc){
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] =current_id[BValue];
        }
        current_id = current_id + Idsize;
    }
}
```

其中（1）中我们没有消耗最后的 } 字符。这么做的原因是： `variable_decl` 与 `function_decl`是放在一起解析的，而 `variable_decl`是以字符 ；结束的。而 `function_decl` 是以字符 } 结束的，若在此通过 match消耗了；字符，那么外层的 `while` 循环就没法准确的知道函数定义已经结束，所以我们将结束符的解析放在了外层的 `while` 循环中

而（2）中的代码是用于将符号表中的信息恢复成全局的信息。这是因为，局部变量是可以和全局变量同名的，一旦同名，在函数体内局部变量就会覆盖全局变量，出了函数体，全局变量就恢复了原先的作用。这段代码线性地遍历所有的标识符，并将保存在 `BXXX` 中的信息还原

#### 解析参数

```c
parameter_decl ::= type {'*'} id {',' type {'*'} id}
```

解析函数的参数就是解析以逗号分隔的一个个标识符，同时记录它们的位置与类型

```c
int index_of_bp;        //堆栈上指针BP的索引

void function_parameter(){
    int type;
    int params;
    params = 0;
    while(token != ')'){
        //(1)
        //整数名称,...
        type = INT;
        if(token==Int){
            match(Int);
        }else if(token == Char){
            type = CHAR;
            match(Char);
        }

        //指针类型
        while(token == Mul){
            match(Mul);
            type = type + PTR;
        }

        //处理参数名称
        if(token != Id){
            printf("%d: bad parameter declaration\n",line);
            exit(-1);
        }
        if(current_id[Class] == Loc){
            printf("%d: duplicate parameter declaration\n",line);
            exit(-1);
        }

        match(Id);

        //(2)
        //存储局部变量
        current_id[BClass] = current_id[Class];current_id[Class] = Loc;
        current_id[BType] = current_id[Type];  current_id[Type]  = type;
        current_id[BValue]= current_id[Value]; current_id[Value] =params++; //当前的索引

        if(token == ','){
            match(',');
        }
    }
    
    //(3)
    index_of_bp = params+1;
}
```

其中（1）与全局变量定义的解析十分一样，用于解析该参数的类型

而（2）则与上节中提到的"局部变量覆盖全局变量"相关，先将全局的信息保存（无论是否真的在全局中用到了这个变量）在 `BXXX` 中，再附上局部变量相关的信息，如 `Value`中存放的是参数的位置（是第几个参数）

（3）则与汇编代码的生成有关，此处 `index_of_bp` 就是前文提到的 `new_bp`的位置

#### 函数体的解析

我们实现的C语言与现代的C语言不太一样，我们需要所有的变量定义出现再所有的语句之前。

函数体的代码如下：

```c
void function_body(){
    // type func_name (...) {...}
    //                   -->|   |<--

    // ...{
    // 1. 局部变量的定义
    // 2. 代码块
    // }

    int pos_local;              //局部变量在堆栈上的位置
    int type;
    pos_local = index_of_bp;

    //(1)
    while(token == Int || token == Char){
        //局部变量声明，和全局变量声明差不多
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while(token != ';'){
            type = basetype;
            while(token == Mul){
                match(Mul);
                type = type + PTR;
            }

            if(token != Id){
                //非法的声明
                printf("%d: bad local declaration\n",line);
                exit(-1);
            }
            if(current_id[Class]){
                //标识符已经存在
                printf("%d: duplicate local declaration\n",line);
                exit(-1);
            }
            match(Id);

            //存储局部变量
            current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
            current_id[BType] = current_id[Type];   current_id[Type]  = type;
            current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;    //当前的索引

            if(token == ','){
                match(',');
            }
        }
        match(';');
    }

    //(2)
    //保存局部变量的堆栈大小
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    //代码段
    while(token != '}'){
        statement();
    }

    //退出子函数的代码
    *++text = LEV;
}
```

其中（1）用于解析函数体内的局部变量的定义，代码与全局的变量定义几乎一样

而（2）则用于生成汇编代码，我们需要在栈上为局部变量留足空间

### 语句

C语言区分语句（statement）和表达式（expression）两个概念。简单地说，可以认为语句就是表达式加`;`

在我们的编译器中识别以下六种语句：

+ ```c
  if(...) <statement> [else <statement>]
  ```

+ ```c
  while(...) <statement>
  ```

+ ```c
  {<statement>}
  ```

+ ```c
  return xxx;
  ```

+ ```c
  <empty statement>;
  ```

+ ```c
  expression; //以分号结尾
  ```

它们的语法分析都相对简单，在这里我们将这些语句编译成汇编代码

#### IF语句

IF语句的作用是跳转，根据条件表达式决定跳转的位置。我们看看下面的伪代码：

```C
if(...) <statement> [else <statement>]
    
  if (...)           <cond>
                     JZ a
    <statement>      <statement>
  else:              JMP b
a:                 a:
   <statement>      <statement>
b:                 b:
```

对应的汇编代码的流程为：

+ 执行条件表达式为`<cond>`
+ 如果条件失败，则跳转到a的位置，执行else语句。这里else语句是可以省略的，此时a和b都指向IF后方的代码
+ 因为汇编代码是顺序排列的，如果执行了 true_statement，为了防止因为顺序排列而执行了false_statement ，所以需要无条件跳转到 `JMP b`

对应的C代码如下：

```c
if (token == If){
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:                 a:
        //     <statement>      <statement>
        // b:                 b:
        match(If);
        match('(');
        expression(Assign);         //解析条件
        match(')');
        //生成if的代码
        *++text = JZ;
        b = ++text;

        statement();                     //解析代码块
        if(token == Else){               //解析else
            match(Else);
            //生成JMP B的代码
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }
        *b = (int)(text+1);
    }
```

#### While语句

While语句比If语句简单，它对应的汇编代码如下：

```C
while(...) <statement>

a:                     a:
    while (<cond>)        <cond>
                          JZ b
     <statement>          <statement>
                          JMP a
 b:                     b:
```

它的C语言代码如下：

```C
else if(token == While){
    // while(...) <statement>
    // a:                     a:
    //    while (<cond>)        <cond>
    //                          JZ b
    //     <statement>          <statement>
    //                          JMP a
    // b:                     b:
    match(While);

    a = text+1;

    match('(');
    expression(Assign);
    match(')');

    *++text = JZ;
    b = ++text;

    statement();

    *++text = JMP;
    *++text = (int)a;
    *b = (int)(text + 1);
}
```

#### return语句

Return 唯一特殊的地方是：一旦遇到了Return语句，则意味着函数要退出了，所以需要生成汇编代码 `LEV`来标识退出

```c
else if(token == Return){
        // return [expression];
        match(Return);

        if(token != ';'){
            expression(Assign);
        }

        match(';');

        //生成return代码
        *++text = LEV;
    }
```

#### 其他语句

其他语句并不直接生成汇编代码，代码如下：

```c
else if(token == '{'){
    // { <statement> ... }
    match('{');

    while(token != '}'){
        statement();
    }
    match('}');
} else if(token == ';'){
    // empty statement
    match(';');
}else{
    // a=b; 或者 函数的调用(function_call())
    expression(Assign);
    match(';');
}
```

### 表达式

表达式是将各种语言要素的一个组合，用来求值

在这里我们需要知道表达式可以被分为单元（如常量，变量）和 操作符 （如*），且我们在解析表达式时应该先解析单元个一元操作符，最后再是二元运算符

表达式有两种形式 ，如下

```C
 unit_unary ::= unit | unit unary_op | unary_op unit
     // 单元 | 单元 + 一元操作符 | 一元操作符 + 单元
 expr ::= unit_unary (bin_op unit_unary ...)
     // 单元一元表达式 和 二元操作符组成
```

#### 运算符的优先级

运算符的优先级决定了表达式的运算顺序，如在普通的四则运算中，乘法的优先级高于加法，这就意味着表达式 `2 + 3 * 4` 的实际运行顺序是 `2 + (3 * 4)`而不是 `(2 + 3) * 4`

C语言定义了各种表达式的优先级，可以参考C语言运算符优先级

传统的编程书籍会用"逆波兰式"实现四则运算来讲解优先级问题。实际上，优先级关心的就是哪个运算符先计算，哪个运算符后计算。而这就以为着我们需要决定先位哪个运算符生成目标代码（汇编），因为汇编代码是顺序排列的，所以我们必须计算优先级高的运算符

实际上确定运算符的优先级 就是使用栈（递归调用的实质也是栈的处理）

举个例子： `2 + 3 - 4 * 5`，的运算顺序是这样的：

1. 将 2 入栈

2. 遇到运算符 `+`，入栈，此时我们期待的是 `+`的另一个参数

3. 遇到数字 3 ，原则上我们需要立即计算 `2+3`的值，但我们不确定 3 是否属于优先级更高的运算符，所以将它先放入栈中

4. 遇到运算符 `-`，它的优先级和 `+`相同，此时判断参数 3 属于这前的 `+` 将运算符 `+`出栈，并将之前的 2 和 3 出栈 ，计算 `2+3`的结果，得到 5 入栈

5. 遇到数字 4 ，同样不能判断是否立即计算，入栈

6. 遇到运算符 * 优先级大于 - ，入栈

7. 遇到数字 5 ，依旧不能确定是否能立即计算，入栈

8. 表达式结束，运算符出栈，为 `*`，将参数出栈，计算 `4*5`得到结果 20 入栈

9. 运算符出栈，为 `-` ,将参数出栈，计算 `5-20`，得到 -15 入栈

10. 此时运算符栈为空，因此得到结果为 -15

11. 

12. ```
    // after step 1, 2
    |      |
    +------+
    | 3    |   |      |
    +------+   +------+
    | 2    |   | +    |
    +------+   +------+
    
    // after step 4
    |      |   |      |
    +------+   +------+
    | 5    |   | -    |
    +------+   +------+
    
    // after step 7
    |      |
    +------+
    | 5    |
    +------+   +------+
    | 4    |   | *    |
    +------+   +------+
    | 5    |   | -    |
    +------+   +------+
    ```

综上在计算一个运算符 x 之前，必须先查看它的右方，找出并计算所有优先级大于 x 的运算符，之后再计算运算符 x

最后需要注意的是优先通常只与多元运算符有关，单元运算符往往没有这个问题（因为只有一个参数）。也可以认为 "运算符"的实质就是两个运算符在抢参数

#### 一元运算符

优先级一般只与多元运算符有关，因此一元运算符的优先级总高于多元运算符，因此我们需要先对其进行解析

当然，这部分也将同时解析参数本身（如变量，数字，字符串等等）

关于表达式的解析，与语法分析相关的部分就是上文所说的优先级问题，而剩下较难较烦的部分是与目标代码的生成有关的。因此对于需要的运算符，我们先从其目标代码开始入手

##### 常量

首先是数字，用 `IMM`指令将它加载到 `AX`中即可

```C
if(token == Num){
    match(Num);

    //生成代码
    *++text = IMM;
    *++text = token_val;
    expr_type = INT;
}
```

接着是字符串常量。比较特别的地方在于C语言的字符串常量支持这种风格

```c
char *p;
p = "firstline"
    "secondline";
```

它相当于

```c
char *p;
p = "first linesecond line"
```

所以解析的时候要注意一点：

```C
else if(token == '"'){
    //生成代码
    *++text = IMM;
    *++text = token_val;

    match('"');
    //存储其余字节
    while (token == '"'){
        match('"');
    }

    //追加末尾字符“,所有数据默认初始化为0,所以只需将数据前移一位
    data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
    expr_type = PTR ;
}
```

##### sizeof

`sizeof`是一个一元运算符，我们需要知道后面的参数类型，类型的解析在前面已经涉及过了

```c
else if(token == Sizeof){
    //在这个编译器中只支持 sizeof(int) sizeof(char) sizeof(*...)
    match(Sizeof);
    match('(');
    expr_type = INT;
    
    if(token == Int){
        match(Int);
    }else if(token == Char){
        match(Char);
        expr_type = CHAR;
    }
    
    while (token == Mul){
        match(Mul);
        expr_type = expr_type + PTR;
    }
    
    match(')');
    
    //生成代码
    *++text = IMM;
    *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);
    
    expr_type = INT;
}
```

这里我们只支持 sizeof(int) sizeof(char) sizeof(pointer type...) 他们的结果是int

##### 变量与函数调用

由于取变量的值与函数的调用都是以 Id 标记开头的，因此将他们放在一起处理

```c
else if(token == Id){
            //当Id出现时，可能是以下几种类型：
            //1.函数调用
            //2.枚举变量
            //3.全局/局部变量
            match(Id);

            id = current_id;

            if(token == '('){
                //函数调用
                match('(');

                //(1)
                //传入参数
                tmp = 0; //参数数量
                while (token != ')'){
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if(token == ','){
                        match(',');
                    }
                }
                match(')');

                //(2)
                //生成代码
                if (id[Class] == Sys){
                    //内置功能
                    *++text = id[Value];
                }else if(id[Class] == Fun){
                    //函数调用
                    *++text = CALL;
                    *++text = id[Value];
                }else{
                    printf("%d: bad function call\n",line);
                    exit(-1);
                }

                //(3)
                //清理栈中的参数
                if(tmp > 0){
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            }else if(id[Class] == Num){
                //(4)
                //枚举变量声明
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            }else{
                //(5)
                //变量
                if(id[Class] == Loc){
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                }else if(id[Class] == Glo){
                    *++text = IMM;
                    *++text = id[Value];
                }else{
                    printf("%d: undefined variable\n",line);
                    exit(-1);
                }

                //(6)
                //生成代码，默认行为为加载存储在ax中的值
                expr_type = id[Type];
                *++text = (expr_type == Char) ? LC : LI;
            }
        }
```

（1）中注意我们是顺序将参数入栈，这和虚拟机的指令是对应的。与之不同的是，标准C是逆序将参数入栈

（2）中判断函数的类型，如果是内置函数，则直接调用对应的汇编指令，如果是普通的函数则使用 `CALL <addr>`的形式来调用

（3）用于清除入栈的参数。因为我们不在乎出栈的值，所以直接修改栈指针的大小

（4）当标识符是全局定义的枚举类型时，直接将对应的值用 IMM 指令 存入 AX 中即可

（5）则是用于加载变量的值，如果是局部变量则与 `bp`指针相对位置的形式，如果是全局变量则用IMM加载变量的地址。

（6）无论是全局还是局部变量，最终都根据他们的类型用 LC或者LI 指令加载对应的值

**问：**关于变量，如果遇到标识符就用 `LC/LI` 载入对应的值，那诸如 `a[10]`之类的表达式要怎么实现呢？

根据标识符后的运算符，可以修改或删除现有 `LC/LI`指令

##### 强制转换

虽然前面并没有提到，但我们一直用 `expr_type`来保存一个表达式的类型，强制转换的作用就是获取转换的类型，并修改 `expr_type`的值

```C
else if(token == '('){
    //强制转换或者括号
    match('(');
    if(token == Int || token == Char){
        tmp = (token == Char) ? CHAR : INT; //转换类型
        match(token);
        while(token == Mul){
            match(Mul);
            tmp = tmp + PTR;
        }

        match(')');

        expression(Inc);    //类型转换的优先级与后缀自增操作符相同
        
        expr_type = tmp;
    }else{
        //普通的的括号
        expression(Assign);
        match(')');
    }
}
```

##### 指针取值

诸如 `*a`的指针取值，关键是判断 `a`的类型，而就像上节所提到的，当一个表达式解析结束时，它的类型保存在变量 `expr_type`中

```C
else if(token == Mul){
    //解引 *<addr>
    match(Mul);
    expression(Inc);        //和自增操作的优先级相同
    
    if (expr_type >= PTR){
        expr_type = expr_type - PTR;
    }else{
        printf("%d: bad dereference\n",line);
        exit(-1);
    }
    
    *++text = (expr_type == CHAR) ? LC : LI;
}
```

##### 取址操作

这里我们就能看到刚刚所说的 "修改或删除LC/LI指令"。前文中我们说到，对于变量我们会先加载他的地址，并根据他们类型使用 `LC/LI`指令加载实际内容，例如对变量 a:

```ini
IMM <addr>
LI
```

那么对变量 a 取址，其实只要不执行 `LC/LI`即可。因此我们删除相印的指令

```c
else if(token == And){
            //取址
            match(And);
            expression(Inc);        //和自增操作的优先级相同
            if(*text == LC || *text == LI){
                text --;
            }else{
                printf("%d: bad address of\n",line);
                exit(-1);
            }
            
            expr_type = expr_type + PTR;
        }
```

##### 逻辑取反

我们没有直接的逻辑取反指令，因此我们判断它是否与数字0相等。而数字0代表了逻辑 'False'

```C
else if(token == '!'){
    //否
    match('!');
    expression(Inc);        //和自增操作的优先级相同

    //生成代码，即 <expr> == 0;
    *++text = PUSH;
    *++text = IMM;
    *++text = 0;
    *++text = EQ;
    
    expr_type = INT;
}
```

##### 按位取反

同样的我们没有相印的指令所以我们用异或实现，即 `~a = a^0xFFFF`

```C
else if(token == '~'){
    match('~');
    expression(Inc);         //和自增操作的优先级相同
    
    //生成代码，用<expr> XOR-1
    *++text = PUSH;
    *++text = IMM;
    *++text = -1;
    *++text = XOR;
    
    expr_type = INT;
}
```

##### 正符号

注意这里并不是四则运算中的加减法，而是单个数字的取正取负操作。我们没有取负的操作，用 `0-x`来实现

```C
else if(token == Sub){
    //负数
    match(Sub);

    if(token == Num){
        *++text = IMM;
        *++text = -token_val;
        match(Num);
    }else{
        
        *++text = IMM;
        *++text = -1;
        *++text = PUSH;
        expression(Inc);
        *++text = MUL;
    }
    
    expr_type = INT;
}
```

##### 自增自减

注意的是自增自减操作的优先级与其位置有关。如 `++p` 的优先级高于 `p++`，这里我们解析的就是类似 `++p`的操作

```C
else if(token == Inc || token == Dec){
    tmp = token;
    match(token);
    expression(Inc);
    //(1)
    if(*text == LC){
        *text = PUSH;       //复制地址
        *++text = LC;
    } else if(*text == LI){
        *text = PUSH;
        *++text = LI;
    }else{
        printf("%d: bad lvalue of pre-increment\n",line);
        exit(-1);
    }
    *++text = PUSH;
    *++text = IMM;
    //(2)
    *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
    *++text = (tmp = Inc) ? ADD : SUB;
    *++text = (expr_type == CHAR) ? SC : SI;
}
```

（1）在实现 ++p 时，我们要用变量 p 的地址两次，所以我们需要先将其PUSH

（2）这一部分是因为自增自减要处理时指针的情况

#### 二元运算符

这里，我们许哟啊处理多运算符的优先级问题，例如之前提到的优先级，我们需要不断地向右扫描，直到遇见优先级小于当前优先级的运算符

回想起我们之前定义的标记，他们是以优先级从低到高排列的，即 `Assign`的优先级最低，而Brak ( [ )的优先级最高

```C
enum {
Num = 128, Fun, Sys, Glo, Loc, Id,
Char, Else, Enum, If, Int, Return, Sizeof, While,
Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};
```

所以当我们调用`expression(level)`进行解析的时候，我们其实通过了参数`level`指定了当前的优先级

所以此时的二元运算符号的解析的框架为：

```c
while (token >= level){
    //解析二元运算符和后缀运算符
}
```

解决了优先级的问题之后，将其编译成汇编代码

##### 赋值操作

赋值操作是优先级别最低的运算符。考虑诸如 `a = (expression)` 的表达式，在解析 = 之前，我们已经有了以下汇编

```ini
IMM <addr>
LC/LI
```

当解析完 = 右边的表达式后，相应的值会存放在 ax 中，此时，为了实际将这个值保存起来，我们需要类似下面的汇编

```ini 
IMM <addr>
PUSH
SC/SI
```

所以我们可以写出以下代码

```C
if(token == Assign){
    //var = expr;
    match(Assign);
    if(*text == LC || *text == LI){
        *text = PUSH;       //保存左值的指针
    }else{
        printf("%d: bad lvalue in assignment\n",line);
        exit(-1);
    }
    expression(Assign);
    
    expr_type = tmp;
    *++text = (expr_type == CHAR) ? SC : SI;
}
```

##### 三目运算符

这是C语言中的唯一一个三元运算符，它相当于一个小型的If语句，所以生成的代码也类似If语句

```C
else if(token == Cond){
    //expr ? a : b;
    match(Cond);
    *++text = JZ;
    addr = ++text;
    expression(Assign);
    if (token == ':'){
        match(':');
    }else{
        printf("%d: missing colon in conditional\n",line);
        exit(-1);
    }
    *addr = (int)(text + 3);
    *++text = JMP;
    addr = ++text;
    expression(Cond);
    *addr = (int)(text + 1);
}
```

##### 逻辑运算符

这包括 || 和 &&。他们的汇编代码如下：

```c
<expr1> || <expr2>     <expr1> && <expr2>

...<expr1>...          ...<expr1>...
JNZ b                  JZ b
...<expr2>...          ...<expr2>...
b:                     b:
```

所以源码如下：

```C
else if(token == Lor){
    //或
    match(Lor);
    *++text = JNZ;
    addr = ++text;
    expression(Lan);
    *addr = (int)(text + 1);
    expr_type = INT ;
}else if(token == Lan){
    //与
    match(Lan);
    *++text = JZ;
    addr = ++text;
    expression(Or);
    *addr = (int)(text + 1);
    expr_type = INT;
}
```

##### 数学运算符

###### 异或

他们包括 `|  ^  &  ==  !=  <=  >=  <  >  <<  >>  +  -  *  /  %`它们的实现都很类似，我们以异或为例：

```
<expr1> ^ <expr2>

...<expr1>...          <- now the result is on ax
PUSH
...<expr2>...          <- now the value of <expr2> is on ax
XOR
```

所以它源码是：

```C
else if(token == Xor){
    //异或
    match(Xor);
    *++text = PUSH;
    expression(And);
    *++text = XOR;
    expr_type = INT;
}
```

###### 与 

```C
else if(token == And){
    //与
    match(And);
    *++text = PUSH;
    expression(Eq);
    *++text = AND;
    expr_type = INT;
}
```

###### 或

```C
else if(token == OR){
    //或
    match(Or);
    *++text = PUSH;
    expression(Xor);
    *++text = OR;
    expr_type = INT;
}
```

###### 相等

```C
else if(token == Eq){
    //相等
    match(And);
    *++text = PUSH;
    expression(Ne);
    *++text = EQ;
    expr_type = INT;
}
```

###### 不相等

```C
else if(token == Ne){
    //不相等
    match(Ne);
    *++text = PUSH;
    expression(Lt);
    *++text = NE;
    expr_type = INT;
}
```

###### 小于大于

```C
else if(token == Lt){
    //小于
    match(Lt);
    *++text = PUSH;
    expression(Shl);
    *++text = LT;
    expr_type = INT;
}else if(token == Gt){
    //大于
    match(Gt);
    *++text = PUSH;
    expression(Shl);
    *++text = GT ;
    expr_type = INT;
}
```

###### 小于等于大于等于

```C
else if(token == Le){
    //小于等于
    match(Le);
    *++text = PUSH;
    expression(Shl);
    *++text = LE;
    expr_type = INT;
}else if(token == Ge){
    //大于等于
    match(Ge);
    *++text = PUSH;
    expression(Shl);
    *++text = GE;
    expr_type = INT;
}
```

###### 左移

```C
else if(token == Shl){
    //左移
    match(Shl);
    *++text = PUSH;
    expression(Add);
    *++text = SHL;
    expr_type = INT;
}
```

###### 右移

```C
else if(token == Shr){
    //右移
    match(Shr);
    *++text = PUSH;
    expression(Add);
    *++text = SHR;
    expr_type = INT;
}
```

其中这里还有一个问题，那就是指针的加减。在C语言中，指针加上数值等于指针移位，而且根据不同的类型移动的位移不同。如 `a+1`，如果 a 是 `char *`型，则移动一字节，如果是 `int *`型，则移动四个字节（32位系统）

另外，在做指针的减法时，如果是两个指针相减（相同类型），则结果是两个指针间隔的元素个数。因此需特殊处理

下面的加法为例，对应的汇编代码为：

```ini
<expr1> + <expr2>

normal         pointer

<expr1>        <expr1>
PUSH           PUSH
<expr2>        <expr2>     |
ADD            PUSH        | <expr2> * <unit>
               IMM <unit>  |
               MUL         |
               ADD
```

即当 `<expr1>`是指针时，要根据它的类型放大 `<expr2>`的值，因此对应的源码如下：

###### 加法

```C
else if(token == Add){
    //加法
    match(Add);
    *++text = PUSH;
    expression(Mul);

    expr_type = tmp;
    if(expr_type > PTR){
        //指针类型，且不是char *型
        *++text = PUSH;
        *++text = IMM;
        *++text = sizeof(int);
        *++text = MUL;
    }
    *++text = ADD;
}
```

###### 减法

```C
else if(token == Sub){
    //减法
    match(Sub);
    *++text = PUSH;
    expression(Mul);
    if(tmp > PTR && tmp == expr_type){
        //指针的减法
        *++text = SUB;
        *++text = PUSH;
        *++text = IMM;
        *++text = sizeof(int);
        *++text = DIV;
        expr_type = INT;
    }else if(tmp > PTR){
        //移动指针
        *++text = PUSH;
        *++text = IMM;
        *++text = sizeof(int);
        *++text = MUL;
        *++text = SUB;
        expr_type = tmp;
    }else{
        //数字减法
        *++text = SUB;
        expr_type = tmp;
    }
}
```

###### 乘法

```C
else if(token == Mul){
    //乘法
    match(Mul);
    *++text = PUSH;
    expression(Inc);
    *++text = MUL;
    expr_type = tmp;
}
```

###### 除法

```C
else if(token == Div){
    //除法
    match(Div);
    *++text = PUSH;
    expression(Inc);
    *++text = Div;
    expr_type = tmp;
}
```

###### 模

```C
else if(token == Mod){
    //模
    match(Mod);
    *++text = PUSH;
    expression(Inc);
    *++text = MOD;
    expr_type = tmp;
}
```

##### 自增自减

这次是后缀形式，即 `p++`或 `p--`。与前缀形式不同的是，执行自增自减操作后，ax上需要保留原来的值。所以我们首先执行类似前缀自增自减的操作，再将 ax 中的值执行减/增的操作

```C
// 前缀形式 生成汇编代码
*++text = PUSH;
*++text = IMM;
*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
*++text = (tmp == Inc) ? ADD : SUB;
*++text = (expr_type == CHAR) ? SC : SI;

// 后缀形式 生成汇编代码
*++text = PUSH;
*++text = IMM;
*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
*++text = (token == Inc) ? ADD : SUB;
*++text = (expr_type == CHAR) ? SC : SI;
*++text = PUSH; 
*++text = IMM; // 执行相反的增/减操作
*++text = (expr_type > PTR) ? sizeof(int) : sizeof(char); 
*++text = (token == Inc) ? SUB : ADD; 
```
由此可以写出后缀自增自减的代码


```C
else if(token == Inc || token == Dec){
    //后缀形式的自增自减运算，我们将其增减后，可以在ax上获取他的原始值
    if(*text == LI){
        *text = PUSH;
        *++text = LI;
    }else if(*text == LC){
        *text = PUSH;
        *++text = LC;
    }else{
        printf("%d: bad value in increment\n",line);
        exit(-1);
    }
    
    *++text = PUSH;
    *++text = IMM;
    *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
    *++text = (token == Inc) ? ADD : SUB;
    *++text = (expr_type == CHAR) ? SC : SI;
    *++text = PUSH;
    *++text = IMM;
    *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
    *++text = (token == Inc) ? SUB : ADD;
    match(token);
}
```

##### 数组取值操作

在C语言学习的过程中我们早已经直到 ， `a[10]`的操作等价于 `*(a + 10)`因此我们要做的就是生成类似的汇编代码

```C
else if(token == Brak){
    //数组操作 var[XX]
    match(Brak);
    *++text = PUSH;
    expression(Assign);
    match(']');

    if(tmp > PTR){
        //指针，但不是char *
        *++text = PUSH;
        *++text = IMM;
        *++text = sizeof(int);
        *++text = MUL;
    }else if(tmp < PTR){
        printf("%d: pointer type expected\n",line);
        exit(-1);
    }
    expr_type = tmp - PTR;
    *++text = ADD;
    *++text = (expr_type == CHAR) ? LC : LI;
}
```

最后补上一个错误处理就好啦

```C
else{
    printf("%d: compiler error,token = %d",line,token);
    exit(-1);
}
```



## 结尾

除了上述对表达式的解析外，我们还需要初始化虚拟机的栈，使我们可以正确的调用 main函数，且当main函数结束时退出进程。

```C
program();

if(!(pc = (int *)idmain[Value])){
    printf("main() not defined\n");
    return -1;
}

//栈初始化
sp = (int *)((int)stack + poolsize);
*--sp = EXIT;           //使用exit用来退出main
*--sp = PUSH; tmp = sp;
*--sp = argc;
*--sp = (int)argv;
*--sp = (int)tmp;
```

最后让我们附上总代码

```C
#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<string.h>
#include<fcntl.h>

int token;              //设置一个标记
char *src,*old_src;     //设置一个指向源代码的指针
int poolsize;           //设置一个内存池用来存放数据
int line;               //用来追踪源代码的行号
int *text,              //代码段
    *old_text,          //用于保存使用过的代码
    *stack;             //栈
char *data;             //数据段
int *pc,*bp,*sp,ax,cycle;       //虚拟机中的寄存器

int token_val;          //当前标记的值
int *current_id,        //当前标识符的ID信息
    *symbols;           //符号表

int *idmain;            //main功能

int basetype;           //声明类型
int expr_type;          //表达式类型

int index_of_bp;        //堆栈上指针BP的索引

//符号表的条目
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, Idsize};

//指令集
enum {
    LEA ,IMM ,JMP ,CALL ,JZ ,JNZ ,ENT ,ADJ ,LEV ,LI ,LC ,SI ,SC ,PUSH,
    OR ,XOR ,AND ,EQ ,NE ,LT ,GT ,LE ,GE ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD,
    OPEN ,READ ,CLOS ,PRTF ,MALC ,MSET ,MCMP ,EXIT
};

//标记和类别（将运算符放在最后，按照优先级顺序排列）
enum {
     Num = 128, Fun, Sys, Glo, Loc, Id,
     Char, Else, Enum, If, Int, Return, Sizeof, While,
     Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

enum {CHAR,INT,PTR};        //变量的类型功能

void next(){
    char *last_pos;
    int hash;

    while (token == *src){
        ++src;
        //这部分用来解析标识位
        if (token == '\n'){
            ++line;
        }else if(token == '#'){
            //跳过宏定义，此编译器不支持
            while(*src != 0 && *src != '\n'){
                src++;
            }
        }else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')){

            //解析标识符
            last_pos = src -1;
            hash = token;

            while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')){
                hash = hash* 147 + *src;
                src++;
            }

            //查找现有的标识符，线性查找
            current_id = symbols;
            while (current_id[Token]){
                if(current_id[Hash] == hash && !memcmp((char*)current_id[Name],last_pos,src-last_pos)){
                    //找到符合的便返回
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + Idsize;
            }

            //展示新的ID
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        } else if(token >= '0' && token <= '9'){
            //处理数字，三种类型：十进制，八进制，十六进制
            token_val = token - '0';
            if (token_val>0){
                //十进制，开始于[0~9]
                while(*src >= '0' && *src <= '9'){
                    token_val = token_val*10 + *src++ - '0';
                }
            }else{
                //从0开始
                if(*src == 'x' || *src == 'X'){
                    //十六进制
                    token = *++src;
                    while((token >= '0'&& token <= '9' )||(token >= 'a'&&token <= 'f')||(token >= 'A'&&token <= 'F')){
                        token_val = token_val * 16 + (token >= 'A'? 9:0);
                        token = *++src;
                    }
                }else{
                    //八进制
                    while(*src >= '0' && *src <= '7'){
                        token_val = token_val*8 + *src++ - '0';
                    }
                }
            }
            token = Num;
            return;
        }else if(token == '"' || token == '\''){
            //解析字符串
            last_pos = data;
            while(*src != 0 && *src != token){
                token_val = *src++;
                if(token_val == '\\'){
                    //抛弃字符串
                    token_val = *src++;
                    if(token_val == 'n'){
                        token_val = '\n';
                    }
                }
                if(token == '"'){
                    *data++ = token_val;
                }
            }
            src++;
            //如果是单个的字符，返回为Num标识
            if(token == '"'){
                token_val = (int)last_pos;
            }else{
                token = Num;
            }
            return;
        }else if(token == '/'){
            if(*src == '/'){
                //跳过内容
                while (*src != 0 && *src != '\n'){
                    ++src ;
                }
            }else{
                //做除法操作
                token = Div;
                return;
            }
        }else if(token == '='){
            //解析==和=
            if(*src == '='){
                src++;
                token = Eq;
            }else{
                token = Assign;
            }
            return;
        }else if(token == '+'){
            //解析++和+
            if(*src == '+'){
                src++;
                token = Inc;
            }else{
                token = Add;
            }
            return;
        }else if(token == '-'){
            //解析-- 和-
            if(*src == '-'){
                src ++;
                token = Dec;
            }else{
                token = Sub;
            }
            return;
        }else if(token == '!'){
            //解析 !=
            if(*src == '='){
                src++;
                token = Ne;
            }
            return;
        }else if(token == '<'){
            //解析 <=,<<,<
            if(*src == '='){
                src++;
                token =Le;
            }else if(*src == '<'){
                src++;
                token = Shl;
            }else{
                token = Lt;
            }
            return;
        }else if(token == '>'){
            //解析 >=,>>,>
            if(*src == '='){
                src++;
                token = Ge;
            }else if(*src == '>'){
                src++;
                token = Shr;
            } else{
                token = Gt;
            }
            return;
        }else if(token == '|'){
            //解析 | 和||
            if(*src == '|'){
                src ++;
                token = Lor;
            }else{
                token = Or;
            }
            return;
        }else if(token == '&'){
            //解析 & 和 &&
            if(*src == '&'){
                src ++;
                token = Lan;
            }else{
                token = And;
            }
            return;
        }else if(token == '^'){
            token = Xor;
            return;
        }else if(token == '%'){
            token = Mod;
            return;
        }else if(token == '*'){
            token = Mul;
            return;
        }else if(token =='['){
            token = Brak;
            return;
        }else if(token == '?'){
            token = Cond;
            return;
        }else if(token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':'){
            //直接返回即可
            return;
        }
    }
    return;
}

void match(int tk){
    if(token == tk){
        next();
    }else{
        printf("%d: expected token: %d\n",line,tk);
        exit(-1);
    }
}

void expression(int level){
    // unit_unary ::= unit | unit unary_op | unary_op unit
    // expr ::= unit_unary (bin_op unit_unary ...)

    //unit_unary()
    int *id;        //标识类型
    int tmp;        //存储临时变量
    int *addr;      //存储地址
    {
        if(!token){
            printf("%d: unexpect token EOF of expression\n",line);
            exit(-1);
        }
        if(token == Num){
            match(Num);

            //生成代码
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        }else if(token == '"'){
            //生成代码
            *++text = IMM;
            *++text = token_val;

            match('"');
            //存储其余字节
            while (token == '"'){
                match('"');
            }

            //追加末尾字符“,所有数据默认初始化为0,所以只需将数据前移一位
            data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
            expr_type = PTR ;
        }else if(token == Sizeof){
            //在这个编译器中只支持 sizeof(int) sizeof(char) sizeof(*...)
            match(Sizeof);
            match('(');
            expr_type = INT;

            if(token == Int){
                match(Int);
            }else if(token == Char){
                match(Char);
                expr_type = CHAR;
            }

            while (token == Mul){
                match(Mul);
                expr_type = expr_type + PTR;
            }

            match(')');

            //生成代码
            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        }else if(token == Id){
            //当Id出现时，可能是以下几种类型：
            //1.函数调用
            //2.枚举变量
            //3.全局/局部变量
            match(Id);

            id = current_id;

            if(token == '('){
                //函数调用
                match('(');

                //(1)
                //传入参数
                tmp = 0; //参数数量
                while (token != ')'){
                    expression(Assign);
                    *++text = PUSH;
                    tmp ++;

                    if(token == ','){
                        match(',');
                    }
                }
                match(')');

                //(2)
                //生成代码
                if (id[Class] == Sys){
                    //内置功能
                    *++text = id[Value];
                }else if(id[Class] == Fun){
                    //函数调用
                    *++text = CALL;
                    *++text = id[Value];
                }else{
                    printf("%d: bad function call\n",line);
                    exit(-1);
                }

                //(3)
                //清理栈中的参数
                if(tmp > 0){
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            }else if(id[Class] == Num){
                //(4)
                //枚举变量声明
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            }else{
                //(5)
                //变量
                if(id[Class] == Loc){
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                }else if(id[Class] == Glo){
                    *++text = IMM;
                    *++text = id[Value];
                }else{
                    printf("%d: undefined variable\n",line);
                    exit(-1);
                }

                //(6)
                //生成代码，默认行为为加载存储在ax中的值
                expr_type = id[Type];
                *++text = (expr_type == Char) ? LC : LI;
            }
        }else if(token == '('){
            //强制转换或者括号
            match('(');
            if(token == Int || token == Char){
                tmp = (token == Char) ? CHAR : INT; //转换类型
                match(token);
                while(token == Mul){
                    match(Mul);
                    tmp = tmp + PTR;
                }

                match(')');

                expression(Inc);    //类型转换的优先级与后缀自增操作符相同

                expr_type = tmp;
            }else{
                //普通的的括号
                expression(Assign);
                match(')');
            }
        }else if(token == Mul){
            //解引 *<addr>
            match(Mul);
            expression(Inc);        //和自增操作的优先级相同

            if (expr_type >= PTR){
                expr_type = expr_type - PTR;
            }else{
                printf("%d: bad dereference\n",line);
                exit(-1);
            }

            *++text = (expr_type == CHAR) ? LC : LI;
        }else if(token == And){
            //取址
            match(And);
            expression(Inc);        //和自增操作的优先级相同
            if(*text == LC || *text == LI){
                text --;
            }else{
                printf("%d: bad address of\n",line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        }else if(token == '!'){
            //否
            match('!');
            expression(Inc);        //和自增操作的优先级相同

            //生成代码，即 <expr> == 0;
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        }else if(token == '~'){
            match('~');
            expression(Inc);         //和自增操作的优先级相同

            //生成代码，用<expr> XOR-1
            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        }else if(token == Add){
            //正数，什么也不会发生
            match(Add);
            expression(Inc);         //和自增操作的优先级相同

            expr_type = INT;
        }else if(token == Sub){
            //负数
            match(Sub);

            if(token == Num){
                *++text = IMM;
                *++text = -token_val;
                match(Num);
            }else{

                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
            }

            expr_type = INT;
        }else if(token == Inc || token == Dec){
            tmp = token;
            match(token);
            expression(Inc);
            //(1)
            if(*text == LC){
                *text = PUSH;       //复制地址
                *++text = LC;
            } else if(*text == LI){
                *text = PUSH;
                *++text = LI;
            }else{
                printf("%d: bad lvalue of pre-increment\n",line);
                exit(-1);
            }
            *++text = PUSH;
            *++text = IMM;
            //(2)
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp = Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        }
    }

    //二元运算符和后缀运算
    {
        while(token >= level){
            //根据当前的操作优先级进行处理
            tmp = expr_type;
            if(token == Assign){
                //var = expr;
                match(Assign);
                if(*text == LC || *text == LI){
                    *text = PUSH;       //保存左值的指针
                }else{
                    printf("%d: bad lvalue in assignment\n",line);
                    exit(-1);
                }
                expression(Assign);

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            }else if(token == Cond){
                //expr ? a : b;
                match(Cond);
                *++text = JZ;
                addr = ++text;
                expression(Assign);
                if (token == ':'){
                    match(':');
                }else{
                    printf("%d: missing colon in conditional\n",line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                expression(Cond);
                *addr = (int)(text + 1);
            }else if(token == Lor){
                //或者
                match(Lor);
                *++text = JNZ;
                addr = ++text;
                expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT ;
            }else if(token == Lan){
                //且
                match(Lan);
                *++text = JZ;
                addr = ++text;
                expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            }else if(token == Xor){
                //异或
                match(Xor);
                *++text = PUSH;
                expression(And);
                *++text = XOR;
                expr_type = INT;
            }else if(token == And){
                //与
                match(And);
                *++text = PUSH;
                expression(Eq);
                *++text = AND;
                expr_type = INT;
            }else if(token == OR){
                //或
                match(Or);
                *++text = PUSH;
                expression(Xor);
                *++text = OR;
                expr_type = INT;
            }else if(token == Eq){
                //相等
                match(And);
                *++text = PUSH;
                expression(Ne);
                *++text = EQ;
                expr_type = INT;
            }else if(token == Ne){
                //不相等
                match(Ne);
                *++text = PUSH;
                expression(Lt);
                *++text = NE;
                expr_type = INT;
            }else if(token == Lt){
                //小于
                match(Lt);
                *++text = PUSH;
                expression(Shl);
                *++text = LT;
                expr_type = INT;
            }else if(token == Gt){
                //大于
                match(Gt);
                *++text = PUSH;
                expression(Shl);
                *++text = GT ;
                expr_type = INT;
            }else if(token == Le){
                //小于等于
                match(Le);
                *++text = PUSH;
                expression(Shl);
                *++text = LE;
                expr_type = INT;
            }else if(token == Ge){
                //大于等于
                match(Ge);
                *++text = PUSH;
                expression(Shl);
                *++text = GE;
                expr_type = INT;
            }else if(token == Shl){
                //左移
                match(Shl);
                *++text = PUSH;
                expression(Add);
                *++text = SHL;
                expr_type = INT;
            }else if(token == Shr){
                //右移
                match(Shr);
                *++text = PUSH;
                expression(Add);
                *++text = SHR;
                expr_type = INT;
            }else if(token == Add){
                //加法
                match(Add);
                *++text = PUSH;
                expression(Mul);

                expr_type = tmp;
                if(expr_type > PTR){
                    //指针类型，且不是char *型
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            }else if(token == Sub){
                //减法
                match(Sub);
                *++text = PUSH;
                expression(Mul);
                if(tmp > PTR && tmp == expr_type){
                    //指针的减法
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                }else if(tmp > PTR){
                    //移动指针
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                }else{
                    //数字减法
                    *++text = SUB;
                    expr_type = tmp;
                }
            }else if(token == Mul){
                //乘法
                match(Mul);
                *++text = PUSH;
                expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            }else if(token == Div){
                //除法
                match(Div);
                *++text = PUSH;
                expression(Inc);
                *++text = Div;
                expr_type = tmp;
            }else if(token == Mod){
                //模
                match(Mod);
                *++text = PUSH;
                expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            }else if(token == Inc || token == Dec){
                //后缀形式的自增自减运算，我们将其增减后，可以在ax上获取他的原始值
                if(*text == LI){
                    *text = PUSH;
                    *++text = LI;
                }else if(*text == LC){
                    *text = PUSH;
                    *++text = LC;
                }else{
                    printf("%d: bad value in increment\n",line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                match(token);
            }else if(token == Brak){
                //数组操作 var[XX]
                match(Brak);
                *++text = PUSH;
                expression(Assign);
                match(']');

                if(tmp > PTR){
                    //指针，但不是char *
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }else if(tmp < PTR){
                    printf("%d: pointer type expected\n",line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            }else{
                printf("%d: compiler error,token = %d",line,token);
                exit(-1);
            }
        }
    }
}

void statement(){
    // 1. if (...) <statement> [else <statement>]
    // 2. while (...) <statement>
    // 3. { <statement> }
    // 4. return xxx;
    // 5. <empty statement>;
    // 6. expression; (expression end with semicolon)

    int *a,*b;                          //用于控制分支

    if (token == If){
        // if (...) <statement> [else <statement>]
        //
        //   if (...)           <cond>
        //                      JZ a
        //     <statement>      <statement>
        //   else:              JMP b
        // a:                 a:
        //     <statement>      <statement>
        // b:                 b:
        match(If);
        match('(');
        expression(Assign);         //解析条件
        match(')');
        //生成if的代码
        *++text = JZ;
        b = ++text;

        statement();                     //解析代码块
        if(token == Else){               //解析else
            match(Else);
            //生成JMP B的代码
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            statement();
        }
        *b = (int)(text+1);
    }else if(token == While){
        // while(...) <statement>
        // a:                     a:
        //    while (<cond>)        <cond>
        //                          JZ b
        //     <statement>          <statement>
        //                          JMP a
        // b:                     b:
        match(While);

        a = text+1;

        match('(');
        expression(Assign);
        match(')');

        *++text = JZ;
        b = ++text;

        statement();

        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text + 1);
    }else if(token == Return){
        // return [expression];
        match(Return);

        if(token != ';'){
            expression(Assign);
        }

        match(';');

        //生成return代码
        *++text = LEV;
    } else if(token == '{'){
        // { <statement> ... }
        match('{');

        while(token != '}'){
            statement();
        }
        match('}');
    } else if(token == ';'){
        // empty statement
        match(';');
    }else{
        // a=b; 或者 函数的调用(function_call())
        expression(Assign);
        match(';');
    }
}

void function_parameter(){
    int type;
    int params;
    params = 0;
    while(token != ')'){
        //(1)
        //整数名称,...
        type = INT;
        if(token==Int){
            match(Int);
        }else if(token == Char){
            type = CHAR;
            match(Char);
        }

        //指针类型
        while(token == Mul){
            match(Mul);
            type = type + PTR;
        }

        //处理参数名称
        if(token != Id){
            printf("%d: bad parameter declaration\n",line);
            exit(-1);
        }
        if(current_id[Class] == Loc){
            printf("%d: duplicate parameter declaration\n",line);
            exit(-1);
        }

        match(Id);

        //(2)
        //存储局部变量
        current_id[BClass] = current_id[Class];current_id[Class] = Loc;
        current_id[BType] = current_id[Type];  current_id[Type]  = type;
        current_id[BValue]= current_id[Value]; current_id[Value] =params++; //当前的索引

        if(token == ','){
            match(',');
        }
    }

    //(3)
    index_of_bp = params+1;
}

void function_body(){
    // type func_name (...) {...}
    //                   -->|   |<--

    // ...{
    // 1. 局部变量的定义
    // 2. 代码块
    // }

    int pos_local;              //局部变量在堆栈上的位置
    int type;
    pos_local = index_of_bp;

    //(1)
    while(token == Int || token == Char){
        //局部变量声明，和全局变量声明差不多
        basetype = (token == Int) ? INT : CHAR;
        match(token);

        while(token != ';'){
            type = basetype;
            while(token == Mul){
                match(Mul);
                type = type + PTR;
            }

            if(token != Id){
                //非法的声明
                printf("%d: bad local declaration\n",line);
                exit(-1);
            }
            if(current_id[Class]){
                //标识符已经存在
                printf("%d: duplicate local declaration\n",line);
                exit(-1);
            }
            match(Id);

            //存储局部变量
            current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
            current_id[BType] = current_id[Type];   current_id[Type]  = type;
            current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;    //当前的索引

            if(token == ','){
                match(',');
            }
        }
        match(';');
    }

    //(2)
    //保存局部变量的堆栈大小
    *++text = ENT;
    *++text = pos_local - index_of_bp;

    //代码段
    while(token != '}'){
        statement();
    }

    //退出子函数的代码
    *++text = LEV;
}

void function_declaration(){
    //type func_name (...) {...}
    //              | this part

    match('(');
    function_parameter();
    match(')');
    match('{');
    function_body();
    //match('});
    //解除所有变量的局部声明
    current_id = symbols;
    while(current_id[Token]){
        if(current_id[Class] == Loc){
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] =current_id[BValue];
        }
        current_id = current_id + Idsize;
    }
}

void enum_declaration(){
    //解析enum [id]{ a = 1, b = 3,....}
    int i;
    i = 0;
    while(token != '}'){
        if(token != Id){
            printf("%d: bad enum identifier %d\n",line,token);
            exit(-1);
        }
        next();
        if(token == Assign){
            //比如{a=10}
            next();
            if(token != Num){
                printf("%d: bad enum initializer\n",line);
                exit(-1);
            }
            i = token_val;
            next();
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        if (token == ','){
            next();
        }
    }
}

void global_declaration(){
// global_declaration ::= enum_decl | variable_decl | function_decl
//
// enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'} '}'
//
// variable_decl ::= type {'*'} id { ',' {'*'} id } ';'
//
// function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'

    int type;       //用来存储某个变量的实际类型
    int i;          //临时变量用来计数

    basetype = INT;

    //解析enum，需要被单独处理
    if(token = Enum){
        //enum [id]{ a = 10, b = 20 ,....}
        match(Enum);
        if(token != '{'){
            match(Id);//跳过[id]部分
        }
        if(token == '{'){
            //解析等式
            match('{');
            enum_declaration();
            match('}');
        }

        match(';');
        basetype = CHAR;
    }

    //解析类型信息
    if(token == Int){
        match(Int);
    }else if(token == Char){
        match(Char);
        basetype = CHAR;
    }

    //解析由逗号分隔的变量声明
    while(token != ';' && token != '}'){
        type = basetype;
        //解析指针类型，注意可能会存在这样的形式"int ***** x"
        while(token == Mul){
            match(Mul);
            type = type + PTR;
        }

        if(token != Id){
            //非法的声明
            printf("%d:bad global declaration\n",line);
            exit(-1);
        }
        if(current_id[Class]){
            //当前标识符存在
            printf("%d: duplicate global declaration\n",line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;

        if(token == '('){
            current_id[Class] = Fun;
            current_id[Value] = (int)(text + 1);        //函数在内存中的地址
            function_declaration();
        }else{
            //变量声明
            current_id[Class] = Glo;                    //全局变量
            current_id[Value] = (int)data;              //分配内存地址
            data = data + sizeof(int);
        }

        if(token == ','){
            match(',');
        }
    }
    next();
}

void program(){
    next();     //读取下一个标记
    while(token > 0){
        global_declaration();
    }
}

int eval(){
    int op,*tmp;
    while(1){
        op = *pc++;
        //汇编指令
        if(op == IMM){ax = *pc++;}                              //向ax中添加立即数
        else if(op == LC){ax = *(char *)ax;}                    //从ax中加载一个字符到ax
        else if(op == LI){ax = *(int *)ax;}                     //从ax中加载一个整数到ax
        else if(op == SC){ax = *(char *)*sp++ = ax;}            //保存ax中的字符到当前地址，并且更新栈指针
        else if(op == SI){*(int *)*sp++ = ax;}                  //保存ax中的整数到当前地址，并且更新栈指针
        else if(op == PUSH){*--sp = ax;}                        //将ax的值压入栈中
        else if(op == JMP){pc = (int *)*pc;}                    //跳跃到指定的地址
        else if(op == JZ){pc = ax ? pc+1 : (int *)*pc;}         //如果ax为0就跳转
        else if(op == JNZ){pc = ax ? (int *)*pc : pc+1;}        //如果ax不为0就跳转
        else if(op == CALL){*--sp = (int)(pc+1);pc = (int *)*pc;}       //将下一条指令的位置存在栈顶 并且跳转到指定的地址
//        else if(op == RET){pc = (int *)*sp++;}                //从栈顶获取返回地址 使得执行流程回到调用子函数之前
        else if(op == ENT){*--sp = (int)bp; bp = sp; sp = sp - *pc++;}  //创建一个新的栈顶
        else if(op == ADJ){sp = sp + *pc++;}                    //等同于 add esp, <size>   pc指向的值便是参数的大小
        else if(op == LEV){sp = bp;bp = (int *)*sp++;pc = (int *)*sp++;}//恢复调用帧和程序计数器
        else if(op == LEA){ax = (int)(bp + *pc++);}             //计算参数的地址并将其存储到ax中
        //运算符
        else if(op == OR){ax = *sp++ | ax;}
        else if(op == XOR){ax = *sp++ ^ ax;}
        else if(op == AND){ax = *sp++ & ax;}
        else if(op == EQ){ax = *sp++ == ax;}
        else if(op == NE){ax = *sp++ != ax;}
        else if(op == LT){ax = *sp++ < ax;}
        else if(op == LE){ax = *sp++ <= ax;}
        else if(op == GT){ax = *sp++ > ax;}
        else if(op == GE){ax = *sp++ >= ax;}
        else if(op == SHL){ax = *sp++ << ax;}
        else if(op == SHR){ax = *sp++ >> ax;}
        else if(op == ADD){ax = *sp++ + ax;}
        else if(op == SUB){ax = *sp++ - ax;}
        else if(op == MUL){ax = *sp++ * ax;}
        else if(op == DIV){ax = *sp++ / ax;}
        else if(op == MOD){ax = *sp++ % ax;}
        //内置函数
        else if(op == EXIT){printf("exit(%d)",*sp);return *sp;}
        else if(op == OPEN){ax = open((char *)sp[1],sp[0]);}
        else if(op == CLOS){ax = close(*sp);}
        else if(op == READ){ax = read(sp[2],(char *)sp[1],sp[0]);}
        else if(op == PRTF){tmp = sp + pc[1];ax = printf((char *)tmp[-1],tmp[-2],tmp[-3],tmp[-4],tmp[-5],tmp[-6]);}
        else if(op == MALC){ax = (int)malloc(*sp);}
        else if(op == MSET){ax = (int)memset((char *)sp[2],sp[1], *sp);}
        else if(op == MCMP){ax = memcmp((char *)sp[2],(char *)sp[1],*sp);}
        else {
            printf("unknown instruction:%d\n",op);
            return -1;
        }
    }
    return 0;
}

int main(int argc,char **argv){
    int i,fd;
    int *tmp;

    argc--;
    argv++;

    poolsize = 256*1024;     //设置内存池的大小为256KB
    line = 1;

    if((fd = open(*argv,0)) < 0){
        printf("could not open(%s)\n",*argv);
        return -1;
    }

    //为虚拟机分配内存
    if(!(text = old_text = malloc(poolsize))){
        printf("could not malloc(%d) for text area.\n",poolsize);
        return -1;
    }
    if(!(data = malloc(poolsize))){
        printf("could not malloc(%d) for data area.\n",poolsize);
        return -1;
    }
    if(!(stack = malloc(poolsize))){
        printf("could not malloc(%d) for stack area.\n",poolsize);
        return -1;
    }
    if(!(symbols = malloc(poolsize))){
        printf("could not malloc(%d) for symbol table\n",poolsize);
        return -1;
    }

    memset(text,0,poolsize);
    memset(data,0,poolsize);
    memset(stack,0,poolsize);
    memset(symbols,0,poolsize);

    bp = sp = (int *)((int)stack + poolsize);
    ax = 0;

    src = "char else enum if int return sizeof while open read printf malloc memset memcmp exit void main";

    //增加关键词
    i = Char;
    while(i<=While){
        next();
        current_id[Token] = i++;
    }

    //添加库
    i = OPEN;
    while( i <= EXIT){
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    next();current_id[Token] = Char;        //处理void型
    next();idmain = current_id;             //追踪主函数

    //读取源文件
    if((fd = open(*argv,0)) < 0){
        printf("could not open(%s)\n",*argv);
        return -1;
    }

    if(!(src = old_src = malloc(poolsize))){
        printf("could not malloc(%d) for source area\n",poolsize);
        return -1;
    }

    //读取源代码文件
    if((i = read(fd,src,poolsize-1)) <= 0){
        printf("read() returned %d\n",i);
        return -1;
    }
    src[i] = 0;     //添加文件结尾字符
    close(fd);
    
    program();

    if(!(pc = (int *)idmain[Value])){
        printf("main() not defined\n");
        return -1;
    }

    //栈初始化
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT;           //使用exit用来退出main
    *--sp = PUSH; tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

    return eval();
}
```
