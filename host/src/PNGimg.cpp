#include "PNGimg.h"

int openImg(int* a_width, int* a_height, png_bytep **rows){
        printf("Opening img\n");

        FILE* img = NULL;
        int width;
        int height;
        png_byte color_type;
        png_byte bit_depth;
        png_bytep *row_pointers;

        img = fopen("./bin/rlc.png", "rb");

        printf("%d\n", img);
        if(img == NULL){
                printf("Error opening image\n");
                return -1;
        }

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
        if(!png){printf("Error creating read struct\n"); return -1;}

        png_infop info = png_create_info_struct(png);
        if(!info){printf("Error creating info struct\n"); return -1;}

        if(setjmp(png_jmpbuf(png))) return -1;

        png_init_io(png, img);

        png_read_info(png, info);

        width      = png_get_image_width(png, info);
        height     = png_get_image_height(png, info);
        color_type = png_get_color_type(png, info);
        bit_depth  = png_get_bit_depth(png, info);

// FORMAT

	if(bit_depth == 16){
    		png_set_strip_16(png);
	}
	if(color_type == PNG_COLOR_TYPE_PALETTE){
    		png_set_palette_to_rgb(png);
	}
	if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8){
 	   	png_set_expand_gray_1_2_4_to_8(png);
	}
  	if(png_get_valid(png, info, PNG_INFO_tRNS)){
    		png_set_tRNS_to_alpha(png);
	}
	if(	color_type == PNG_COLOR_TYPE_RGB ||
     		color_type == PNG_COLOR_TYPE_GRAY ||
     		color_type == PNG_COLOR_TYPE_PALETTE){
    			png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	}
  	if(	color_type == PNG_COLOR_TYPE_GRAY ||
     		color_type == PNG_COLOR_TYPE_GRAY_ALPHA){
    			png_set_gray_to_rgb(png);
	}

	png_read_update_info(png, info);

// STOP FORMAT

        row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
        for(int y = 0; y < height; y++) {
                row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
        }

        png_read_image(png, row_pointers);

        printf("W : %d, H : %d\n", width, height);
	
	png_bytep row = row_pointers[12];
	png_bytep px  = &(row[2 * 4]);

	printf("2, 12 = RGBA(%3d, %3d, %3d, %3d)\n", px[0], px[1], px[2], px[3]);

        fclose(img);
	img = NULL;

	*a_width = width;
	*a_height = height;
	*rows = row_pointers;

	// FREE all the memory
	if (png && info){
        	png_destroy_read_struct(&png, &info, NULL);
		png = NULL;
		info = NULL;
	}
        
	return 0;
}

int getRGBpixel(int **r, int **g, int **b, int width, int height, png_bytep *rows)
{
	int nbPixel = width * height;
	int datasize = nbPixel * sizeof(int);
	int *red, *green, *blue;
	
	red 	= (int*)malloc(datasize);
	green 	= (int*)malloc(datasize);
	blue 	= (int*)malloc(datasize);

	if(red == NULL or green == NULL or blue == NULL){
		printf("Out of memory");
		return -1;
	} 
	for(int y = 0; y < height; y++){
		png_bytep row = rows[y];
		for(int x = 0; x < width; x++){
			png_bytep px = &(row[x * 4]);
			red[y * width + x]= px[0];
			green[y * width + x] = px[1];
			blue[y * width + x] = px[2];
		}
	}	

	*r = red;
	*g = green;
	*b = blue;	

	printf("RGB copied\n");

	return 0;
}

void process(int width, int height, png_bytep *rows, int* grey){
	for(int y = 0; y < height; y++){
		png_bytep row = rows[y];
		for(int x = 0; x < width; x++){
			png_bytep px = &(row[x * 4]);
			px[0] = grey[y * width + x];
			px[1] = grey[y * width + x];
			px[2] = grey[y * width + x];
		}
	}	
}

void write_png_file(int width, int height, png_bytep *row_pointers) {
  	int y;

  	FILE *fp = fopen("out.png", "wb");
	  if(!fp) abort();

	  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	  if (!png) abort();

	  png_infop info = png_create_info_struct(png);
	  if (!info) abort();

	  if (setjmp(png_jmpbuf(png))) abort();

	  png_init_io(png, fp);

	  // Output is 8bit depth, RGBA format.
	  png_set_IHDR(
		png,
        	info,
	        width, height,
       	 	8,
	        PNG_COLOR_TYPE_RGBA,
        	PNG_INTERLACE_NONE,
	        PNG_COMPRESSION_TYPE_DEFAULT,
	        PNG_FILTER_TYPE_DEFAULT
	  );
	  png_write_info(png, info);
 
	  png_write_image(png, row_pointers);
	  png_write_end(png, NULL);
 
	  fclose(fp);
}

void draw_line(png_bytep *row, int rDim, int phiDim, int accPos, float discR, float discPhi){
	// accumulator format (r,phi)	

	printf("Start drawing lines\n");
	
	// Since width of acc is rDim -> Xpos = pos % rDim
	// Ypos = pos / rdim -> int / int always floored (if both positive)
	int rPos = accPos % rDim;
	int phiPos = accPos / rDim;

	printf("phi = %d , r = %d\n",phiPos,rPos);
}
