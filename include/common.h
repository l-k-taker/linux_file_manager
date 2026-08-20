#ifndef COMMON_H
#define COMMON_H


#define BUFFER_SIZE 1024


void clear_input_buffer();


void get_string(
        char *buffer,
        int size
);


void print_line();

void wait_enter();
void print_success(char *msg);
void print_error(char *msg);

#endif
