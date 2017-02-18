#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*linked list*/

struct node {
    char* key;
    struct node *next;
    int position;
};

struct node *head = NULL;
struct node *current = NULL;

pthread_cond_t * okevict;



void insertLast(char * key, int pos) {
    struct node *current = head;
    if (head == NULL) {
        current = malloc(sizeof (struct node));
        current->key = strdup(key);
        current->position = pos;
        current->next = NULL;
        head = current;
        return;
    }

    while (current->next != NULL) {
        current = current->next;
    }

    /* now we can add a new variable */
    current->next = malloc(sizeof (struct node));
    current->next->key = strdup(key);
    current->next->position = pos;
    current->next->next = NULL;
}




struct node* delete(char* key) {

    //start from the first link
    struct node* current = head;
    struct node* previous = NULL;

    //if list is empty
    if (head == NULL) {
        return NULL;
    }

    //navigate through list
    while (strcmp(current->key, key) != 0) {

        //if it is last node
        if (current->next == NULL) {
            return NULL;
        } else {
            //store reference to current link
            previous = current;
            //move to next link
            current = current->next;
        }
    }

    //found a match, update the link
    if (current == head) {
        //change first to point to next link
        head = head->next;
    } else {
        //bypass the current link
        previous->next = current->next;
    }

    return current;
}
int cc = 0;

void put2Front(char* key) {
    if (strcmp(head->key, key) == 0) {
        return;
    }
    struct node* toput = delete(key);
    toput->next = head;
    head = toput;
    cc++;
    // printf("\n%i\n", cc);
    return;
}

void put2Tail(char* key) {
    if (strcmp(head->key, key) == 0) {
        return;
    }
    struct node* toput = delete(key);
    struct node* current = head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = toput;
    toput->next = NULL;
}



//find a link with given key

struct node* find_pos(int pos) {

    //start from the first link
    struct node* current = head;

    int count = 1;

    //navigate through list
    while (count != pos) {
        current = current->next;
        count++;
    }

    //if data found, return the current Link
    return current;
}
//find a link with given key

struct node* find_b4_last() {

    //start from the first link
    struct node* current = head;
    struct node* previous = current;

    //navigate through list
    while (current->next != NULL) {
        //go to next link
        previous = current;
        current = current->next;

    }

    //if data found, return the current Link
    return previous;
}

//find a link with given key

struct node* find_last() {

    //start from the first link
    struct node* current = head;


    //navigate through list
    while (current->next != NULL) {
        //go to next link
        current = current->next;

    }

    //if data found, return the current Link
    return current;
}


struct htb* htb;
pthread_mutex_t* hash_lock;

/*HASH TABLE*/
struct htb {
    /* you can define this struct to have whatever fields you want. */
    struct c_file *h_table[20000000]; //SIZE WILL CHANGE LATER
    unsigned long size;
    unsigned long current_size;
};

struct c_file {
    struct file_data *_data;
    pthread_cond_t* processing;
    int done;
    int ref;
};

unsigned long
hash_str(char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

//returns the index number >= 0 if found, -1 if not found

int h_lookup(char* filename, struct htb* htb) {
    unsigned long index = hash_str(filename) % 20000000;

    while (1) {
        if (htb->h_table[index] != NULL) {
            if (strcmp(htb->h_table[index]->_data->file_name, filename) == 0) {
                return index; //found
            } else {
                index = index + 1;
                index = index % 20000000;
            }
        } else if (htb->h_table[index] == NULL) {
            return -1; //did not found
        }
    }
}

//returns 1 if insert successfully, 0 if already exists

int h_insert(struct c_file* file2insert, struct htb * htb) {

    unsigned long index = hash_str(file2insert->_data->file_name) % 20000000;

    while (1) {
        if (htb->h_table[index] != NULL) {
            if (strcmp(htb->h_table[index]->_data->file_name, file2insert->_data->file_name) == 0) {
                return 0; //already exists
            } else {
                index = index + 1;
                index = index % 20000000;
            }
        } else if (htb->h_table[index] == NULL) {
            struct c_file* temp = malloc(sizeof (struct c_file));
            temp->_data = malloc(sizeof (struct file_data));
            temp->_data->file_buf = strdup(file2insert->_data->file_buf);
            temp->_data->file_name = strdup(file2insert->_data->file_name);
            temp->_data->file_size = file2insert->_data->file_size;
            htb->current_size = htb->current_size + file2insert->_data->file_size;
            htb->h_table[index] = temp;
            return index; //find a spot, inserted
        }
    }
}

struct htb *
h_init(unsigned long size) {
    struct htb *htb;
    okevict = malloc(sizeof (pthread_cond_t));
    pthread_cond_init(okevict, NULL);
    htb = (struct htb *) malloc(sizeof (struct htb));

    hash_lock = malloc(sizeof (pthread_mutex_t));
    htb->size = size;
    htb->current_size = 0;

    return htb;
}



/*END OF HASH TABLE*/



//nov 6: 1. current working thread count 2.blocking after finishing a request(?)

struct server {
    int nr_threads;
    int max_requests;
    int max_cache_size;
    /* add any other parameters you need */
};

//circular buffer
int *ptr_tobuffer;
int bf_in;
int bf_out;
int buffer_size;

int bf_put(int new) {
    if (bf_in == ((bf_out - 1 + buffer_size) % buffer_size)) {
        return -1; /* Queue Full*/
    }

    ptr_tobuffer[bf_in] = new;

    bf_in = (bf_in + 1) % buffer_size;

    return 0; // No errors
}

int bf_get(int *old) {
    if (bf_in == bf_out) {
        return -1; /* Queue Empty - nothing to get*/
    }

    *old = ptr_tobuffer[bf_out];

    bf_out = (bf_out + 1) % buffer_size;

    return 0; // No errors
}



//mutex lock on the circular buffer and its current position
pthread_mutex_t *bf_lock;

//conditional variable for empty circular buffer
pthread_cond_t *bf_empty;

//conditional varriable for full circular buffer
pthread_cond_t *bf_full;

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void) {
    struct file_data *data;

    data = Malloc(sizeof (struct file_data));
    data->file_name = NULL;
    data->file_buf = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data) {
    free(data->file_name);
    free(data->file_buf);
    free(data);
}

int evict(struct htb * hta) {

    struct node *current = head;

    while (hta->h_table[current->position]->ref > 0) {
        current = current->next;
        if (current == NULL) {
            return 0;
        }
    }

    delete(current->key);
    hta->current_size = hta->current_size - hta->h_table[current->position]->_data->file_size;
    file_data_free(hta->h_table[current->position]->_data);
    hta->h_table[current->position] = NULL;
    return 1;

}


static void
do_server_request(struct server *sv, int connfd) {
    int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

    /* fills data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
        file_data_free(data);
        return;
    }
    /* reads file, 
     * fills data->file_buf with the file contents,
     * data->file_size with file size. */
    if (data->file_size >= htb->size) {
dontc:

        ret = request_readfile(rq);
        if (!ret) {
            goto out;
        }
        request_sendfile(rq);
        goto out;
    } else {
        pthread_mutex_lock(hash_lock);
        int k = h_lookup(data->file_name, htb);
        if (k == -1) {//mei zhao dao
            //printf("\nMISS!\n");

            if (htb->current_size + data->file_size >= htb->size) {

                int y = evict(htb);
                if (y == 0) {
                    pthread_mutex_unlock(hash_lock);
                    goto dontc;
                }

            }
            pthread_mutex_unlock(hash_lock);
            ret = request_readfile(rq);
            if (!ret) {
                goto out;
            }
            pthread_mutex_lock(hash_lock);
            if (h_lookup(data->file_name, htb) == -1) {
                struct c_file* file2insert = malloc(sizeof (struct c_file));
                file2insert->_data = data;
                file2insert->ref = 0;
                int pos = h_insert(file2insert, htb);
                insertLast(file2insert->_data->file_name, pos);
                free(file2insert);
            }
            put2Tail(data->file_name);
        } else {
            //printf("\nsetting data: %i\n", k);
            request_set_data(rq, htb->h_table[k]->_data);
            put2Tail(data->file_name);
        }
    }
    /* sends file to client */
    //printf("\nsending\n");
    int u = h_lookup(data->file_name, htb);
    htb->h_table[u]->ref++;
    pthread_mutex_unlock(hash_lock);
    request_sendfile(rq);
    pthread_mutex_lock(hash_lock);
    htb->h_table[u]->ref--;

    //pthread_cond_signal(okevict);
    pthread_mutex_unlock(hash_lock);

out:
    request_destroy(rq);
    file_data_free(data);
}

/* entry point functions */
void *thread_do(void* vp) {
    struct server *sv = (struct server *) vp;
    //int firsttime = 0;
    while (1) {

        int *getnum;
        int num;
        getnum = &num;
        //if (firsttime == 0) {
        pthread_mutex_lock(bf_lock);
        //}
        //no requests for now, go to sleep
        while (bf_get(getnum) == -1) {
            pthread_cond_wait(bf_empty, bf_lock);
        }
        pthread_cond_signal(bf_full); // i just finished a request the buffer cant be full
        pthread_mutex_unlock(bf_lock);

        do_server_request(sv, num);


    }

}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size) {
    htb = h_init(max_cache_size);



    struct server *sv;

    sv = Malloc(sizeof (struct server));
    sv->nr_threads = nr_threads;
    sv->max_requests = max_requests;
    sv->max_cache_size = max_cache_size;

    /*if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
            TBD();
    }*/

    /* Lab 4: create queue of max_request size when max_requests > 0 */
    if (max_requests > 0) {
        ptr_tobuffer = malloc((max_requests + 1) * sizeof (int));
        bf_in = 0;
        bf_out = 0;
        buffer_size = max_requests + 1;
    }
    bf_empty = malloc(sizeof (pthread_cond_t));
    bf_full = malloc(sizeof (pthread_cond_t));
    bf_lock = malloc(sizeof (pthread_mutex_t));
    pthread_cond_init(bf_empty, NULL);
    pthread_cond_init(bf_full, NULL);
    pthread_mutex_init(bf_lock, NULL);

    /* Lab 5: init server cache and limit its size to max_cache_size */




    /* Lab 4: create worker threads when nr_threads > 0 */
    pthread_t thread_listp[nr_threads];
    int k = 0;
    if (nr_threads > 0) {
        for (k = 0; k < nr_threads; k++) {
            pthread_create(&thread_listp[k], NULL, thread_do, (void*) sv);
        }
    }
    return sv;
}

void
server_request(struct server *sv, int connfd) {
    if (sv->nr_threads == 0) { /* no worker threads */
        do_server_request(sv, connfd);
    } else {
        /*  Save the relevant info in a buffer and have one of the
         *  worker threads do the work. */
        pthread_mutex_lock(bf_lock);
        //buffer is full, block until buffer is not full and add connfd to the buffer 
        while (bf_put(connfd) == -1) {
            pthread_cond_wait(bf_full, bf_lock); //BLOCK REASON: FULL
        }
        //call up one of the threads
        pthread_cond_signal(bf_empty); //WAKE UP REASON: EMPTY
        pthread_mutex_unlock(bf_lock);



    }
}
