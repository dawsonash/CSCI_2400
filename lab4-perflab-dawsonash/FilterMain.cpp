#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();

    //created variables to store frequently needed data in the cache
  int width = (input -> width) - 1;
  int height = (input -> height) - 1;
    
  output -> width = input -> width;
  output -> height= input -> height;

    //variable that will be used in nested loop
  int divisorConstant = filter -> getDivisor();
    
  
  //instead of finding this function in memory each time I need to call it, save the values to an array to use later.
  int filterLocal[3][3];
  for(int i = 0; i < 3; i++){
    for(int j = 0; j < 3; j++){
      filterLocal[i][j] = filter -> get(i,j);
    }
  }
    // removed [plane] dimension 
    //flipped for loop because color[row][col] because the cache will have faster access to the succeeding [col] elements
    for(int row = 1; row <  height; row = row + 1) {
  for(int col = 1; col < width; col = col + 1) {

	int temp = 0;
  int temp1 = 0;
  int temp2 = 0;

      //reduced total operations in the unrolled loop
    int rowDecOne = row -1;
    int rowIncOne = row + 1; 
    int colDecOne = col - 1;
    int colIncOne = col + 1;
    
        //instruction level parralelsim unrolling also lets the CPU do more work concurrently 
        //unrolled double loop - less overhead because no control flow needed
      temp +=  (input -> color[0][rowDecOne][colDecOne] * filterLocal[0][0] );
      temp1 +=  (input -> color[1][rowDecOne][colDecOne] * filterLocal[0][0] );
      temp2 +=  (input -> color[2][rowDecOne][colDecOne] * filterLocal[0][0] );
      
      temp +=  (input -> color[0][rowDecOne][col] * filterLocal[0][1] );
      temp1 +=  (input -> color[1][rowDecOne][col] * filterLocal[0][1] );
      temp2 +=  (input -> color[2][rowDecOne][col] * filterLocal[0][1] );
      
      temp +=  (input -> color[0][rowDecOne][colIncOne] * filterLocal[0][2] );
      temp1 +=  (input -> color[1][rowDecOne][colIncOne] * filterLocal[0][2] );
      temp2 +=  (input -> color[2][rowDecOne][colIncOne] * filterLocal[0][2] );
      
      temp +=  (input -> color[0][row][colDecOne] * filterLocal[1][0] );
      temp1 +=  (input -> color[1][row][colDecOne] * filterLocal[1][0] );
      temp2 +=  (input -> color[2][row][colDecOne] * filterLocal[1][0] );
      
      temp +=  (input -> color[0][row][col] * filterLocal[1][1] );
      temp1 +=  (input -> color[1][row][col] * filterLocal[1][1] );
      temp2 +=  (input -> color[2][row][col] * filterLocal[1][1] );
      
      temp +=  (input -> color[0][row][colIncOne] * filterLocal[1][2] );
      temp1 +=  (input -> color[1][row][colIncOne] * filterLocal[1][2] );
      temp2 +=  (input -> color[2][row][colIncOne] * filterLocal[1][2] );

      temp +=  (input -> color[0][rowIncOne][col - 1] * filterLocal[2][0] );
      temp1 +=  (input -> color[1][rowIncOne][col - 1] * filterLocal[2][0] );
      temp2 +=  (input -> color[2][rowIncOne][col - 1] * filterLocal[2][0] );
      
      temp +=  (input -> color[0][rowIncOne][col] * filterLocal[2][1] );
      temp1 +=  (input -> color[1][rowIncOne][col] * filterLocal[2][1] );
      temp2 +=  (input -> color[2][rowIncOne][col] * filterLocal[2][1] );
      
      temp +=  (input -> color[0][rowIncOne][colIncOne] * filterLocal[2][2] );
      temp1 +=  (input -> color[1][rowIncOne][colIncOne] * filterLocal[2][2] );
      temp2 +=  (input -> color[2][rowIncOne][colIncOne] * filterLocal[2][2] );


	temp /= divisorConstant;
  temp1 /= divisorConstant;
  temp2 /= divisorConstant;

  temp = max(0,temp);
  temp = min(255,temp);

  temp1 = max(0,temp1);
  temp1 = min(255,temp1);

  temp2 = max(0,temp2);
  temp2 = min(255,temp2);

  output -> color[0][row][col] = temp;
  output -> color[1][row][col] = temp1;
  output -> color[2][row][col] = temp2;


      }
    }
  
    int h = output -> height;
    int w = output -> width;
    
  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (w * h);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (w * h));
  return diffPerPixel;
}
