#include "csapp.h"
#include<errno.h>

#define MAXARGS 128
#define MAXPATH 8192

// history variables
typedef struct _HisNode{
    char input[MAXLINE];
    struct _HisNode *next;
} HisNode;

HisNode *hisHead, *hisTail;
int hisNum;
char historyPath[MAXPATH];

// background job variables
typedef struct _BgNode{
    char name[MAXLINE];
    pid_t pid;
    pid_t pgid;
    int id;
    int running;
    int should_wait;
    struct _BgNode *next;
} BgNode;

BgNode *bgHead, *bgTail;
BgNode *foreground_job;
int bgNextId, bgNum;
pid_t shell_pid;

int foreground_wait;

// shell functions
void init();
void background_job_init();
void myshell_readinput(char *);
int myshell_parseinput (char *, char **);
void myshell_execute(char *, int , char **, int );
void pipe_execute(int , char **, int *, int );
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

// background jobs functions
int background_command(int , char **) ;
int get_background(char * );
void sigchld_handler(int );
void sigstp_handler(int );
int command_jobs();
int command_bg(int , char **);
int command_fg(int , char **);
int command_kill(int , char **);
void make_foreground_job(pid_t , int, char **);
void add_job(pid_t , int, char **);
void delete_job(pid_t );
void pop_job(pid_t );
int get_bgNode_by_id(int , BgNode **);
void print_jobs();


// utils functions
int parse_number(char*, int, int);
void replace_command(char *, HisNode *, int, int);
void remove_new_line(char *);
void remove_quotations(char *);
int parse_id(char* c);
void copy_commands(int , char **, char *);