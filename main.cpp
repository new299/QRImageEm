#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "qr_encodeem.h"
#include <iostream>

using namespace std;

int main() {
  char   *inputdata = "aaaaaaaaTESTaaaa";

  int outputdata_len=16;
  uint8_t outputdata[4096];

  int width=25;
  bool ok = qr_encode_data(0,0,0,-1,(uint8_t *) inputdata,16,outputdata,&outputdata_len,&width);

  if(ok == false) printf("Encoding error\n");
  qr_dumpimage(outputdata,width);
}
