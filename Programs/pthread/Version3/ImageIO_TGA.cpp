/*----------------------------------------------------------------------------------+
|	This is a modified version of the so-called "Lighthouse Library" for reading	|
|	images encoded in the *uncompressed, uncommented .tga (TARGA file) format. 		|
|	I had been using and modifying this code for years, simply mentioning			|
|	"Source Unknown" in my comments when I finally discovered, thanks to Web		|
|	searches, the origin of this code.  By then it had been adapted to work along	|
|	with reader/writer code for another image file format: the awful PPM/PBM/PGM	|
|	format of obvious Unix origin.													|
|	This is just to say that I am not claiming authorship of this code.  I am only	|
|	distributing it in this form instead of the original because (1) I have long	|
|	lost the original code, and (2) This version works with images organized		|
|	nicely into a struct.															|
|																					|
|	Jean-Yves Hervé		Dept. of Computer Science and Statistics, URI				|
|						2018-09-26													|
+----------------------------------------------------------------------------------*/

#include <cstdlib>        
#include <cstdio>

#include "ImageIO_TGA.h"

void swapRGB_(unsigned char* theData, unsigned int height, unsigned int width);
void swapRGBA_(unsigned char* theData, unsigned int height, unsigned int width);


//----------------------------------------------------------------------
//	Utility function for memory swapping
//	Used because TGA stores the RGB data in reverse order (BGR)
//----------------------------------------------------------------------
void swapRGB_(unsigned char* theData, unsigned int height, unsigned int width)
{
    unsigned int numBytes = height*width;

	for(unsigned int k = 0; k < numBytes; k++)
	{
		unsigned char tmp = theData[k*3+2];
		theData[k*3+2] = theData[k*3];
		theData[k*3] = tmp;
	}
}

void swapRGBA_(unsigned char* theData, unsigned int height, unsigned int width)
{
    unsigned int numBytes = height*width;
    
    for(unsigned int k=0; k<numBytes; k++){
        unsigned char temp = theData[k*4+2];
        theData[k*4+2] = theData[k*4];
        theData[k*4] = temp;
    }
}

// ---------------------------------------------------------------------
//	Function : readTGA 
//	Description :
//	
//	This function reads an image of type TGA (8 or 24 bits, uncompressed
//	
//----------------------------------------------------------------------

RasterImage* readTGA(const char* filePath)
{
	//--------------------------------
	//	open TARGA input file
	//--------------------------------
	FILE* tga_in = fopen(filePath, "rb" );
	if (tga_in == nullptr)
	{
		printf("Cannot open image file %s\n", filePath);
		exit(11);
	}

	//--------------------------------
	//	Read the header (TARGA file)
	//--------------------------------
	char	head[18] ;
	fread( head, sizeof(char), 18, tga_in ) ;
	/* Get the size of the image */
	unsigned int imgWidth = ((unsigned int)head[12]&0xFF) | (unsigned int)head[13]*256;
	unsigned int imgHeight = ((unsigned int)head[14]&0xFF) | (unsigned int)head[15]*256;
	unsigned int numBytes = imgWidth * imgHeight;

	ImageType imgType;
	//unsigned short maxVal;
	//unsigned int bytesPerPixel;
	//unsigned int bytesPerRow;
	if((head[2] == 2) && (head[16] == 24))
	{
		imgType = RGBA32_RASTER;
		//maxVal = 255;
		//bytesPerPixel = 4;
		//bytesPerRow = 4*imgWidth;
	}
	else if((head[2] == 3) && (head[16] == 8))
	{
		imgType = GRAY_RASTER;
		//maxVal = 255;
		//bytesPerPixel = 1;
		//bytesPerRow = imgWidth;
	}
	else
	{
		printf("Unsuported TGA image: ");
		printf("Its type is %d and it has %d bits per pixel.\n", head[2], head[16]);
		printf("The image must be uncompressed while having 8 or 24 bits per pixel.\n");
		fclose(tga_in);
		exit(12);
	}

	RasterImage* image = new RasterImage(imgWidth, imgHeight, imgType);
	unsigned char* data = (unsigned char*) image->raster;
	
	//--------------------------------
	//	Read the pixel data
	//--------------------------------

	//	Case of a color image
	//------------------------	
	//if (image.type == kTGA_COLOR) *****************************
	if(image->type == RGBA32_RASTER)
	{
		//	First check if the image is mirrored vertically (a bit setting in the header)
		if(head[17]&0x20)
		{
			unsigned char* ptr = data + numBytes*4 - image->bytesPerRow;
			for(unsigned int i = 0; i < image->height; i++)
			{
                for (unsigned int j=0; j<image->width; j++)
                {
                    fread(ptr, 3*sizeof(char), 1, tga_in);
					ptr -= 4;
                }
			}
		}
		else
        {
            unsigned char* dest = data;
            for (unsigned int i=0; i<image->height; i++)
            {
                for (unsigned int j=0; j<image->width; j++)
                {
                    fread(dest, 3*sizeof(char), 1, tga_in);
                    dest+=4;
                }
            }
			
        }
        
        //  tga files store color information in the order B-G-R
        //  we need to swap the Red and Blue components
    	swapRGBA_(data, image->height, image->width);
	}

	//	Case of a gray-level image
	//----------------------------	
	else
	{
		//	First check if the image is mirrored vertically (a bit setting in the header)
		if(head[17]&0x20)
		{
			unsigned char* ptr = data + numBytes - image->width;
			for(unsigned int i = 0; i < image->height; i++)
			{
				fread( ptr, sizeof(char), image->width, tga_in ) ;
				ptr -= image->width;
			}
		}
		else
			fread(data, sizeof(char), numBytes, tga_in);
	}

	fclose(tga_in) ;
	return image;
}	


//---------------------------------------------------------------------*
//	Function : writeTGA 
//	Description :
//	
//	 This function write out an image of type TGA (24-bit color)
//	
//	Return value: Error code (0 = no error)
//----------------------------------------------------------------------*/ 
ImageIOErrorCode writeTGA(const char* filePath, const RasterImage* image)
{
	//--------------------------------
	// open TARGA output file 
	//--------------------------------
	FILE* tga_out = fopen(filePath, "wb" );
	if (tga_out == NULL)
	{
		printf("Cannot create image file %s \n", filePath);
		return kCannotOpenWrite;
	}

	//	Yes, I know that I tell you over and over that cascading if-else tests
	//	are bad style when testing an integral value, but here only two values
	//	are supported.  If I ever add one more I'll use a switch, I promise.
	
	//------------------------------
	//	Case of a color image
	//------------------------------
	if (image->type == RGBA32_RASTER)
	{
		//--------------------------------
		// create the header (TARGA file)
		//--------------------------------
		char head[18] ;
		head[0]  = 0 ;		  					// ID field length.
		head[1]  = 0 ;		  					// Color map type.
		head[2]  = 2 ;		  					// Image type: true color, uncompressed.
		head[3]  = head[4] = 0 ;  				// First color map entry.
		head[5]  = head[6] = 0 ;  				// Color map lenght.
		head[7]  = 0 ;		  					// Color map entry size.
		head[8]  = head[9] = 0 ;  				// Image X origin.
		head[10] = head[11] = 0 ; 				// Image Y origin.
		head[13] = (char) (image->width >> 8) ;		// Image width.
		head[12] = (char) (image->width & 0x0FF) ;
		head[15] = (char) (image->height >> 8) ;		// Image height.
		head[14] = (char) (image->height & 0x0FF) ;
		head[16] = 24 ;		 					// Bits per pixel.
		head[17] = 0 ;		  					// Image descriptor bits ;
		fwrite(head, sizeof(char), 18, tga_out );

		unsigned char* data  = (unsigned char*) image->raster;
		for(unsigned int i = 0; i < image->height; i++)
		{
			unsigned long offset = i*4*image->width;
			for(unsigned int j = 0; j < image->width; j++)
			{
				fwrite(&data[offset+2], sizeof(char), 1, tga_out);
				fwrite(&data[offset+1], sizeof(char), 1, tga_out);
				fwrite(&data[offset], sizeof(char), 1, tga_out);
				offset+=4;
			}
		}

		fclose( tga_out ) ;
	}
	//------------------------------
	//	Case of a gray-level image
	//------------------------------
	else if (image->type == GRAY_RASTER)
	{
		//--------------------------------
		// create the header (TARGA file)
		//--------------------------------
		char	head[18] ;
		head[0]  = 0 ;		  					// ID field length.
		head[1]  = 0 ;		  					// Color map type.
		head[2]  = 3 ;		  					// Image type: gray-level, uncompressed.
		head[3]  = head[4] = 0 ;  				// First color map entry.
		head[5]  = head[6] = 0 ;  				// Color map lenght.
		head[7]  = 0 ;		  					// Color map entry size.
		head[8]  = head[9] = 0 ;  				// Image X origin.
		head[10] = head[11] = 0 ; 				// Image Y origin.
		head[13] = (char) (image->width >> 8) ;		// Image width.
		head[12] = (char) (image->width & 0x0FF) ;
		head[15] = (char) (image->height >> 8) ;		// Image height.
		head[14] = (char) (image->height & 0x0FF) ;
		head[16] = 8 ;		 					// Bits per pixel.
		head[17] = 0 ;		  					// Image descriptor bits ;
		fwrite( head, sizeof(char), 18, tga_out );

		unsigned char* data  = (unsigned char*) image->raster;
		for(unsigned int i = 0; i < image->height; i++)
		{
			fwrite(&data[i*image->width], sizeof(char), image->width, tga_out);
		}

		fclose( tga_out ) ;
	}
	else
	{
		printf("Image type not supported for output in TGA format\n");
		return kWrongFileType;
	}
	


	return kNoIOerror;
}	

