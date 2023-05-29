#include "csapp.h"

// client pool
typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

void init_pool(int , pool *);
void add_client(int , pool *);
void check_clients(pool *);
void close_client(pool *, int, int);
int connection_cnt;

// stock binary tree
typedef struct treeNode {
    int id;
    int left_stock;
    int price;
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

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    socklen_t clientlen = sizeof( struct sockaddr_storage);
    static pool pool;
    //char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    
    init_pool(listenfd, &pool);
    Signal(SIGINT, sigint_handler);
    make_tree();
    
    while (1) {
        /* Wait for listening/connected descriptor(s) to become ready */
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            
            //Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            //printf("Connected to (%s, %s), connfd = %d\n", client_hostname, client_port, connfd);
            
            add_client(connfd, &pool);
        }

        /* Echo a text line from each ready connected descriptor */
        check_clients(&pool);
    }

    save_tree();
    return 0;
}

void init_pool(int listenfd, pool *p) {
    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;
    for (i=0; i< FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);

    connection_cnt = 0;
}

void add_client(int connfd, pool *p) {
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
        if (p->clientfd[i] < 0) {
            /* Add connected descriptor to the pool */
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            
            /* Add the descriptor to descriptor set */
            FD_SET(connfd, &p->read_set);
            
            /* Update max descriptor and pool highwater mark */
            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;

            connection_cnt++;
            break;
        }

    if (i == FD_SETSIZE) /* Couldn’t find an empty slot */
        app_error("add_client error: Too many clients");
}

void check_clients(pool *p){
    
    int i, connfd, n, end;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                end = execute_command(connfd, buf, n);
                if(end) close_client(p, connfd, i);
            }
            /* EOF detected, remove descriptor from pool */
            else {
                close_client(p, connfd, i);
            }
        }
    }
}

void close_client(pool *p, int connfd, int i){
    //printf("connection closed for connfd = %d\n", connfd);
    Close(connfd);
    FD_CLR(connfd, &p->read_set);
    p->clientfd[i] = -1;
    connection_cnt--;
    if(connection_cnt==0) save_tree(); // save tree when there is no connection
}

int execute_command(int connfd, char *buf, int n){
    char *argv[5];
    int id, cnt;

    //printf("execute command from %d : %s\n", connfd, buf);
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

    if(p->left_stock >=cnt){
        p->left_stock -= cnt;
        ret = sucess;
    }
    else{
        ret = fail;
    }
    Rio_writen(connfd, ret, MAXLINE);

}

void sell(int connfd, int id, int cnt){
    treeNode *p;
    char sucess[30] = "[sell] sucess\n";

    p = find_node(root, id);
    
    p->left_stock += cnt;

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

    sprintf(temp, "%d %d %d\n", root->id, root->left_stock, root->price);
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