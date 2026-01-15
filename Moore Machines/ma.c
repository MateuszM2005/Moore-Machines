#include <stdio.h>
#include "stdint.h"
#include "ma.h"
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#ifndef BITS_TO_WORDS
#define BITS_TO_WORDS(x) (((x) == 0) ? 0 : (1 + (((x) - 1) / 64)))
#endif

typedef struct input input_bit; //each connection is represented by this
struct input { //ma_out -> input -> ma_in
    uint64_t posInOut, posInIn, indexInIn;
    moore_t *ma_in, *ma_out;
}; //maybe the name is quite mid

struct moore {
    uint64_t s;
    uint64_t* state;

    uint64_t in;
    input_bit** input;
    uint64_t* inputBits;

    uint64_t con;
    input_bit** connections;

    uint64_t out;
    uint64_t* output;

    transition_function_t transition_function;
    output_function_t output_function;
};

//DEBUGING
void wtorekAutomaty(moore_t* out) { //debugging only safe to delete
    printf("state: %"PRIu64", length: %"PRIu64"\n",out->state[0],out->s);
    printf("output: %"PRIu64", length: %"PRIu64"\n", out->output[0], out->in);
}

//OUTPUT FUNCTION FOR MA_SIMPLE
void identical(uint64_t *output, uint64_t const *state,
                                  size_t m, size_t s) {
    m = s;
    for(unsigned i = 0; i < BITS_TO_WORDS(m); i++) output[i] = state[i];
}

//BIT OPERATIONS - helper functions
void setBit(uint64_t* to, const uint64_t* from, uint64_t indexTo, uint64_t indexFrom) {
    uint64_t wordTo = indexTo / 64;
    uint64_t bitTo = indexTo % 64;

    uint64_t wordFrom = indexFrom / 64;
    uint64_t bitFrom = indexFrom % 64;

    uint64_t bit = (from[wordFrom] >> bitFrom) & 1ULL;

    to[wordTo] &= ~(1ULL << bitTo);
    to[wordTo] |= (bit << bitTo);
}

void setManually(uint64_t* to, uint64_t from, uint64_t indexTo, uint64_t indexFrom) {
    const uint64_t wordTo = indexTo / 64;
    const uint64_t bitTo = indexTo % 64;

    const uint64_t bit = (from >> indexFrom) & 1;

    to[wordTo] &= ~(1ULL << bitTo);
    to[wordTo] |= (bit << bitTo);
}

//copies from to to and zeroes excess bits
void rewrite(uint64_t* to, const uint64_t* from, uint64_t length) {
    const uint64_t fullWords = length / 64;
    const uint64_t remainingBits = length % 64;

    for(uint64_t i = 0; i < fullWords; ++i) {
        to[i] = from[i];
    }

    if (remainingBits > 0) { //zero the excess bits
        const uint64_t mask = (1ULL << remainingBits) - 1;
        to[fullWords] = from[fullWords] & mask;
    }
}

//DELETE FUNCTIONS
void freeInput(input_bit *i) { //leaves no pointer behind
    if(i == NULL) return;
    if(i->ma_in) i->ma_in->connections[i->indexInIn] = NULL;
    if(i->ma_out) i->ma_out->input[i->posInOut] = NULL;
    free(i);
    i = NULL;
}

void ma_delete(moore_t *a) { //very safe in case malloc error happened mid initialization
    if(a == NULL) return;

    if(a->input) {
        for(uint64_t i = 0; i < a->in; i++) {
            if(a->input[i]) {
                freeInput(a->input[i]);
                a->input[i] = NULL;
            }
        }
        free(a->input);
        a->input = NULL;
    }

    if(a->inputBits) {
        free(a->inputBits);
        a->inputBits = NULL;
    }

    if(a->connections) {
        for(uint64_t i = 0; i < a->con; i++) {
            if(a->connections[i]) {
                freeInput(a->connections[i]);
                a->connections[i] = NULL;
            }
        }
        free(a->connections);
        a->connections = NULL;
    }

    if(a->state) {
        free(a->state);
        a->state = NULL;
    }
    if(a->output) {
        free(a->output);
        a->output = NULL;
    }

    free(a);
}


//OPERATIONS USING TRANSITION AND OUTPUT FUNCTIONS
void ma_state(moore_t* a) {
    //create input
    uint64_t input[BITS_TO_WORDS(a->in)];
    for(uint64_t i = 0; i < BITS_TO_WORDS(a->in); ++i) {
        input[i] = 0;
    }
    for(uint64_t i = 0; i < a->in; ++i) {
        if(a->input[i]) {
            if(a->input[i]->ma_in) {
                uint64_t* from = a->input[i]->ma_in->output;
                uint64_t indexFrom = a->input[i]->posInIn;
                setBit(input, from,i, indexFrom);
            }
        }
        else {
            setBit(input, a->inputBits, i, i);
        }
    }

    //create a copy of state array
    uint64_t state[BITS_TO_WORDS(a->s)];
    for(uint64_t i = 0; i < BITS_TO_WORDS(a->s); ++i) {
        state[i] = a->state[i];
    }

    //complete the calculation of the new state
    a->transition_function(a->state, input, state, a->in, a->s);

}

void ma_output(moore_t* a) {
    //output calculation is very simple
    a->output_function(a->output, a->state, a->out, a->s);
}

int rewriteConnections(moore_t* a_out, const uint64_t num) {
    //resize and remove NULLs
    uint64_t count = 0;
    for(uint64_t i = 0; i < a_out->con; ++i) {
        if(a_out->connections[i]) ++count;
    }
    input_bit** newConnections = (input_bit**)malloc((count + num) * sizeof(input_bit*));
    if(newConnections == NULL) {
        errno = ENOMEM;
        return 1;
    }
    uint64_t pos = 0;
    for(uint64_t i = 0; i < a_out->con; ++i) {
        if(a_out->connections[i]) {
            newConnections[pos] = a_out->connections[i];
            newConnections[pos]->indexInIn = pos;
            ++pos;
        }
    }
    for(; pos < count + num; ++pos) newConnections[pos] = NULL;
    free(a_out->connections);
    a_out->connections = newConnections;
    a_out->con = count + num; //ZMIANA 3: dopisanie num i wyzerowanie reszty tablicy
    return 0;
}

//USER FUNCTIONS (EXCEPT DELETE)
moore_t * ma_create_full(size_t n, size_t m, size_t s, transition_function_t t,
                         output_function_t y, uint64_t const *q) {
    if(s == 0 || m == 0 || t == NULL || y == NULL || q == NULL) {
        errno = EINVAL;
        return NULL;
    }
    moore_t *out = malloc(sizeof(moore_t)); //declare ma itself
    if(out == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    out->s = 0;
    out->in = 0;
    out->con = 0;
    out->out = 0;
    out->transition_function = NULL;
    out->output_function = NULL;
    out->state = NULL;
    out->input = NULL;
    out->connections = NULL;
    out->inputBits = NULL;
    out->output = NULL;

    out->state = (uint64_t*)malloc(BITS_TO_WORDS(s) * sizeof(uint64_t));
    if(out->state == NULL) {
        ma_delete(out);
        errno = ENOMEM;
        return NULL;
    }
    for(uint64_t i = 0; i < BITS_TO_WORDS(s); ++i) {
        out->state[i] = 0;
    }
    out->input = (input_bit**)malloc(n >= SIZE_MAX / sizeof(input_bit*)
        ? 9223372036854775807 : n * sizeof(input_bit*));
    if(out->input == NULL) {
        ma_delete(out);
        errno = ENOMEM;
        return NULL;
    }
    for(uint64_t i = 0; i < n; ++i) {
        out->input[i] = NULL;
    }
    out->inputBits = (uint64_t*)malloc(BITS_TO_WORDS(n) * sizeof(uint64_t));
    if(out->inputBits == NULL) {
        ma_delete(out);
        errno = ENOMEM;
        return NULL;
    }
    for(uint64_t i = 0; i < BITS_TO_WORDS(n); ++i) {
        out->inputBits[i] = 0;
    }

    out->connections = NULL;

    out->output = (uint64_t*)malloc(BITS_TO_WORDS(m) * sizeof(uint64_t));
    if(out->output == NULL) {
        ma_delete(out);
        errno = ENOMEM;
        return NULL;
    }
    for(uint64_t i = 0; i < BITS_TO_WORDS(m); ++i) {
        out->output[i] = 0;
    }

    out->s = s;
    out->in = n;
    out->out = m;

    rewrite(out->state, q, s);
    out->transition_function = t;
    out->output_function = y;

    ma_output(out);
    return out;
}

moore_t * ma_create_simple(size_t n, size_t s, transition_function_t t) {
    uint64_t *data = malloc(BITS_TO_WORDS(s) * sizeof(uint64_t));
    if(data == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    for(uint64_t i = 0; i < BITS_TO_WORDS(s); i++) data[i] = 0;
    moore_t* out = ma_create_full(n, s, s, t,identical, data);
    free(data);
    return out;
}

int ma_connect(moore_t *a_in, size_t in, moore_t *a_out, size_t out, size_t num) {
    //check for invalid data
    if(a_in == NULL || a_out == NULL || num == 0
        || num + in > a_in->in || num + out > a_out->out
        || num > UINT64_MAX - in || num > UINT64_MAX - out) {
        errno = EINVAL;
        return -1;
    }
    /* ZMIANA 2: uwzględnienie overflowu tu i w innych miejscach */
    //rewrit and resize connection array, remove NULLs
    if(rewriteConnections(a_out, num)) {
        return -1;
    }
    uint64_t pos = a_out->con - num;
    //create new connections - this is done seperately in case of memory error
    input_bit** connections = (input_bit**)malloc((num) * sizeof(input_bit*));
    if(connections == NULL) {
        errno = ENOMEM;
        return -1;
    }
    for(uint64_t i = 0; i < num; ++i) { //in case of bad malloc - remove them all.
        connections[i] = (input_bit*)malloc(sizeof(input_bit));
        if(connections[i] == NULL) {
            while (i > 0) {
                free(connections[i-1]);
                --i;
            }
            free(connections);
            errno = ENOMEM;
            return -1;
        }
    }
    //input those connections

    for(uint64_t i = 0; i < num; ++i) {
        if(a_in->input[i + in]) freeInput(a_in->input[i + in]);
        a_in->input[i+in] = connections[i];
        connections[i] = NULL;

        a_in->input[i+in]->posInOut = in + i;
        a_in->input[i+in]->posInIn = out + i;

        a_in->input[i+in]->ma_in = a_out;
        a_in->input[i+in]->ma_out = a_in;

        a_in->input[i+in]->indexInIn = pos + i;
        a_out->connections[pos + i] = a_in->input[i+in];
    }
    free(connections);
    ma_output(a_in);
    return 0;
}

int ma_disconnect(moore_t *a_in, size_t in, size_t num) {
    if(a_in == NULL || num == 0 || num + in > a_in->in || num > UINT64_MAX - in) {
        errno = EINVAL;
        return -1;
    }
    for(uint64_t i = in; i < num + in; ++i) {
        if(a_in->input[i]) freeInput(a_in->input[i]);
        a_in->input[i] = NULL;
    }
    return 0;
}

int ma_set_input(moore_t *a, uint64_t const *input) {
    if(a == NULL || input == NULL || a->in == 0) { //ZMIANA NR 1: dodanie a->in == 0
        errno = EINVAL;
        return -1;
    }

    rewrite(a->inputBits, input, a->in);

    ma_output(a);
    return 0;
}

int ma_set_state(moore_t *a, uint64_t const *state) {
    if(a == NULL || state == NULL) {
        errno = EINVAL;
        return -1;
    }
    rewrite(a->state, state, a->s);
    ma_output(a);
    return 0;
}

uint64_t const* ma_get_output(moore_t const *a) {
    if(a == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return a->output;
}

int ma_step(moore_t *at[], size_t num) {
    if(at == NULL || num == 0) {
        errno = EINVAL;
        return -1;
    }
    for(uint64_t i = 0; i < num; ++i) {
        if(at[i] == NULL) {
            errno = EINVAL;
            return -1;
        }
    }
    for(uint64_t i = 0; i < num; ++i) {
        ma_state(at[i]);
    }
    for(uint64_t i = 0; i < num; ++i) {
        ma_output(at[i]);
    }
    return 0;
}
