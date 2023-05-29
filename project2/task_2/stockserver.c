#include "csapp.h"
#define NTHREADS 100
#define SBUFSIZE 100

// stock binary tree
typedef struct treeNode {
    int id;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t w_mutex;
    struct treeNode *left;
    struct treeNode *right;
} treeNode;

treeNode *root;

void make_tree();
void save_tree();
void parse_stock_input(char *, int *, int *, int *);
void write_tree(FILE *, treeNode *);
treeNode* find_node(treeNode* , int );
void print_tree(int , treeNode*, char *);
void add_node(treeNode* );

// commands
int execute_command();
int parse_input(char *, char **);
void show(int);
void buy(int, int, int);
void sell(int, int, int);

void sigint_handler(int);

// buffer
typedef struct {
    int *buf;   
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;

void sbuf_init(sbuf_t *, int);
void sbuf_deinit(sbuf_t *);
void sbuf_insert(sbuf_t *, int);
int sbuf_remove(sbuf_t *);

// multi thread
void *thread(void *vargp);
void client_service(int);
void thread_init();
sem_t connection_mutex;
int connection_cnt;

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    socklen_t clientlen = sizeof( struct sockaddr_storage);
    //char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    
    Signal(SIGINT, sigint_handler);
    make_tree();
    sbuf_init(&sbuf, SBUFSIZE);
    thread_init();
    
    pthread_t tid;
    for(int i=0; i<NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL);
    
    while (1) {
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        //Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        //printf("Connected to (%s, %s), connfd = %d\n", client_hostname, client_port, connfd);
        sbuf_insert(&sbuf, connfd);
    }

    sbuf_deinit(&sbuf);
    save_tree();
    return 0;
}

void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear)%(sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}
int sbuf_remove(sbuf_t *sp){
    int item;

    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front)%(sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);

        P(&connection_mutex);
        connection_cnt++;
        V(&connection_mutex);

        client_service(connfd);
        
        P(&connection_mutex);
        connection_cnt--;
        if(connection_cnt==0) save_tree();
        V(&connection_mutex);

        Close(connfd);
        //printf("end connection fd : %d\n", connfd);
    }
}

void client_service(int connfd){
    rio_t rio;
    int end, n;
    char buf[MAXLINE];

    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        end = execute_command(connfd, buf, n);
        if(end) break;
    }
}

void thread_init(){
    Sem_init(&connection_mutex, 0, 1);
    connection_cnt = 0;
}

int execute_command(int connfd, char *buf, int n){
    char *argv[5];
    int id, cnt;

    //printf("\nexecute command from %d : %s", connfd, buf);
    parse_input(buf, argv);

    if (!strcmp(argv[0], "show")) {
        show(connfd);
        return 0;
    }
    if (!strcmp(argv[0], "buy")){
        id = atoi(argv[1]);
        cnt = atoi(argv[2]);
        buy(connfd, id, cnt);
        return 0;
    }
    if (!strcmp(argv[0], "sell")){
        id = atoi(argv[1]);
        cnt = atoi(argv[2]);
        sell(connfd, id, cnt);
        return 0;
    }
    if (!strcmp(argv[0], "exit")){
        Rio_writen(connfd, "", MAXLINE);
        return 1;
    }
    return 0;
}

int parse_input(char *input, char **argv){
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */

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

void show(int connfd){
    // traverse and print
    char ret[MAXLINE];
    ret[0] = '\0';
    print_tree(connfd, root, ret);
    Rio_writen(connfd, ret, MAXLINE);
}

void buy(int connfd, int id, int cnt){
    treeNode *p;
    char *ret;
    char sucess[30] = "[buy] sucess\n";
    char fail[30] = "Not enough left stock\n";

    p = find_node(root, id);

    P(&(p->w_mutex));

    if(p->left_stock >=cnt){
        p->left_stock -= cnt;
        ret = sucess;
    }
    else{
        ret = fail;
    }

    V(&(p->w_mutex));
    Rio_writen(connfd, ret, MAXLINE);
}

void sell(int connfd, int id, int cnt){
    treeNode *p;
    char sucess[30] = "[sell] sucess\n";

    p = find_node(root, id);
    
    P(&(p->w_mutex));
    p->left_stock += cnt;
    V(&(p->w_mutex));
    Rio_writen(connfd, sucess, MAXLINE);
}

void make_tree(){
    treeNode *new;
    char buffer[MAXLINE];

    FILE *fp=fopen("stock.txt", "r");
    root = NULL;

    if(fp==NULL){
        return;
    }

    while(fgets(buffer, MAXLINE, fp)!=NULL){
        if(buffer[0]==' ' || buffer[0]=='\n' || buffer[0]==0) continue;

        //printf("%s", buffer);

        new = (treeNode *)Malloc(sizeof(treeNode));
        parse_stock_input(buffer, &(new->id), &(new->left_stock), &(new->price));
        new->readcnt = 0;
        Sem_init(&(new->mutex), 0, 1);
        Sem_init(&(new->w_mutex), 0, 1);
        new->left = NULL;
        new->right = NULL;
        add_node(new);
    }
    fclose(fp);
}

void parse_stock_input(char *buf, int *id, int *left_stock, int *price){
    char *delim;
    char *argv[5];
    int argc = 0;
    
    if(buf[strlen(buf)-1]=='\n') buf[strlen(buf)-1] = ' ';
    else buf[strlen(buf)] = ' ';

    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
                buf++;
        
    }
    argv[argc] = NULL;    
    
    *id = atoi(argv[0]);
    *left_stock = atoi(argv[1]);
    *price = atoi(argv[2]);
    
    return;
}

void save_tree(){

    FILE *fp=fopen("stock.txt", "w");

    write_tree(fp, root);

    fclose(fp);
}

void write_tree(FILE *fp, treeNode *root){
    if(root==NULL) return;

    fprintf(fp, "%d %d %d\n", root->id, root->left_stock, root->price);
    write_tree(fp, root->left);
    write_tree(fp, root->right);
}

treeNode* find_node(treeNode* root, int id){
    
    if(root->id==id) return root;

    if(root->id>id) return find_node(root->left, id);
    else return find_node(root->right, id);
}

void print_tree(int connfd, treeNode* root, char *ret){
    if(root==NULL) return;

    char temp[100];

    // reading mutex
    P(&(root->mutex));
    root->readcnt++;
    if(root->readcnt==1){
        P(&(root->w_mutex)); // do not allow writing
    }
    V(&(root->mutex));

    sprintf(temp, "%d %d %d\n", root->id, root->left_stock, root->price);
    
    // reading mutex
    P(&(root->mutex));
    root->readcnt--;
    if(root->readcnt==0){
        V(&(root->w_mutex)); // allow writing
    }
    V(&(root->mutex));
    
    strcat(ret, temp);
    //Rio_writen(connfd, temp, strlen(temp));

    print_tree(connfd, root->left, ret);
    print_tree(connfd, root->right, ret);
}

void add_node(treeNode* new){
    treeNode *ptr = root;

    if(root==NULL) {root = new; return;}
    
    while(1){
        if(ptr->id>new->id){
            if(ptr->left) ptr = ptr->left;
            else{
                ptr->left = new;
                break;
            }
        }
        else{
            if(ptr->right) ptr = ptr->right;
            else {
                ptr->right = new;
                break;
            }
        }
    }

}

void sigint_handler(int sig){
    save_tree();
    exit(0);
}