#include "common.h"

#include <stdio.h>
#include <string.h>


void clear_input_buffer()
{

    int c;

    while((c=getchar())!='\n'
            &&
          c!=EOF);

}



void get_string(
        char *buffer,
        int size
)
{

    fgets(
        buffer,
        size,
        stdin
    );


    buffer[strcspn(
        buffer,
        "\n"
    )]=0;

}


void wait_enter()
{
    printf("\nPress Enter continue...");
    getchar();
    getchar();
}


void print_success(char *msg)
{
    printf("\n[SUCCESS] %s\n",msg);
}


void print_error(char *msg)
{
    printf("\n[ERROR] %s\n",msg);
}


void print_line()
{

    printf(
    "===========================================\n"
    );

}
