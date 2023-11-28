//
//  main.c
// CSC412 - Fall 2023 - Prog 06
//
//  Created by Jean-Yves Hervé on 2017-05-01, modified 2023-11-12
//

#include <iostream>
#include <vector>
#include <random>
#include <pthread.h>
//
#include <cstdio>
#include <cstdlib>
#include <time.h>
//
#include "gl_frontEnd.h"
#include "ImageIO_TGA.h"

using namespace std;

//==================================================================================
//	Function prototypes
//==================================================================================
void myKeyboard(unsigned char c, int x, int y);
void initializeApplication(int argc, char** argv);
void fill_window(unsigned char window[5][5], RasterImage* image, int row, int col);
void* threadFunc(void* argument);


//==================================================================================
//	Application-level global variables
//==================================================================================

//	Don't touch. These are defined in the front-end source code
extern int	gMainWindow;


//	Don't rename any of these variables/constants
//--------------------------------------------------
unsigned int numLiveFocusingThreads = 0;		//	the number of live focusing threads

//	An array of C-string where you can store things you want displayed in the spate pane
//	that you want the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
//	I preallocate the max number of messages at the max message
//	length.  This goes against some of my own principles about
//	good programming practice, but I do that so that you can
//	change the number of messages and their content "on the fly,"
//	at any point during the execution of your program, whithout
//	having to worry about allocation and resizing.
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
int numMessages;
time_t launchTime;

//	This is the image that you should be writing into.  In this
//	handout, I simply read one of the input images into it.
//	You should not rename this variable unless you plan to mess
//	with the display code.
RasterImage* imageOut;

//	Random Generation stuff
random_device myRandDev;
//	If you get fancy and specialized, you may decide to go for a different engine,
//	for exemple
//		mt19937_64  Mersenne Twister 19937 generator (64 bit)
default_random_engine myEngine(myRandDev());
//	a distribution for generating random r/g/b values
uniform_int_distribution<unsigned char> colorChannelDist;
//	Two random distributions for row and column indices.  I will
//	only be able to initialize them after I have read the image
uniform_int_distribution<unsigned int> rowDist;
uniform_int_distribution<unsigned int> colDist;


//------------------------------------------------------------------
//	The variables defined here are for you to modify and add to
//------------------------------------------------------------------
#define IN_PATH		"./DataSets/Series02/"
#define OUT_PATH	"./Output/"

const string hardCodedOutPath = "./outputImage.tga";

typedef struct
{
    pthread_t threadID;
    unsigned int index;
    unsigned int startRow, endRow;
    vector<RasterImage*> images;
    RasterImage* imageOut;
} ThreadInfo;

//==================================================================================
//	These are the functions that tie the computation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

//	I can't see any reason why you may need/want to change this
//	function
void displayImage(GLfloat scaleX, GLfloat scaleY)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glPixelZoom(scaleX, scaleY);

	//--------------------------------------------------------
	//	stuff to replace or remove.
	//	Here, each time we render the image I assign a random
	//	color to a few random pixels
	//--------------------------------------------------------
	unsigned char** imgRaster2D = (unsigned char**)(imageOut->raster2D);

	for (int k=0; k<100; k++) {
		unsigned int i = rowDist(myEngine);
		unsigned int j = colDist(myEngine);

		//	get pointer to the pixel at row i, column j
		unsigned char* rgba = imgRaster2D[i] + 4*j;
		// random r, g, b
		rgba[0] = colorChannelDist(myEngine);
		rgba[1] = colorChannelDist(myEngine);
		rgba[2] = colorChannelDist(myEngine);
		//	keep alpha unchanged at 255
	}

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glDrawPixels(imageOut->width, imageOut->height,
				  GL_RGBA,
				  GL_UNSIGNED_BYTE,
				  imageOut->raster);

}


void displayState(void)
{
	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//--------------------------------------------------------
	//	stuff to replace or remove.
	//--------------------------------------------------------
	//	Here I hard-code a few messages that I want to see displayed in my state
	//	pane.  The number of live focusing threads will always get displayed
	//	(as long as you update the value stored in the.  No need to pass a message about it.
	time_t currentTime = time(NULL);
	numMessages = 3;
	sprintf(message[0], "System time: %ld", currentTime);
	sprintf(message[1], "Time since launch: %ld", currentTime-launchTime);
	sprintf(message[2], "I like cheese");
	
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//	You may have to synchronize this call if you run into
	//	problems, but really the OpenGL display is a hack for
	//	you to get a peek into what's happening.
	//---------------------------------------------------------
	drawState(numMessages, message);
}

void cleanupAndQuit(void)
{
	writeTGA(hardCodedOutPath.c_str(), imageOut);

	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		free(message[k]);
	free(message);

	// delete images [optional]
	
	exit(0);
}

//	This callback function is called when a keyboard event occurs
//	You can change things here if you want to have keyboard input
//
void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;
	
	switch (c)
	{
		//	'esc' to quit
		case 27:
			//	If you want to do some cleanup, here would be the time to do it.
			cleanupAndQuit();
			break;

		//	Feel free to add more keyboard input, but then please document that
		//	in the report.
		
		
		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function.
//------------------------------------------------------------------------
int main(int argc, char** argv)
{
	//	Now we can do application-level initialization
	initializeApplication(argc, argv);

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv, imageOut);
	

	//==============================================
	//	This is OpenGL/glut magic.  Don't touch
	//==============================================
	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
		
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//	This is a part that you have to edit and add to, for example to
//	load a complete stack of images and initialize the output image
//	(right now it is initialized simply by reading an image into it.
//==================================================================================

void initializeApplication(int argc, char** argv)
{

	//	I preallocate the max number of messages at the max message
	//	length.  This goes against some of my own principles about
	//	good programming practice, but I do that so that you can
	//	change the number of messages and their content "on the fly,"
	//	at any point during the execution of your program, whithout
	//	having to worry about allocation and resizing.
	message = (char**) malloc(MAX_NUM_MESSAGES*sizeof(char*));
	for (int k=0; k<MAX_NUM_MESSAGES; k++) {
        message[k] = (char *) malloc((MAX_LENGTH_MESSAGE + 1) * sizeof(char));
    }
	
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I load an image to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator, although I usually rant on and on
	//	that the C/C++ default random generator is junk and that the people who use it
	//	are at best fools.  Here I am not using it to produce "serious" data (as in a
	//	simulation), only some color, in meant-to-be-thrown-away code
	
	//	seed the pseudo-random generator
	//srand((unsigned int) time(NULL));

	//	right now I read *one* hardcoded image, into my output
	//	image. This is definitely something that you will want to
	//	change.
	//const string hardCodedInput = "../TempData/_MG_6386.tga";
	//imageOut = readTGA(hardCodedInput.c_str());

    // Read the command line arguments
    unsigned int numThreads = stoi(argv[1]);
    std::string output_image = argv[2];

    cout << "Threads: " << numThreads << endl;
    cout << "Output image: " << output_image << endl;


    // create a vector of raster images
    vector<RasterImage*> images;
    for(int i = 3; i < argc; i++){
        images.push_back(readTGA(argv[i]));
    }

    // initialize output image
    imageOut = new RasterImage(images[0]->width, images[0]->height, RGBA32_RASTER);

    // print raster image vector
    for (unsigned int i = 0; i < images.size(); i++) {
        cout << "Image " << i << " width: " << images[i]->width << endl;
        cout << "Image " << i << " height: " << images[i]->height << endl;
    }

    // create array of ThreadInfo structs
    ThreadInfo* threadInfo = (ThreadInfo*) calloc(numThreads, sizeof(ThreadInfo));

    // create start and end index for threads
    unsigned int m = images[0]->height / numThreads;
    unsigned int p = images[0]->height % numThreads;
    int start = 0;
    int end = m - 1;

    for (unsigned int k =0; k < numThreads; k++) {
        if (k<p){
            end ++;
        }
        threadInfo[k].index = k;
        threadInfo[k].startRow = start;
        threadInfo[k].endRow = end;
        threadInfo[k].images = images;
        threadInfo[k].imageOut = imageOut;
        start = end + 1;
        end = start + m - 1;
    }

    // create threads
    for (unsigned int k = 0; k < numThreads; k++) {
        pthread_create(&threadInfo[k].threadID, NULL, threadFunc, threadInfo+k);
    }

    // wait for threads to finish
    for (unsigned int k = 0; k < numThreads; k++) {
        void* useless;
        pthread_join(threadInfo[k].threadID, &useless);
    }

    cout << "back" << endl;

    // free memory
    free(threadInfo);

    // write output image
    writeTGA(output_image.c_str(), imageOut);



//	//	Having read my image, I can now initialize my random distributions
//	rowDist = uniform_int_distribution<unsigned int>(0, imageOut->height-1);
//	colDist = uniform_int_distribution<unsigned int>(0, imageOut->width-1);

	launchTime = time(NULL);
}

void* threadFunc(void* argument){
    ThreadInfo* info = (ThreadInfo*) argument;
    unsigned char** rasterOut = (unsigned char**)(info->imageOut->raster2D);


    //loop through every pixel in the region
    for (unsigned int i = info->startRow; i <= info->endRow; i++) {
        for (unsigned int j = 0; j < info->imageOut->width; j++) {

            //check if pixel is out of bounds
            if (i < 2 || i >= info->imageOut->height - 2 || j < 2 || j >= info->imageOut->width - 2) {
                continue;
            }

            //create 5x5 window centered at current pixel
            unsigned char window[5][5];

            unsigned char contrast_score = 0;
            unsigned char image_index = 0;

            // loop through every image
            for(long unsigned int k = 0; k < info->images.size(); k++){
                // fill the window with the current pixel
                fill_window(window, info->images[k], i, j);


                unsigned char min_gray = 0;
                unsigned char max_gray = 0;
                unsigned char** rasterIn = (unsigned char**)(info->images[k]->raster2D);
                //loop through every pixel in the window
                for (int l = 0; l < 5; l++) {
                    for (int m = 0; m < 5; m++) {

                        unsigned char* rgba = rasterIn[l] + 4*m;
                        // get the gray value of the pixel
                        unsigned char gray = (unsigned char) (rgba[0] + rgba[1] + rgba[2]);

                        //keep track of the min and max gray values
                        if (gray < min_gray) {
                            min_gray = gray;
                        }
                        if (gray > max_gray) {
                            max_gray = gray;
                        }
                    }
                }
                // calculate the contrast score
                unsigned char contrast = max_gray - min_gray;
                if (contrast > contrast_score) {
                    contrast_score = contrast;
                    image_index = k;
                }

                //clear window
                for (int l = 0; l < 5; l++) {
                    for (int m = 0; m < 5; m++) {
                        window[l][m] = 0;
                    }
                }
            }
            unsigned char** rasterIn = (unsigned char**)(info->images[image_index]->raster2D);
            // set the pixel to the pixel in the image with the highest contrast score
            rasterOut[i][j] = rasterIn[i][j];


        }

    }

    cout << "Finished" << std::endl;
    //
    return NULL;

}

void fill_window(unsigned char window[5][5], RasterImage* image, int row, int col){
    int start_row = 0;
    int start_col = 0;
    for (int k = row - 2; k <= row + 2; k++) {
        for (int l = col - 2; l <= col + 2; l++) {
            // check if pixel is out of bounds
            if (k < 0 || k >= image->height || l < 0 || l >= image->width) {
                window[start_row][start_col] = 0;
                start_col += 1;
                continue;
            }
            unsigned char** raster2D = (unsigned char**) (image->raster2D);
            window[start_row][start_col] = raster2D[k][l];
            start_col += 1;
        }
        start_col = 0;
        start_row += 1;
    }

    return;
}


