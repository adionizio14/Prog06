//
//  main.c
// CSC412 - Fall 2023 - Prog 06
//
//  Created by Jean-Yves Hervé on 2017-05-01, modified 2023-11-12
//

#include <iostream>
#include <string>
#include <random>
#include <pthread.h>
#include <unistd.h>
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
void* threadFunc(void* argument);
int computeContrast(RasterImage* image, int row, int col);
void fill_imageOut(RasterImage* imageOut);
void combine_images(int** rasterOut, RasterImage* image, int row, int col);


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

unsigned int numThreads;
std::string output_image;
std::vector<RasterImage*> images;
unsigned int i = 0;

struct ThreadInfo
{
    pthread_t threadID;
    unsigned int index;
    unsigned int startRow, endRow;
    vector<RasterImage*> images;
    RasterImage* imageOut;
    uniform_int_distribution<unsigned int> rowDist;
    uniform_int_distribution<unsigned int> colDist;
    pthread_mutex_t lock;
};

std::vector<ThreadInfo> threadInfo;
pthread_mutex_t myWriteLock;
bool shouldTerminate = false;

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


    // each time the image is rendered, a new thread is created until the max number of threads is reached
    if (i < numThreads) {
        int err = pthread_create(&threadInfo[i].threadID, nullptr, threadFunc, &threadInfo[i]);
        if (err != 0) {
            cout << "Could not create Thread " << i << ". [" << err << "]: " <<
                 strerror(err) << endl << flush;
            exit(1);
        }
        i++;
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

    //	Free allocated resource before leaving (not absolutely needed, but
    //	just nicer.  Also, if you crash there, you know something is wrong
    //	in your code.
    for (int k=0; k<MAX_NUM_MESSAGES; k++)
        free(message[k]);
    free(message);

    // delete images [optional]
    images.clear();

    // free threadInfo
    threadInfo.clear();

    // join threads
    for(int i = 0; i < numThreads; i++){
        pthread_join(threadInfo[i].threadID, nullptr);
    }

    // destroy lock
    pthread_mutex_destroy(&myWriteLock);

    // destroy thread locks
    for(int i = 0; i < numThreads; i++){
        pthread_mutex_destroy(&(threadInfo[i].lock));
    }

    // write output image
    writeTGA(output_image.c_str(), imageOut);

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
            shouldTerminate = true;
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
    srand((unsigned int) time(NULL));
    // Read the command line arguments
    numThreads = stoi(argv[1]);
    output_image = argv[2];
    numLiveFocusingThreads = numThreads;

    // create a vector of raster images
    vector<RasterImage*> images;
    for(int i = 3; i < argc; i++){
        images.push_back(readTGA(argv[i]));
    }

    // initialize output image
    imageOut = new RasterImage(images[0]->width, images[0]->height, RGBA32_RASTER);

    // initialize image out raster with pure black pixels
    fill_imageOut(imageOut);

    //	Having read my image, I can now initialize my random distributions
    rowDist = uniform_int_distribution<unsigned int>(0, imageOut->height-1);
    colDist = uniform_int_distribution<unsigned int>(0, imageOut->width-1);

    // initialize mutex
    pthread_mutex_init(&myWriteLock, nullptr);

    // create array of ThreadInfo structs
    threadInfo.resize(numThreads);

    // create start and end index for threads
    unsigned int m = images[0]->height / numThreads;
    unsigned int p = images[0]->height % numThreads;
    int start = 0;
    int end = m - 1;

    // fill in threadInfo structs
    for(unsigned int i = 0; i < numThreads; i++){
        if (i<p){
            end ++;
        }
        threadInfo[i].index = i;
        threadInfo[i].images = images;
        threadInfo[i].imageOut = imageOut;
        threadInfo[i].rowDist = rowDist;
        threadInfo[i].colDist = colDist;
        threadInfo[i].startRow = start;
        threadInfo[i].endRow = end;
        start = end + 1;
        end = end + m;

        pthread_mutex_init(&(threadInfo[i].lock), nullptr);

    }

    launchTime = time(NULL);
}

void* threadFunc(void* argument){

    // get thread info
    ThreadInfo* info = (ThreadInfo*) argument;

    int** rasterOut = (int**)(info->imageOut->raster2D);

    //create vector for each pixel

    int count = 0;

    do {

        // lock the region mutex
        pthread_mutex_lock(&(info->lock));

        // get a random pixel
        unsigned int i = info->rowDist(myEngine);
        unsigned int j = info->colDist(myEngine);


        unsigned char contrast_score = 0;
        unsigned char image_index = 0;

        // loop through every image
        for(long unsigned int k = 0; k < info->images.size(); k++){

            // get the contrast of each window
            int contrast = computeContrast(info->images[k], i, j);

            // keep track of the image with the highest contrast score
            if (contrast > contrast_score) {
                contrast_score = contrast;
                image_index = k;
            }
        }

        // get the raster of the image with the highest contrast score
        combine_images(rasterOut, info->images[image_index], i, j);

        // check to see if the user pressed the escape key
        if (shouldTerminate) {
            break;
        }

        // unlock the region mutex
        pthread_mutex_unlock(&(info->lock));

        //check to see if escape key was pressed
    }
    while(true);

    return nullptr;
}

int computeContrast(RasterImage* image, int row, int col){

    // get the raster of the image
    int** rasterIn = (int**)(image->raster2D);

    // get the start and end row and column for an 11x11 window
    int start_row = max(0, row-5), end_row=min((int) image->height-1, (int) row+5);
    int start_col = max(0, col-5), end_col=min((int) image->width-1, (int)col+5);

    // initialize min and max gray values
    int min_gray = 255;
    int max_gray = 0;

    // loop through the 11x11 window
    for (int k = start_row; k <= end_row; k++) {
        for (int l = start_col; l <= end_col; l++) {

            // get the rgba values of the pixel
            unsigned char* rgba = (unsigned char*)(rasterIn[k] + l);

            // get the gray value of the pixel
            int gray = (int) ((0 + rgba[0] + rgba[1] + rgba[2])/3);

            //keep track of the min and max gray values
            if (gray < min_gray) {
                min_gray = gray;
            }
            if (gray > max_gray) {
                max_gray = gray;
            }
        }

    }

    return max_gray - min_gray;
}

void fill_imageOut(RasterImage* imageOut){

    int** rasterOut = (int**)(imageOut->raster2D);

    for (int i = 0; i < imageOut->height; i++) {
        for (int j = 0; j < imageOut->width; j++) {
            unsigned char* rgba = (unsigned char*)(rasterOut[i] + j);
            rgba[0] = 0;
            rgba[1] = 0;
            rgba[2] = 0;
            rgba[3] = 255;
        }
    }
}

void combine_images(int** rasterOut, RasterImage* image, int row, int col){

    // lock the mutex
    pthread_mutex_lock(&myWriteLock);

    int start_row = max(0, row-5), end_row=min((int) image->height-1, (int) row+5);
    int start_col = max(0, col-5), end_col=min((int) image->width-1, (int)col+5);

    int** rasterIn = (int**)(image->raster2D);

    for (int k = start_row; k <= end_row; k++) {
        for (int l = start_col; l <= end_col; l++) {

            unsigned char* rgba_out = (unsigned char*)(rasterOut[k] + l);
            int red = (int) rgba_out[0];
            int green = (int) rgba_out[1];
            int blue = (int) rgba_out[2];
            int alpha = (int) rgba_out[3];

            unsigned char* rgba_in = (unsigned char*)(rasterIn[k] + l);

            if (red == 0 && green == 0 && blue == 0 && alpha == 255) {
                rgba_out[0] = rgba_in[0];
                rgba_out[1] = rgba_in[1];
                rgba_out[2] = rgba_in[2];
            }
            else{
                rgba_out[0] = (unsigned char) (0.5 * (int)rgba_in[0] + 0.5 * (int)rgba_out[0]);
                rgba_out[1] = (unsigned char) (0.5 * (int)rgba_in[1] + 0.5 * (int)rgba_out[1]);
                rgba_out[2] = (unsigned char) (0.5 * (int)rgba_in[2] + 0.5 * (int)rgba_out[2]);
            }
        }
    }

    // unlock the mutex
    pthread_mutex_unlock(&myWriteLock);

}

bool checkMultipleRegions(int row, int endRow, int startRow){

    int windowSize = 11;
    int windowHalfSize = windowSize / 2;

    // Calculate the bounds of the 11x11 window
    int windowTop = row - windowHalfSize;
    int windowBottom = row + windowHalfSize;

    bool overlap = false;

    // Check if the top or bottom of the window is in another region
    if (windowTop < startRow || windowBottom > endRow) {
        overlap = true;
    }

    return overlap;

}
