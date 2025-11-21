/// 
//  mandel_movie.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  Quinn Lindgren
//  11/18/25
//  CPE 2600 Lab 12
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include "jpegrw.h"

#define FRAMES 50

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void* compute_section(void* params);
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, int num_threads );
static void show_help();

typedef struct parameters{
	double xcenter;
	double ycenter;
	double xscale;
	double yscale;
	int image_width;
	int image_height;
	int max;
	int nproc;
	int acproc;
	int nthreads;
}parameters;

typedef struct thread_parameters{
    imgRawImage *img;
    double xmin;
	double xmax;
	double ymin;
	double ymax;
    int max;
    int start;
    int end;
}thread_parameters;


int main( int argc, char *argv[] )
{
	char c;
	parameters param_vals = {0,0,4,0,1000,1000,1000,1,1,1};
	parameters *params = &param_vals;
	mmap(params, sizeof(*params), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	// These are the default configuration values used
	// if no command line arguments are given.
	char outfile[FRAMES][16]; // store every file name in string array
	// For each command line argument given,
	// override the appropriate configuration value.
    while((c = getopt(argc,argv,"x:y:s:W:H:m:n:t:h"))!=-1) {
		switch(c) 
		{
			case 'x':
				params->xcenter = atof(optarg);
				break;
			case 'y':
				params->ycenter = atof(optarg);
				break;
			case 's':
				params->xscale = atof(optarg);
				break;
			case 'W':
				params->image_width = atoi(optarg);
				break;
			case 'H':
				params->image_height = atoi(optarg);
				break;
			case 'm':
				params->max = atoi(optarg);
				break;
			case 'n':
				params->nproc = atoi(optarg);
				break;
			case 't':
				params->nthreads = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	if(params->nproc < 1){
		printf("Must use at least one process - using default (1)\n");
		params->nproc = 1;
	} else if(params->nthreads < 1){
		printf("Must use at least one thread - using default (1)\n");
		params->nthreads = 1;
	}
	
    for(int i = 0; i < FRAMES; i++){
		if(params->acproc > params->nproc){ 
			wait(NULL);
			params->acproc = params->acproc - 1;
		}
		int pid = fork();
		if(pid == 0){
				
		sprintf(outfile[i],"mandel%d.jpg",i);
		params->xscale = params->xscale + (0.001 * 0.5*i); // increment scale to zoom
				// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
				params->yscale = params->xscale / params->image_width * params->image_height;

				// Display the configuration of the image.
				printf("mandel%d: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",i,params->xcenter,params->ycenter,params->xscale,params->yscale,params->max,outfile[i]);

				// Create a raw image of the appropriate size.
				imgRawImage* img = initRawImage(params->image_width,params->image_height);

				// Fill it with a black
				setImageCOLOR(img,0);

				// Compute the Mandelbrot image
				compute_image(img,params->xcenter-params->xscale/2,params->xcenter+params->xscale/2,params->ycenter-params->yscale/2,params->ycenter+params->yscale/2,params->max,params->nthreads);

				// Save the image in the stated file.
				storeJpegImageFile(img,outfile[i]);

				// free the mallocs
				freeRawImage(img);

				exit(0);
		} else{
			params->acproc = params->acproc + 1;;
		}
    }
	while(params->acproc > 0){
		wait(NULL);
		params->acproc = params->acproc - 1;
	}

	munmap(params, sizeof(*params));

	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads )
{
	int height = img->height;

	pthread_t threads[num_threads];
	thread_parameters tparams[num_threads];
	int row_thread = height / num_threads;

	for (int i = 0; i < num_threads; i++){
		tparams[i].img = img;
		tparams[i].xmin = xmin;
		tparams[i].xmax = xmax;
		tparams[i].ymin = ymin;
		tparams[i].ymax = ymax;
		tparams[i].max = max;
		tparams[i].start = i * row_thread;
		if(i == num_threads - 1){
			tparams[i].end = height;
		} else{
			tparams[i].end = (i + 1) * row_thread;
		}
		pthread_create(&threads[i], NULL, compute_section, &tparams[i]);
	}
	for(int i = 0; i < num_threads; i++){
		pthread_join(threads[i], NULL);
	}
}

void* compute_section(void* params){
	thread_parameters *tparams = (thread_parameters*) params;
	int width = tparams->img->width;

	for(int j=tparams->start;j<tparams->end;j++) {

		for(int i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = tparams->xmin + i*(tparams->xmax-tparams->xmin)/width;
			double y = tparams->ymin + j*(tparams->ymax-tparams->ymin)/tparams->img->height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,tparams->max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(tparams->img,i,j,iteration_to_color(iters,tparams->max));
		}
	}
	return NULL;
}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}

// Show help message
void show_help()
{
	printf("Use: mandel_movie [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-n <procs> Number of processes to split into\n");
	printf("-t <threads> Number of threads to split into\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel_movie -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel_movie -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel_movie -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}