//gcc -g -Wall -o bin/Hex_to_double_RGB Hex_to_double_RGB.c

/*
 * Copyright (C) 2017 Santiago León O. <santileortiz@gmail.com>
 */

#include <stdio.h>

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} vect3_int_t;

int main (int argc, char **argv)
{
    unsigned int test = 0;
    if (argc == 2) {
        sscanf (argv[1], "%X", &test);

        vect3_int_t col;
        col.r = (test & 0xFF0000) >> 16;
        col.g = (test & 0x00FF00) >> 8;
        col.b = test & 0x0000FF;
        printf ("{{%d, %d, %d}},\n", col.r, col.g,  col.b);
        printf ("{{%f, %f, %f}},\n", (double)col.r/255, (double)col.g/255,  (double)col.b/255);
    }
}
