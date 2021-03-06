#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <math.h>
#define HM_SIZE 100

/** Read the JPEG image at `filename` as an array of bytes.
 Data is returned through the out pointers, while the return
 value indicates success or failure.
 NOTE: 1) if image is RGB, then the bytes are concatenated in R-G-B order
 2) `image` should be freed by the user
 */

int backgroundColor = 255;
int foregroundColor = 0;
int _width, _height, _channels;

typedef struct {
	int firstVal;
	int secondVal;
} pair;

typedef struct {
	int size;
	int position;
	pair *pairs;
} vector;

void init(vector* v) {
	v->size = 100;
	v->position = 0;
	v->pairs = malloc(sizeof(v->pairs) * v->size);
}

void push(int val1, int val2, vector* v) {
	if(v->position == v->size) {
		pair *tmp = malloc(sizeof(tmp) * v->size * 2);
		for(int i = 0; i < v->size; i++) {
			tmp[i] = v->pairs[i];
		}
		v->size *= 2;
		free(v->pairs);
		v->pairs = tmp;
	}
	v->pairs[v->position].firstVal = val1;
	v->pairs[v->position].secondVal = val2;
	v->position++;
}

void destroy(vector* v) {
	free(v->pairs);
}

static inline int
read_JPEG_file(char *filename,
			   int *width, int *height, int *channels, unsigned char *(image[]))
{
	FILE *infile;
	if ((infile = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		return 0;
	}
	
	struct jpeg_error_mgr jerr;
	struct jpeg_decompress_struct cinfo;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, infile);
	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);
	
	*width = cinfo.output_width, *height = cinfo.output_height;
	*channels = cinfo.num_components;
	// printf("width=%d height=%d c=%d\n", *width, *height, *channels);
	*image = malloc(*width * *height * *channels * sizeof(*image));
	JSAMPROW rowptr[1];
	int row_stride = *width * *channels;
	
	while (cinfo.output_scanline < cinfo.output_height) {
		rowptr[0] = *image + row_stride * cinfo.output_scanline;
		jpeg_read_scanlines(&cinfo, rowptr, 1);
	}
	jpeg_finish_decompress(&cinfo);
	
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return 1;
}


/** Writes the image in the specified file.
 NOTE: works with Grayscale or RGB modes only (based on number of channels)
 */
static inline void
write_JPEG_file(char *filename, int width, int height, int channels,
				unsigned char image[], int quality)
{
	FILE *outfile;
	if ((outfile = fopen(filename, "wb")) == NULL) {
		fprintf(stderr, "can't open %s\n", filename);
		exit(1);
	}
	
	struct jpeg_error_mgr jerr;
	struct jpeg_compress_struct cinfo;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo,outfile);
	
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = channels;
	cinfo.in_color_space = channels == 1 ? JCS_GRAYSCALE : JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	
	jpeg_start_compress(&cinfo, TRUE);
	JSAMPROW rowptr[1];
	int row_stride = width * channels;
	while (cinfo.next_scanline < cinfo.image_height) {
		rowptr[0] = & image[cinfo.next_scanline * row_stride];
		jpeg_write_scanlines(&cinfo, rowptr, 1);
	}
	jpeg_finish_compress(&cinfo);
	
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
}

int _getThreshold(int theta, unsigned char image[]) {
	int position, currentRow, currentColumn, currentChannel, avg, g, l, countg, countl;
	
	g = 0;
	l = 0;
	countg = 0;
	countl = 0;
	
	for(int h = 0; h < _height; h++) {
		currentRow = h*_width*_channels;
		for(int w = 0; w < _width; w++) {
			currentColumn = w*_channels;
			for(int currentChannel = 0; currentChannel < _channels; currentChannel++) {
				position = currentRow + currentColumn + currentChannel;
				if((int)image[position] > theta) {
					g += (int)image[position];
					countg++;
				}
				else {
					l += (int)image[position];
					countl++;
				}
			}
		}
	}
	
	g /= countg;
	l /= countl;
	
	avg = (g + l)/2;
	
	if (avg == theta) {
		return theta;
	}
	
	return _getThreshold(avg, image);
}

int getThreshold(unsigned char image[]) {
	int position = 0, threshold, cornerAvg;
	
	int avgChannels = 0;
	for(int currentChannel = 0; currentChannel < _channels; currentChannel++) {
		position = 0;
		avgChannels += image[position];
		position = (_width - 1)*_channels + currentChannel;
		avgChannels += image[position];
		position = (_height-1)*_width*_channels + currentChannel;
		avgChannels += image[position];
		position = (_height-1)*_width*_channels + (_width - 1)*_channels + currentChannel;
		avgChannels += image[position];
	}
	
	threshold = avgChannels / (_channels*4);
	
	return _getThreshold(threshold, image);
}

void imgThresholding(int threshold, unsigned char image[]) {
	int position, currentRow, currentColumn, currentChannel;
	for(int h = 0; h < _height; h++) {
		currentRow = h*_width*_channels;
		for(int w = 0; w < _width; w++) {
			currentColumn = w*_channels;
			for(int currentChannel = 0; currentChannel < _channels; currentChannel++) {
				position = currentRow + currentColumn + currentChannel;
				if((int)image[position] > threshold) {
					image[position] = backgroundColor;
				}
				else {
					image[position] = foregroundColor;
				}
			}
		}
	}
	
}

void initLabelArray(unsigned char labels[]) {
	int position, currentRow;
	for(int h = 0; h < _height; h++) {
		currentRow = h*_width;
		for(int currentColumn = 0; currentColumn < _width; currentColumn++) {
			position = currentRow + currentColumn;
			labels[position] = 0;
		}
	}
}

int isValid(int row, int col) {
	return (row >= 0 && col >= 0 && row < _height && col < _width);
}

int getCaseAndSetLabel(vector *linked, unsigned char labels[], int row, int col, int *setLabel) {
	int posNeighbor, labeled, diffLabels, currentLabel, min;
	int neighborRow[4] = {-1, -1, -1, 0};
	int neighborCol[4] = {1, 0, -1, -1};
	labeled = 0;
	diffLabels = 0;
	min = -1;
	
	for(int i = 0; i < 4; i++) {
		if(isValid(row+neighborRow[i], neighborCol[i]+col)) {
			posNeighbor = (row+neighborRow[i])*_width + (neighborCol[i]+col);
			if(labels[posNeighbor] != 0) {
				currentLabel = labels[posNeighbor];
				if(currentLabel != min) {
					if (min == -1) {
						min = currentLabel;
					}
					else {
						diffLabels = 1;
						if(currentLabel < min) {
							push(min, currentLabel, linked);
							min = currentLabel;
						}
						else {
							push(currentLabel, min, linked);
						}
					}
				}
				labeled++;
			}
		}
	}
	
	if (setLabel != NULL) {
		*setLabel = min;
	}
	
	return labeled == 0;
}

void firstPass(vector *linked, unsigned char labels[], unsigned char image[]) {
	int position, currentRow, nextLabel, minLabel, minNeighbor, caseLabel, setLabel;
	nextLabel = 100;
	
	for(int h = 0; h < _height; h++) {
		currentRow = h*_width;
		for(int currentColumn = 0; currentColumn < _width; currentColumn++) {
			position = currentRow + currentColumn;
			
			if((int)image[position] != backgroundColor) {
        caseLabel = getCaseAndSetLabel(linked, labels, h, currentColumn, &setLabel);
				if (caseLabel) {
					labels[position] = nextLabel;
					nextLabel += 30;
				} else {
					labels[position] = setLabel;
				}
			}
		}
	}
}

int cmpfunc (const void *a, const void *b) {
	return (((pair *)b)->firstVal - ((pair *)a)->firstVal);
}
	
void secondPass(vector *linked, unsigned char labels[]) {
  qsort(linked->pairs, linked->position, sizeof(int), cmpfunc);
  int min, j, currentRow, position;
  for(int h = 0; h < _height; h++) {
    currentRow = h*_width;
    for(int currentColumn = 0; currentColumn < _width; currentColumn++) {
      position = currentRow + currentColumn;
      if((int)labels[position] != 0) {
        int i = 0;
        while(linked->pairs[i].firstVal > labels[position] && i < linked->position) {
          if(labels[position] == linked->pairs[i].firstVal) {
            labels[position] = linked->pairs[i].secondVal;
          }
          i++;
        }
      }
    }
  }
}

void cca(unsigned char labels[], unsigned char image[]){
	vector linked;
  init(&linked);
	initLabelArray(labels);
	
	firstPass(&linked, labels, image);
	secondPass(&linked, labels);
  destroy(&linked);
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("Usage: ./ex1 image_in.jpg image_out.jpg\n");
		return 1;
	}
	
	unsigned char *image;
	int threshold;
	
	read_JPEG_file(argv[1], &_width, &_height, &_channels, &image);
	
	threshold = getThreshold(image);
	imgThresholding(threshold, image);
	
	unsigned char *labels;
	labels = malloc(_width * _height * sizeof(*labels));
	cca(labels, image);
	
	write_JPEG_file(argv[2], _width, _height, _channels, labels, 95);
	
	return 0;
}
