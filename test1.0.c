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