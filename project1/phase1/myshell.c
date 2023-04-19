#include "myshell.h"

int main(){
    char *argv[MAXARGS];
    char input[MAXLINE]; // input string
    int argc;

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
        // Parsing: Translate the input string into command line arguments.
        argc = myshell_parseinput(input, argv);
        // Executing: Execute the command by forking a child process and return to parent process.
        myshell_execute(argc, argv);

    } while (1);

    return 0;
}

void init(){
    get_history_path();
    load_history();
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

void myshell_execute(int argc, char **argv){
    pid_t pid;

    if (argv[0] == NULL) return;   /* Ignore empty lines */

    //quit -> exit(0), & -> ignore, other -> run
    if (!builtin_command(argc, argv)) { 
        if((pid = Fork())==0){ // child process
            if (execvp(argv[0], argv) < 0) {	//ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        else{ // parent process
            int status;
            if(Waitpid(pid, &status, 0)<0){
                printf("%d: Waitpid error\n", status);
            }
            else return;
        }
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
    if (!strcmp(argv[0], "history")){
        command_history();
        return 1;
    }
    if (!strcmp(argv[0], "exit")){
        command_exit();
        return 1;
    }
    return 0;                     /* Not a builtin command */
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
    //fflush(fp);
    fclose(fp);
}

// add node to history linked list
void add_history(char *input){
    HisNode *new;

    if(hisNum!=0){
       if(!strcmp(hisTail->input, input)) {
        return; // ignore
        }
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
        if(input[i]=='"' || input[i]=='\'') input[i] = ' ';
        i++;
    }
    
}