#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "wc.h"
//10000000

struct wc {
    /* you can define this struct to have whatever fields you want. */
    struct words *h_table[20000000]; //SIZE WILL CHANGE LATER

};

struct words {
    char *wordi;
    int times;
};

unsigned long
hash_str(unsigned char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

struct wc *
wc_init(char *word_array, long size) {
    struct wc *wc;

    wc = (struct wc *) malloc(sizeof (struct wc));
    assert(wc);

    char *copy_array = strdup(word_array);



    char *tken = strtok(copy_array, " \n\t\f\r");


    while (tken != NULL) {



        //


        unsigned long index = hash_str((unsigned char *) tken) % 20000000;

        int done = 0;
        while (done == 0) {
            if (wc->h_table[index] != NULL) {
                if (strcmp(wc->h_table[index]->wordi, tken) == 0) {
                    wc->h_table[index]->times++;
                    done = 1;
                    //printf("inserted\n");//test
                    //free(tinsert->wordi);
                    //free(tinsert);
                } else {
                    index = index + 1;
                    index = index % 20000000;
                    //printf("collide\n");//test
                }
            } else if (wc->h_table[index] == NULL) {
                struct words* tinsert = (struct words *) malloc(sizeof (struct words)); //new structure contains: char*, int
                tinsert->wordi = strdup(tken);
                tinsert->times = 1;
                wc->h_table[index] = tinsert;
                done = 1;
                //printf("inserted\n");//test
            }
        }

        tken = strtok(NULL, " \n\t\f\r");
        //tinsert = NULL;
    }

    free(tken);
    free(copy_array);

    return wc;
}

void
wc_output(struct wc *wc) {
    int hlp = 0;
    for (hlp = 0; hlp < 20000000; hlp++) {
        if (wc->h_table[hlp] != NULL)
            printf("%s:%i\n", wc->h_table[hlp]->wordi, wc->h_table[hlp]->times);
    }

}

void
wc_destroy(struct wc *wc) {
    int i = 0;
    for (i = 0; i < 20000000; i++) {//SIZE***
        if (wc->h_table[i] != NULL) {
            free(wc->h_table[i]->wordi);
            wc->h_table[i]->wordi = NULL;
        }
        free(wc->h_table[i]);
        wc->h_table[i] = NULL;
    }
    free(wc);
}
