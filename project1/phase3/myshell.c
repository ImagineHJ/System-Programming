#include "myshell.h"

int main(){
    char *argv[MAXARGS];
    char input[MAXLINE]; // input string
    int argc, bg;

    init();

    do{
        // Shell Prompt: print your prompt
        printf("CSE4100-MP-P1> ");
        // Reading: Read the command from standard input.
        myshell_readinput(input);
        // Translating: translate !! or !# to commands
        myshell_translate_input(input);
        // Add History : Save commands to history
        add_history(input);
        // Get Background : Find if the command should run in background
        bg = get_background(input);
        // Parsing: Translate the input string into command line arguments.
        argc = myshell_parseinput(input, argv);
        // Executing: Execute the command by forking a child process and return to parent process
        myshell_execute(input, argc, argv, bg);

    } while (1);

    return 0;
}

void init(){
    get_history_path();
    load_history();
    background_job_init();
}
void background_job_init(){
    Signal(SIGCHLD, sigchld_handler); // sigaction signal handler
    Signal(SIGTSTP, sigstp_handler); // sigaction signal handler
    bgNextId = 1; bgNum = 0;
    bgHead = bgTail = NULL;
    foreground_job = NULL;
    shell_pid = getpid();
    foreground_wait = 0;
}

void myshell_readinput(char *input){
    Fgets(input, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);
}

int myshell_translate_input(char *input){
    char temp[MAXLINE];
    int end;
    HisNode *ptr;

    int i=0;
    while(1){
        if(input[i]==' ' || input[i]=='\n' || input[i]==0) break;
        if(input[i]=='!') break;
        i++;
    }

    // if !!
    if (input[i]=='!' && input[i+1]=='!'){
        replace_command(input, hisTail, i, i+2);
        printf("%s", input);
    }
    
    // if !#
    else if (input[i]=='!' && input[i+1]>='0' && input[i+1]<='9'){
        end = i+1;
        while(input[end]>='0' && input[end]<='9') end++;
        int num = parse_number(input, i+1, end-1); // parse the number
        ptr=hisHead;
        if(hisNum<num) return 1 ; // fail
        for(int i=1; i<num; i++){
            ptr = ptr->next;
        }
        replace_command(input, ptr, i, end);
        printf("%s", input);
    }
    
    return 0; // success
}


int myshell_parseinput (char *input, char **argv){
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */

    remove_quotations(input);

    input[strlen(input)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*input && (*input == ' ')) /* Ignore leading spaces */
	    input++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(input, ' '))) {
        argv[argc++] = input;
        *delim = '\0';
        input = delim + 1;
        while (*input && (*input == ' ')) /* Ignore spaces */
                input++;
    }
    argv[argc] = NULL;    
    return argc;

}

void myshell_execute(char *input, int argc, char **argv, int bg){
    pid_t pid;
    sigset_t mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    if (argv[0] == NULL) return;   /* Ignore empty lines */
    if (!strcmp(argv[0], "exit")){
        command_exit();
    }

    if(!background_command(argc, argv)){
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);// block SIGCHLD
        if((pid=Fork())==0){ // child
            pipe_execute(argc, argv, NULL, 1);
            exit(0);
        }
        else{ // parent
            if(!bg){
                int status;
                foreground_wait = 1;
                setpgid(pid, pid);
                make_foreground_job(pid, argc, argv);
                // printf("front job pid %d, pgid %d\n",  pid, getpgid(pid));
                // printf("shell pid %d, pgid %d\n",  getpid(), getpgid(0));
                while(foreground_wait) Sigsuspend(&prev_one);
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);// unblock SIGCHLD
            }
            else{
                Sigprocmask(SIG_BLOCK, &mask_all, NULL);// block SIGCHLD
                setpgid(pid, pid);
                add_job(pid, argc, argv); // add job
                // printf("back job pid %d, pgid %d\n",  pid, getpgid(pid));
                // printf("shell pid %d, pgid %d\n",  getpid(), getpgid(0));
                Sigprocmask(SIG_SETMASK, &prev_one, NULL);// unblock SIGCHLD
            }
        }
    }
}


void pipe_execute(int argc, char **argv, int *my_fd, int first){
    pid_t pid;
    int pipe_i = -1, first_argc = argc, second_argc;
    int chd_fd[2];
    char *first_argv[MAXARGS];
    char **second_argv;

    if (argv[0] == NULL) return;   /* Ignore empty lines */

    for(int i=0; i<argc; i++){
        if(!strcmp(argv[i], "|")) {
            pipe_i = i;
            break;
        }
        first_argv[i] = argv[i];
        }

    if(pipe_i>0){
        first_argc = pipe_i;
        second_argc = argc-first_argc-1;
        second_argv = argv+pipe_i+1;
        if(pipe(chd_fd)<0){
            printf("Pipe Error\n");
            exit(0);
        }
    }
    first_argv[first_argc] = NULL;

    if((pid = Fork())==0){ // child process
        if(!first){
            dup2(my_fd[0], STDIN_FILENO); // read from par_fd[0]
            close(my_fd[1]); // close par_fd[1] (used for writing)
        }
        if(pipe_i!=-1){ // have pipe
            close(chd_fd[0]); // close my_fd[0] (used for reading)
            dup2(chd_fd[1], STDOUT_FILENO); // write to my_fd[1]
        }
        
        if(!builtin_command(first_argc, first_argv)){
            if (execvp(first_argv[0], first_argv) < 0) {
                printf("%s: Command not found.\n", first_argv[0]);
                exit(0);
            }
        }
        else exit(0);
    }
    else{ // parent process
        int status;
        if(!first) {close(my_fd[0]);close(my_fd[1]);}
        if(Waitpid(pid, &status, 0)<0){
            printf("%d: Waitpid error\n", status);
        }
        else {
            if(pipe_i!=-1) pipe_execute(second_argc, second_argv, chd_fd, 0);
        }
        return;
    }
}

// ---------------------------------------
// built in shell commands
int builtin_command(int argc, char **argv) 
{
    if (!strcmp(argv[0], "cd")) {
        command_cd(argc, argv);
        return 1;
    }
    else if (!strcmp(argv[0], "history")){
        command_history();
        return 1;
    }
    
    return 0; /* Not a builtin command */
}

int command_cd(int argc, char **argv){
    if(argc==1){
        // go to home directory
        if(!chdir(getenv("HOME"))) return 0;
    }
    if(argc==2){
        if(!chdir(argv[1])) return 0;
    }
    return 1; // fail
}

int command_exit(){
    save_history(); // save history commands
    exit(0);
}


// ---------------------------------------
// background functions
int background_command(int argc, char **argv)
{
    if (!strcmp(argv[0], "jobs")){
        command_jobs();
        return 1;
    }
    else if (!strcmp(argv[0], "bg")){
        command_bg(argc, argv);
        return 1;
    }
    else if (!strcmp(argv[0], "fg")){
        command_fg(argc, argv);
        return 1;
    }
    else if (!strcmp(argv[0], "kill")){
        command_kill(argc, argv);
        return 1;
    }
    
    return 0; /* Not a builtin command */
}

int get_background(char * input){
    int end = strlen(input)-1;
    
    while(end>=0){
        if(input[end]=='&') {
            input[end] = ' ';
            return 1;
        }
        if(input[end]!=' ' &&  input[end]!='\n') break;
        end--;
    }
    return 0;
}

void sigchld_handler(int sig){
    pid_t pid;
    sigset_t mask_all, prev_all;
    
    Sigfillset(&mask_all);
    while((pid=waitpid(-1, NULL, WNOHANG))>0){ // reap child
        if(foreground_job && pid==foreground_job->pid) {
            foreground_wait = 0; 
            foreground_job = NULL; 
            continue;
        }
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);// block SIGCHLD
        delete_job(pid);
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);// unblock
    }
}

void sigstp_handler(int sig){

    if(!foreground_wait) return;

    foreground_wait = 0;

    // stop foreground job
    kill(-(foreground_job->pgid), SIGSTOP);

    // to background
    foreground_job->running = 0;
    if(bgNum==0){
        bgHead = foreground_job;
        bgTail = foreground_job;
    }
    else{
        bgTail->next = foreground_job;
        bgTail = foreground_job;
    }
    bgNum++;
    foreground_job = NULL; 
}

void make_foreground_job(pid_t pid, int argc, char **argv){
    BgNode *new;
    
    new = (BgNode *)Malloc(sizeof(BgNode));
    copy_commands(argc, argv, new->name);
    new->pid = pid;
    new->pgid = getpgid(pid); 
    new->running = 1;
    new->next = NULL;
    new->id = bgNextId; bgNextId++;

    foreground_job = new;
}

int command_jobs(){
    print_jobs();
    return 0;
}

int command_bg(int argc, char **argv){
    BgNode *job;
    int id = parse_id(argv[1]);

    if(get_bgNode_by_id(id, &job)!=0){
        return 1;
    }

    // print
    job->running = 1;
    if(job->running) printf("[%d] running %s\n", job->id, job->name);
    else printf("[%d] suspended %s\n", job->id, job->name);
    
    // awake
    kill(-(job->pgid), SIGCONT);
}

int command_fg(int argc, char **argv){
    BgNode *job;
    int id = parse_id(argv[1]);
    sigset_t mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);

    if(get_bgNode_by_id(id, &job)!=0){
        return 1;
    }

    pop_job(job->pid);
    // change state
    job->running = 1;
    
    // print
    if(job->running) printf("[%d] running %s\n", job->id, job->name);
    else printf("[%d] suspended %s\n", job->id, job->name);

    foreground_job = job;
    foreground_wait = 1;
    
    Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);// block SIGCHLD
    // awake
    kill(-(job->pgid), SIGCONT);

    // wait
    while(foreground_wait) Sigsuspend(&prev_one);
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);// unblock SIGCHLD
    return 0;
}

int command_kill(int argc, char **argv){
    BgNode *job;
    int id = parse_id(argv[1]);

    if(get_bgNode_by_id(id, &job)!=0){
        return 1;
    }
    kill(-(job->pgid), SIGKILL);
}

void add_job(pid_t pid, int argc, char **argv){
    BgNode *new;

    new = (BgNode *)Malloc(sizeof(BgNode));
    copy_commands(argc, argv, new->name);
    new->pid = pid;
    new->pgid = getpgid(pid); 
    new->running = 1;
    new->id = bgNextId; bgNextId++;
    new->next = NULL;
    if(bgNum==0){
        bgHead = new;
        bgTail = new;

    }
    else{
        bgTail->next = new;
        bgTail = new;
    }
    bgNum++;
}

void delete_job(pid_t pid){
    BgNode *ptr = bgHead, *prev;

    while(ptr){
        if(ptr->pid==pid) break;
        prev = ptr;
        ptr=ptr->next;
    }

    if(ptr==NULL) return;
    
    if(ptr==bgHead && ptr==bgTail){
        bgHead = bgTail = NULL;
    }
    else if(ptr==bgHead){
        bgHead = ptr->next;
    }
    else if(ptr==bgTail){
        bgTail = prev;
        prev->next = NULL;
    }
    else{
        prev -> next = ptr->next;
    }
    free(ptr);
    bgNum--;
}

void pop_job(pid_t pid){
    BgNode *ptr = bgHead, *prev;

    while(ptr){
        if(ptr->pid==pid) break;
        prev = ptr;
        ptr=ptr->next;
    }

    if(ptr==NULL) return;
    
    if(ptr==bgHead && ptr==bgTail){
        bgHead = bgTail = NULL;
    }
    else if(ptr==bgHead){
        bgHead = ptr->next;
    }
    else if(ptr==bgTail){
        bgTail = prev;
        prev->next = NULL;
    }
    else{
        prev -> next = ptr->next;
    }
    bgNum--;

    ptr->next = NULL;
}


int get_bgNode_by_id(int id, BgNode **job){
    BgNode *ptr = bgHead;
    
    while(ptr){
        if(ptr->id==id) break;
        ptr=ptr->next;
    }
    if(!ptr) {
        printf("No Such Job\n");
        return 1;
    }
    *job = ptr;
    return 0;
}

int get_pid(int id, pid_t *pid){
    BgNode *ptr = bgHead;
    
    while(ptr){
        if(ptr->id==id) break;
        ptr=ptr->next;
    }
    if(!ptr) {
        printf("No Such Job\n");
        return 1;
    }
    *pid = ptr->pid;
    return 0;
}

int get_pgid(int id, pid_t *pid){
    BgNode *ptr = bgHead;
    
    while(ptr){
        if(ptr->id==id) break;
        ptr=ptr->next;
    }
    if(!ptr) {
        printf("No Such Job\n");
        return 1;
    }
    *pid = ptr->pgid;
    return 0;
}

void print_jobs(){
    BgNode *ptr = bgHead;

    while(ptr){
        if(ptr->running) printf("[%d] running %s\n", ptr->id, ptr->name);
        else printf("[%d] suspended %s\n", ptr->id, ptr->name);
        ptr=ptr->next;
    }
}

// ---------------------------------------
// history commands

int command_history(){
    print_history();
    return 0;
}

void get_history_path(){
    getcwd(historyPath, MAXPATH);
    strcat(historyPath, "/history.txt");
}

// read history.txt and construct a linked list
void load_history(){
    HisNode *new;
    char buffer[MAXLINE];

    hisNum = 0;
    FILE *fp=fopen(historyPath, "r");

    if(fp==NULL){
        hisHead = hisTail = NULL;
        return;
    }
    
    while(fgets(buffer, MAXLINE, fp)!=NULL){
        if(buffer[0]==' ' || buffer[0]=='\n' || buffer[0]==0) continue;

        new = (HisNode *)Malloc(sizeof(HisNode));
        strcpy(new->input, buffer);
        new->next = NULL;

        if(hisNum==0){
            hisHead = new;
        }
        else{
            hisTail->next = new;
        }

        hisTail = new;
        hisNum++;
    }
    if(hisNum==0){ // empty file
        hisHead = hisTail = NULL;
    }

    fclose(fp);
}

// save linked list to history.txt
void save_history(){
    // specify directory 

    FILE *fp=fopen(historyPath, "w");
    HisNode *ptr = hisHead, *next;

    while(ptr){
        next = ptr->next;
        fprintf(fp, "%s", ptr->input);
        free(ptr);
        ptr = next;
    }
    Fclose(fp);
}

// add node to history linked list
void add_history(char *input){
    HisNode *new;

    if(hisNum!=0){
        
       if(!strcmp(hisTail->input, input)) return; // ignore
    }

    // 중복예외처리
    new = (HisNode *)Malloc(sizeof(HisNode));
    strcpy(new->input, input);

    new->next = NULL;

    if(hisNum==0){
        hisHead = hisTail =new;
    }
    else{
        hisTail->next = new;
        hisTail = new;
    }
    
    hisNum++;
}

// print all nodes in history linked list
void print_history(){
    HisNode *ptr = hisHead;
    int num=1;

    while(ptr){
        // print num
        printf("%d\t%s", num, ptr->input);
        ptr = ptr->next;
        num++;
    }
}

// ---------------------------------------
// utils
int parse_number(char* c, int s, int e){
    int n = 0, digit = 1;

    for(int i=e; i>=s; i--){
        n += (c[i]-'0')*digit;
        digit *= 10;
    }
    return n;
}

void replace_command(char *input, HisNode *ptr, int s, int e){
    char temp[MAXLINE];

    strcpy(temp, input+e);
    strcpy(input+s, ptr->input);
    remove_new_line(input);
    strcat(input, temp);
}

void remove_new_line(char *str){
    int i=0;
    while(str[i]!='\n') i++;
    str[i] = 0;
}

void remove_quotations(char *input){
    int i=0;

    while(input[i]!=0){
        if(input[i]=='"'|| input[i]=='\'') input[i] = ' ';
        i++;
    }
    
}

int parse_id(char* c){
    int end=1, n = 0, digit = 1;
    
    if(c[0]!='%') return -1;

    while(c[end]>='0' && c[end]<='9') end++;

    for(int i=end-1; i>=1; i--){
        n += (c[i]-'0')*digit;
        digit *= 10;
    }
    return n;
}

void copy_commands(int argc, char **argv, char *dest){
    
    for(int i=0; i<argc; i++) {
        strcat(dest, argv[i]);
        strcat(dest, " ");
    }
    dest[strlen(dest)-1]=0;    
}