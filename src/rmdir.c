/*
	Authors:	Matthew Dean, John Ryan
	
	Description:	This program is an implementation of the FAT12 file system, optimized for
			execution on 64-bit Ubuntu 16.02 LTS

	Certification of Authenticity:
	As curators of this code, we certify that all code presented is our original intellectual property
	or has been cited appropriately. 

	Reservation of Intellectual Property Rights:
	As curators of this code, we reserve all rights to this code (where not otherwise cited) as
	intellectual property and do not release it for unreferenced redistribution. However, we do
	allow and encourage inspiration to be drawn from this code and welcome use of our code with
	proper citation.
*/
#include <stdio.h>
#include <stdlib.h>
#include "sharedMemory.h"
#include "utilities.h"

FILE* FILE_SYSTEM_ID;

void resetFatEntry(int flc, char *fat);

int main(int argc, char **argv)
{
	if(argc > 2)
	{
		printf("ERROR: Too many arguments! Usage: rmdir {path}\n");
		exit(1);
	}
	if(argc == 1)
	{
		printf("ERROR: Too few argume nts! Usage: rmdir {path}\n");
		exit(1);
	}

	//Inspired by https://beej.us/guide/bgipc/output/html/multipage/shm.html
 	int shmId = shmget((key_t) 8675308, sizeof(char), 0666 | IPC_CREAT);
 
 	SharedMemory *sharedMemory = (SharedMemory *)shmat(shmId, (void *) 0, 0);
 
	FILE_SYSTEM_ID = fopen(sharedMemory->floppyImageName, "r+");

	if (FILE_SYSTEM_ID == NULL)
   	{
      		printf("ERROR: Could not open the floppy drive or image.\n");
      		exit(1);
   	}

   	int flc = searchForFileDirectory(argv[1], sharedMemory->firstLogicalCluster);

	if(flc == -2)
	{
		printf("ERROR: Specified path leads to a directory, not a file.\n");
		exit(1);
	}

	if(flc < 0)
	{
		printf("ERROR: File or directory not found.\n");
		exit(1);
	}

	unsigned char *fat = readFAT12Table(1);
   
   //last name in path should be the file name
	char **directoryComponents = malloc(MAX_INPUT_LENGTH * sizeof(char));
   	int depth = split(argv[1], &directoryComponents, "/\n");
	char *targetName = directoryComponents[depth - 1];

   int nextCluster;
   // Loop through all clusters...
   do
   {
      nextCluster = get_fat_entry(flc, fat);
      unsigned char *clusterBytes = malloc(BYTES_PER_SECTOR * sizeof(char));

      int realCluster;
      if(flc == 0)
      {
         realCluster = 19;
      }
      else
      {
         realCluster = 31 + flc;
      }

      int numBytes = read_sector(realCluster, clusterBytes);


      int j;
      // loop through each entry in cluster...
      for(j = 0; j < 16; j++)
      {
         FileStructure file;
         int entryOffset = j * 32;

         file = getFileAtEntry(clusterBytes + entryOffset);

         if(fileMatchesTarget(file, targetName) == 0)
         {
            

            // if it isnt a directory (aka a file), exit
            if((file.attributes & 0x10) != 0x10)
            {
            	printf("ERROR: Path refers to a file, not a directory.\n");
               exit(1);
            }

            int count = countEntriesInFlc(file.flc);
            if(count > 2)
            {
               printf("ERROR: Directory is not empty. Cannot delete.\n");
               exit(1);
            }

            clusterBytes[entryOffset] = 0xe5;
            write_sector(realCluster, clusterBytes);

            char *tmp = malloc(BYTES_PER_SECTOR * sizeof(char));
            tmp = readFAT12Table(1);
            
            resetFatEntry(file.flc, tmp);
            saveFAT12Table(1, tmp);
            
            return 0;
         }

      }

      if(nextCluster > 0x00 && nextCluster < 0xFF0)
      {
         flc = nextCluster;
      }
   } while (nextCluster > 0x00 && nextCluster < 0xFF0);

   	printf("Unable to locate directory.\n");
	exit(1);
}

void resetFatEntry(int flc, char *fat)
{
   int entry = get_fat_entry(flc, fat);
   if(entry > 0 && entry < 0xFF0)
   {
      resetFatEntry(entry, fat);
   }

   set_fat_entry(flc, 0x00, fat);
}