#include "QR_Encode.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  if (argc<3) {
    printf("qrencode <input file> <output file> [QR Code level] [QR Code version]\n");
    printf("level and version default to 3 and 0\n");
    return 1;
  }

  int level=3;
  if(argc>3) {
    level = atoi(argv[3]);
  }

  int version=0;
  if(argc>4) {
    version = atoi(argv[4]);
  }

  // Read in the input data from file, terminate with 0.
  FILE *inputfile = fopen(argv[1],"r");
  uint8_t inputdata[10000];
  int n;
  for(n=0;(!feof(inputfile)) && (n < 10000);n++) {
    int c = getc(inputfile);
    inputdata[n] = c;
    inputdata[n+1]=0;
  }

  // **** This calls the library and encodes the data
  // *** length is taken from NULL termination, however can also be passed by parameter.


  uint8_t data[3917]; // maximum number of bytes is 3917

  CQR_Encode encoder;
  bool ok = encoder.EncodeData(level,version,0,-1,inputdata,0);
  if(ok == false) {
    printf("Encoding failed\n");
    return 1;
  }


  int QR_width = encoder.m_nSymbleSize;

  // Write the data to the output file
  //FILE *outputfile=fopen(argv[2],"w");
  //int size = ((QR_width*QR_width)/8)+1;
  //fwrite(QR_m_data,size,1,outputfile);
  //fclose(outputfile);


  // This code dumps the QR code to the screen as ASCII.
  printf("QR Code width: %u\n",QR_width);

  
  for(int y=0;y<QR_width;y++) {
    for(int x=0;x<QR_width;x++) {
      if (encoder.m_byModuleData[x][y]) { printf("1"); }
                                   else { printf("0"); }
    }
    printf("\n");
  }

/* for binary representation 
  int bit_count=0;
  for(n=0;n<size;n++) {
    int b=0;
    for(b=7;b>=0;b--) {
      if((bit_count%QR_width) == 0) printf("\n");
      if((QR_m_data[n] & (1 << b)) != 0) { printf("1"); }
                                    else { printf("0"); }
      bit_count++;
    }
  }
*/

  return 0;
}
