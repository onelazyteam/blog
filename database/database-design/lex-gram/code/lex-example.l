/*
  compile and run it as following:
  1. lex lex-example.l
  2. cc lex.yy.c -o lex-example -lfl (if you are using lex, instead of flex, you may have to change '-lfl' to '-ll')
  3. ./lex-example (the step 2 will generate the file 'lex-example'. if you run it, it waits for you to type some input)
  4. terminate with (control + D)
*/

%{
#include <stdio.h>
%}

%%
[0123456789]+           printf("NUMBER\n");
[a-zA-Z][a-zA-Z0-9]*    printf("WORD\n");
%%
