#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

#define RDY 0
#define RUNN 1
#define BLK 2
#define EXT 3

struct thread* current_thread;
Tid id_arr[1024];

struct rdyq {
    struct thread* head_t;
    int q_size;
};

struct rdyq rdy_que_inst = {NULL, 0};
struct rdyq ext_que_inst = {NULL, 0};

struct rdyq* rdy_que = &rdy_que_inst;
struct rdyq* ext_que = &ext_que_inst;

Tid yield_ret;

/* This is the thread control block */
struct thread {
    
    Tid id;
    int State;
    ucontext_t t_context;
    void * stack_ptr;
    int ismain;

    struct thread* pre_t;
    struct thread* next_t;
};

//RDY QUE IMPLEMENTATION START

struct thread * q_head_deq(struct rdyq* qp) {
    int enabled = interrupts_set(0);

    if (qp->head_t == NULL) {
        interrupts_set(enabled);
        return NULL;
    }


    if (qp->head_t->next_t == NULL) {
        struct thread* bf = qp->head_t;
        qp->head_t = NULL;
        qp->q_size--;
        interrupts_set(enabled);
        return bf;
    }


    struct thread* bf = qp->head_t;
    qp->head_t = qp->head_t->next_t;
    qp->head_t->pre_t = NULL;
    qp->q_size--;
    bf->next_t = NULL;
    interrupts_set(enabled);
    return bf;
}

struct thread * q_select(struct rdyq* qp, Tid trdid) {
    int enabled = interrupts_set(0);

    struct thread* temp = qp->head_t;
    if (temp == NULL) {
        interrupts_set(enabled);
        return temp;
    }
    //only one thread
    if (temp->next_t == NULL) {
        qp->head_t = NULL;
        qp->q_size--;
        interrupts_set(enabled);
        return temp;
    }
    //select head
    if (temp->id == trdid) {
        qp->head_t = temp->next_t;
        qp->head_t->pre_t = NULL;
        temp->next_t = NULL;
        qp->q_size--;
        interrupts_set(enabled);
        return temp;
    }

    //normal select
    while (temp != NULL) {
        //found
        if (temp->id == trdid) {
            //if its tail
            if (temp->next_t == NULL) {
                temp->pre_t->next_t = NULL;
                temp->pre_t = NULL;
                qp->q_size--;
                interrupts_set(enabled);
                return temp;
            }


            temp->pre_t->next_t = temp->next_t;
            temp->next_t->pre_t = temp->pre_t;
            temp->next_t = NULL;
            temp->pre_t = NULL;
            qp->q_size--;
            interrupts_set(enabled);
            return temp;
        }

        temp = temp->next_t;
    }
    interrupts_set(enabled);
    return NULL;
}

int q_find(struct rdyq * qp, Tid trdid) {
    int enabled = interrupts_set(0);

    struct thread* temp = qp->head_t;

    while (temp != NULL) {
        if ((int) temp->id == (int) trdid) {
            interrupts_set(enabled);
            return 1;
        }

        temp = temp->next_t;
    }
    interrupts_set(enabled);
    return 0;
}

void q_clear(struct rdyq * qp) {
    int enabled = interrupts_set(0);

    struct thread * temp = qp->head_t;
    while (temp != NULL) {
        if (temp->ismain == 1) {
            temp = temp->next_t;
            continue;
        }

        struct thread * buff = temp;
        temp = temp->next_t;
        id_arr[buff->id] = 0;
        free(buff->stack_ptr);
        free(buff);

    }
    qp->head_t = NULL;
    interrupts_set(enabled);
}

//before adding should set up tid, state & context first then pass into this function.

void q_tail_add(struct rdyq * qp, struct thread* toinsert) {
    int enabled = interrupts_set(0);
    if (qp->head_t == NULL) {
        toinsert->next_t = NULL;
        toinsert->pre_t = NULL;
        qp->head_t = toinsert;
        qp->q_size++;
        return;
    }

    struct thread* bf = qp->head_t;
    while (bf->next_t != NULL) {
        bf = bf->next_t;
    }
    bf->next_t = toinsert;
    toinsert->pre_t = bf;
    toinsert->next_t = NULL;
    qp->q_size++;
    interrupts_set(enabled);
}

void q_print(struct rdyq * qp) {
    struct thread * temp = qp->head_t;

    printf("\n{ ");
    while (temp != NULL) {
        printf("%i, ", temp->id);
        temp = temp->next_t;
    }
    printf("  }\n");

}

//RDY QUE IMPLEMENTATION ENDS

int thread_giveid(struct thread* to_b_assigned) {
    int enabled = interrupts_set(0);
    int count;
    for (count = 0; count < 1024; count++) {
        if (id_arr[count] == 0) {
            to_b_assigned->id = count;
            id_arr[count] = 1;
            interrupts_set(enabled);
            return 1;
        }
    }
    interrupts_set(enabled);
    return -1;
}

// this is main thread's thread structure
struct thread m_thread_inst;

void
thread_init(void) {

    rdy_que->q_size = 0;
    ext_que->q_size = 0;
    int count = 0;
    for (count = 0; count < 1024; count++) {
        id_arr[count] = 0;
    }
    struct thread * m_thread = &m_thread_inst;
    m_thread->ismain = 1;
    int assigned = thread_giveid(m_thread);
    assert(assigned);

    m_thread->State = RUNN;
    m_thread->stack_ptr = NULL;
    current_thread = m_thread;

}

Tid
thread_id() {
    return current_thread->id;
    return THREAD_INVALID; //???
}

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg) {
    Tid ret;

    int enabled = interrupts_set(1);
    thread_main(arg); // call thread_main() function with arg
    interrupts_set(enabled);
    ret = thread_exit();
    // we should only get here if we are the last thread. 
    assert(ret == THREAD_NONE);
    // all threads are done, so process should exit
    exit(0);
}

Tid
thread_create(void (*fn) (void *), void *parg) {
    int enabled = interrupts_set(0);


    struct thread * c_thread;
    int blank_space = 0;
    int c = 0;
    for (c = 0; c < 1024; c++) {
        if (id_arr[c] == 0) {
            blank_space = 1;
            break;
        }
    }
    if (blank_space == 0) {
        interrupts_set(enabled);
        return THREAD_NOMORE;
    }


    c_thread = malloc(sizeof (struct thread));
    if (c_thread == NULL) {
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }

    int k = thread_giveid(c_thread);
    assert(k);
    c_thread->ismain = 0;

    unsigned long long position;


    //get current context 
    //interrupts_set(enabled);
    int gg = getcontext(&(c_thread->t_context));
    assert(!gg);
    //interrupts_set(0);
    //allocate stack
    c_thread->stack_ptr = malloc(THREAD_MIN_STACK);
    if (c_thread->stack_ptr == NULL) {
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }


    //changing stack position
    position = (unsigned long long) c_thread->stack_ptr;
    position = position + THREAD_MIN_STACK;
    position = position / 16 * 16 - 8;

    c_thread->t_context.uc_mcontext.gregs[REG_RBP] = position;
    c_thread->t_context.uc_mcontext.gregs[REG_RSP] = position;

    //setting address of rip/rdi/rsi to stub function & its parameters
    c_thread->t_context.uc_mcontext.gregs[REG_RIP] = (unsigned long long) &thread_stub;
    c_thread->t_context.uc_mcontext.gregs[REG_RDI] = (unsigned long long) fn;
    c_thread->t_context.uc_mcontext.gregs[REG_RSI] = (unsigned long long) parg;

    //put the created thread into ready que with state = ready
    c_thread->State = RDY;
    q_tail_add(rdy_que, c_thread);
    interrupts_set(1);
    return c_thread->id;
}


Tid
thread_yield_do(Tid want_tid, int * pa) {
    //int enabled = interrupts_set(0);
    if ((want_tid == THREAD_SELF)) {
        want_tid = current_thread->id;
        *pa = want_tid;
        //interrupts_set(enabled);
        return want_tid;
    }

    if ((want_tid == THREAD_ANY)&&(rdy_que->head_t == NULL)) {
        *pa = THREAD_NONE;
        //interrupts_set(enabled);
        return THREAD_NONE;
    }



    int success = getcontext(&(current_thread->t_context));
    assert(!success);

    unsigned long long *reg = (unsigned long long *) current_thread->t_context.uc_mcontext.gregs[REG_RBP];
    current_thread->t_context.uc_mcontext.gregs[REG_RBP] = *reg;
    current_thread->t_context.uc_mcontext.gregs[REG_RIP] = *(reg + 1);
    current_thread->t_context.uc_mcontext.gregs[REG_RSP] = (unsigned long long) (reg + 2);
    current_thread->State = RDY;
    q_tail_add(rdy_que, current_thread);


    struct thread * popout;

    //IF ANY, POP HEAD
    if (want_tid == THREAD_ANY) {

        popout = q_head_deq(rdy_que);
        popout->State = RUNN;
        current_thread = popout;
        *pa = popout->id;

        int sss = setcontext(&(popout->t_context));
	printf("\ngayyyyyyyyyyyyyyy\n");
        assert(!sss);
        //interrupts_set(enabled);
        return yield_ret;
    } else {
        if (q_find(rdy_que, want_tid) == 0) {
            q_select(rdy_que, current_thread->id);
            current_thread->State = RUNN;
            *pa = THREAD_INVALID;
            //interrupts_set(enabled);
            return THREAD_INVALID;
        } else {
            popout = q_select(rdy_que, want_tid);
            popout->State = RUNN;
            *pa = popout->id;
            current_thread = popout;

            int ssss = setcontext(&(popout->t_context));
            assert(!ssss);

            return yield_ret;
        }
    }

}

Tid
thread_yield(Tid want_tid) {

    q_clear(ext_que);


    int a;
    int *pa = &a;
    int enabled = interrupts_set(0);
    thread_yield_do(want_tid, pa);
    interrupts_set(enabled);

    return a;
}

Tid
thread_exit() {
    int enabled = interrupts_set(0);

    //if theres no threads in the rdy_que, current thread is the only one running
    if (rdy_que->head_t == NULL) {
        interrupts_set(enabled);
        return THREAD_NONE;
    }


    //put current thread into exit que, get one thread from ready que to run
    //set current thread's state to exit
    current_thread->State = EXT;


    //put ct into ext que
    q_tail_add(ext_que, current_thread);

    //popout a thread from ready que to run
    struct thread * popout;
    popout = q_head_deq(rdy_que);
    popout->State = RUNN;
    current_thread = popout;

    int sss = setcontext(&(popout->t_context));
    assert(!sss);
    interrupts_set(enabled);
    return THREAD_FAILED;
}

Tid
thread_kill(Tid tid) {
    int enabled = interrupts_set(0);
    if ((q_find(rdy_que, tid) == 0) || (tid == current_thread->id)) {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    struct thread * temp;
    temp = q_select(rdy_que, tid);
    temp->State = EXT;
    q_tail_add(ext_que, temp);
    interrupts_set(enabled);

    return tid;
}


/* This is the wait queue structure */
struct wait_queue {
    /* ... Fill this in ... */
    struct rdyq* w8q;
};

struct wait_queue *
wait_queue_create() {
    struct wait_queue *wq;

    wq = malloc(sizeof (struct wait_queue));
    assert(wq);

    wq->w8q = malloc(sizeof (struct rdyq));
    assert(wq->w8q);
    //TBD();

    return wq;
}

void
wait_queue_destroy(struct wait_queue *wq) {
    int enabled = interrupts_set(0);
    q_clear(wq->w8q);
    free(wq->w8q);
    free(wq);
    interrupts_set(enabled);
}

Tid
thread_sleep(struct wait_queue *queue) {
    int enabled = interrupts_set(0);

    //if theres no threads in the rdy_que, current thread is the only one running
    if (queue == NULL) {

        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    if (rdy_que->head_t == NULL) {
        interrupts_set(enabled);
        return THREAD_NONE;
    }


    //put current thread into exit que, get one thread from ready que to run
    //set current thread's state to exit
    current_thread->State = EXT;
    int success = getcontext(&(current_thread->t_context));
    assert(!success);

    unsigned long long *reg = (unsigned long long *) current_thread->t_context.uc_mcontext.gregs[REG_RBP];
    current_thread->t_context.uc_mcontext.gregs[REG_RBP] = *reg;
    current_thread->t_context.uc_mcontext.gregs[REG_RIP] = *(reg + 1);
    current_thread->t_context.uc_mcontext.gregs[REG_RSP] = (unsigned long long) (reg + 2);

    //put ct into ext que
    q_tail_add(queue->w8q, current_thread);

    //popout a thread from ready que to run
    struct thread * popout;
    popout = q_head_deq(rdy_que);
    popout->State = RUNN;
    current_thread = popout;

    int sss = setcontext(&(popout->t_context));
    assert(!sss);
    interrupts_set(enabled);
    return popout->id;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all) {
    int enabled = interrupts_set(0);

    if (queue == NULL) {
        interrupts_set(enabled);
        return 0;
    }
    if (queue->w8q->head_t == NULL) {
        interrupts_set(enabled);
        return 0;
    }

    if (all == 0) {
        q_tail_add(rdy_que, q_head_deq(queue->w8q));
        interrupts_set(enabled);
        return 1;
    } else if (all == 1) {
        struct thread* temp = q_head_deq(queue->w8q);
        int count = 1;
        while (temp != NULL) {
            q_tail_add(rdy_que, temp);
            temp = q_head_deq(queue->w8q);
            count++;
        }
        interrupts_set(enabled);
        return count - 1;
    }
    interrupts_set(enabled);
    return 0;
}

struct lock {
    /* ... Fill this in ... */
    int availability;
    int holding;
    struct wait_queue *wq;
};

struct lock *
lock_create() {
    int enabled = interrupts_set(0);
    struct lock *lock;

    lock = malloc(sizeof (struct lock));
    assert(lock);

    lock->availability = 1;
    lock->holding = -1;
    lock->wq = wait_queue_create();

    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    wait_queue_destroy(lock->wq);

    free(lock);
    interrupts_set(enabled);
    return;
}

void
lock_acquire(struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(lock != NULL);

    while (1) {
        if (lock->availability == 1) {
            lock->availability = 0;
            lock->holding = current_thread->id;
            interrupts_set(enabled);
            return;
        } else if (lock->availability == 0) {
            thread_sleep(lock->wq);
        }
    }

}

void
lock_release(struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(lock != NULL);
    while (1) {
        if (lock->availability == 0 && lock->holding == current_thread->id) {
            lock->availability = 1;
            lock->holding = -1;
            thread_wakeup(lock->wq, 1);
            interrupts_set(enabled);
            return;
        }
    }

}

struct cv {
    /* ... Fill this in ... */
    int sleeper;
    struct wait_queue * wq;
};

struct cv *
cv_create() {
    struct cv *cv;

    cv = malloc(sizeof (struct cv));
    assert(cv);
    cv->sleeper = 0;
    cv->wq = wait_queue_create();

    return cv;
}

void
cv_destroy(struct cv *cv) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);

    wait_queue_destroy(cv->wq);

    free(cv);
    interrupts_set(enabled);
}

void
cv_wait(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);

    assert(cv != NULL);
    assert(lock != NULL);

        if (lock->holding == current_thread->id) {
            lock_release(lock);
            cv->sleeper++;
            thread_sleep(cv->wq);
            lock_acquire(lock);
            interrupts_set(enabled);
            return;
        }



}

void
cv_signal(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->holding == current_thread->id) {
        thread_wakeup(cv->wq, 0);
    }
    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock *lock) {
    int enabled = interrupts_set(0);
    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->holding == current_thread->id) {
        thread_wakeup(cv->wq, 1);
    }
    interrupts_set(enabled);
    
    
    
}
