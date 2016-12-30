#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <math.h>

int openImg(int* a_width, int* a_height, png_bytep **rows);
int getRGBpixel(int **r, int **g, int **b, int width, int height, png_bytep *rows);
void write_png_file(int width, int height, png_bytep *row_pointers);
void process(int width, int height, png_bytep *rows, int* grey);
void draw_line(png_bytep *row, int rDim, int phiDim, int accPos, float discR, float discPhi);
