/*
 ******************************************************************************
 *
 *   This File Contains PROPRIETARY and CONFIDENTIAL TRADE SECRET information
 *   which is the exclusive property of VeriSolutions, LLC.  "Trade Secret"
 *   means information constituting a trade secret within the meaning of
 *   Section 10-1-761(4) of the Georgia Trade Secrets Act of 1990, including
 *   all amendments hereafter adopted.
 *
 *   (c) 2017 by VeriSolutions, LLC.                 www.VeriSolutions.co
 *
 ******************************************************************************
 *
 *      Contiki Image Reduction Program -
 *          This program reduces the 128kb image output of the linker into
 *          a smaller file by eliminating all the FF's that pad out the
 *          image producted by the linker.
 *
 *          What is produced is a ".img" file that is just the ".bin" file
 *          split into two segments.  The first segment is the so-called
 *          CCFG paramters and the second is the executable image, both of
 *          which are loaded into Contiki flash memory.
 *
 *          Each of the two segments are prepended by a short 8-byte header 
 *          consisting of first, a 4-byte offset from the begining of flash
 *          followed by a 4-byte length of the segment.  These headers are
 *          followed by exactly that length of bytes of data to be burned
 *          into flash.
 *
 ******************************************************************************
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main (int argc, char *argv[]) {

    FILE        *fIn, *fOut;
    uint8_t     *imageBuf, *bPtr;
    int         totalInBytes;
    int         totalOutBytes;
    const int   expectedImageSize = 128*1024;
    const int   sizeOfCCFG = 88;
    uint8_t     ccfgBuf[sizeOfCCFG];
    char        imgFileName[256];
    char        *c;

    if (argc != 2) {
        printf("usage: %s input-file-name\n", argv[0]);
        exit(1);
    }

    if ((fIn = fopen(argv[1], "r")) == NULL) {
        printf("File \"%s\" not found\n", argv[1]);
        exit(1);
    }

    strcpy(imgFileName, argv[1]);
    if (strcmp(c=strrchr(imgFileName, '.'), ".bin")) {
        printf("Illegal file name \"%s\"\n", argv[1]);
        exit(1);
    }
    strcpy(c, ".img");

    if (!(imageBuf = (uint8_t*)malloc(expectedImageSize*2))) {
        printf("Cannot allocate buffer space\n");
        exit(1);
    }

    // This program only made to handle Contiki 128Kb images
    bPtr = imageBuf;
    while (fread(bPtr, sizeof(uint8_t), 1, fIn) == 1) {
        bPtr++;
        if ((bPtr-imageBuf) > (expectedImageSize*2)) {
            printf("Image too large. Exiting...\n");
            exit(1);
        }
    }

    // This program only made to handle Contiki 128Kb images
    totalInBytes = bPtr - imageBuf;
    printf("\"%s\" contains image of %dK bytes\n", argv[1], 
            totalInBytes/1024);
    if (totalInBytes != expectedImageSize) {
        printf("Image is not of expected size. Exiting...\n");
        exit(1);
    }

    if ((fOut = fopen(imgFileName, "w")) == NULL) {
        printf("Cannot open \"%s\"\n", imgFileName);
        exit(1);
    }

    // Copy out the 88 bytes of CCFG from the end of the image
    for (int i=0; i<sizeOfCCFG; i++) {
        ccfgBuf[i] = imageBuf[expectedImageSize-sizeOfCCFG+i];
    }

    // Write out the 88 bytes of the CCFG image portion
    uint32_t    value;
    value = expectedImageSize - sizeOfCCFG;     // offset
    fwrite(&value, sizeof(value), 1, fOut);
    value = sizeOfCCFG;                         // size
    fwrite(&value, sizeof(value), 1, fOut);
    for (int i=0; i<sizeOfCCFG; i++) {
        fwrite(ccfgBuf+i, sizeof(ccfgBuf[0]), 1, fOut);
    }
    totalOutBytes = sizeOfCCFG+(2*(sizeof(value)));

    // Look for where image ends
    int32_t *ptr = (int32_t*)(imageBuf+expectedImageSize-sizeOfCCFG)-1;
    while (*ptr == -1) {
        ptr--;
    }

    // Write out the valid image contents
    value = 0x00000000;
    fwrite(&value, sizeof(value), 1, fOut);         // offset
    uint32_t imageLen = (uint64_t)(ptr+1) - (uint64_t)imageBuf;
    fwrite(&imageLen, sizeof(imageLen), 1, fOut);   // size
    totalOutBytes += sizeof(value) + sizeof(imageLen);
    for (int i=0; i<imageLen; i++) {
        fwrite(imageBuf+i, sizeof(imageBuf[0]), 1, fOut);
    }
    totalOutBytes += imageLen;

    fclose(fOut);
    printf("\"%s\" contains reduced image of %d bytes\n", imgFileName, 
           totalOutBytes); 
    exit(0);
}
