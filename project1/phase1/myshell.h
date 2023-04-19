#include "csapp.h"
#include<errno.h>

#define MAXARGS 128
#define MAXPATH 8192

typedef struct _HisNode{
    char input[MAXLINE];
    struct _HisNode *next;
} HisNode;

HisNode *hisHead, *hisTail;
int hisNum;
char historyPath[MAXPATH];


// shell functions
void init();
void myshell_readinput(char *);
int myshell_parseinput (char *, char **);
void myshell_execute(int, char **);
int myshell_translate_input(char *);

// built in commands
int builtin_command(int, char **);
int command_cd(int, char **);
int command_exit();
int command_history();

// history functions
void get_history_path();
void load_history();
void save_history();
void add_history(char *);
void print_history();

int parse_number(char*, int, int);
void replace_command(char *, HisNode *, int, int);
void remove_new_line(char *);
void remove_quotations(char *);