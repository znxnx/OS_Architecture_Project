/*
Yeah! Come on, come on, come on, come on 
Now touch me, baby 
Can't you see that I am not afraid? 
What was that promise that you made? 
Why won't you tell me what she said? 
What was that promise that you made? 

Now, I'm gonna love you 
Till the heavens stop the rain 
I'm gonna love you 
Till the stars fall from the sky for you and I 

Come on, come on, come on, come on 
Now touch me, baby 
Can't you see that I am not afraid? 
What was that promise that you made? 
Why won't you tell me what she said? 
What was that promise that you made? 

I'm gonna love you 
Till the heavens stop the rain 
I'm gonna love you 
Till the stars fall from the sky for you and I 
I'm gonna love you 
Till the heavens stop the rain 
I'm gonna love you 
Till the stars fall from the sky for you and I 

Stronger than dirt
*/

#include <stdio.h>
#include <stdlib.h>
#include "sharedMemory.h"
#include "utilities.h"

FILE* FILE_SYSTEM_ID;

int main(int argc, char **argv)
{
	if(argc > 2)
	{
		printf("Too many arguments! Usage: rm {path}");
		exit(1);
	}
	if(argc == 1)
	{
		printf("Too few argume nts! Usage: rm {path}");
		exit(1);
	}

	//Inspired by https://beej.us/guide/bgipc/output/html/multipage/shm.html
 	int shmId = shmget((key_t) 8675308, sizeof(char), 0666 | IPC_CREAT);
 
 	SharedMemory *sharedMemory = (SharedMemory *)shmat(shmId, (void *) 0, 0);
 
	FILE_SYSTEM_ID = fopen(sharedMemory->floppyImageName, "r+");

	if (FILE_SYSTEM_ID == NULL)
   	{
      		printf("Could not open the floppy drive or image.\n");
      		exit(1);
   	}

   	int flc = searchForFileDirectory(argv[1], sharedMemory->firstLogicalCluster);

	if(flc == -2)
	{
		printf("Specified path leads to a directory, not a file.\n");
		exit(1);
	}

	if(flc < 0)
	{
		printf("File or directory not found.\n");
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

         //if first byte is 0xE5 or 0x00, it's a free space
         if(clusterBytes[entryOffset] == 0xe5 || clusterBytes[entryOffset] == 0x00)
         {
         	int cluster = findFreeCluster();

         	if(cluster < 0)
         	{
         		printf("The file system is full. Cannot touch.\n");
         	}

         	int i = 0;

         	//set the name
         		if(targetName[0] == '.')
         		{
         			printf("File name cannot start with '.'\n");
         			exit(1);
         		}
         		char **parts;
         		int partCount = split(targetName, &parts, ".\\");
         		
         		if(partCount == 2)
         		{
         			char *fileName = parts[0];
         			for(i = 0; i < 8 && fileName[i] != '\0'; i++)
		         	{
		         		clusterBytes[entryOffset + i] = fileName[i];
		         	}
		         	if(i < 7)
		         	{
		         		clusterBytes[entryOffset + i] = '\0';
		         	}
		         	char *extension = parts[1];
		         	for(i = 8; i < 11 || extension[i - 8] != '\0'; i++)
		         	{
		         		clusterBytes[entryOffset + i] = extension[i - 8];
		         	}
		         	if(i < 10)
		         	{
		         		clusterBytes[entryOffset + i] = '\0';
		         	}
         		}
         		else
         		{
         			for(i = 0; i < 8 && targetName[i] != '\0'; i++)
		         	{
		         		clusterBytes[entryOffset + i] = targetName[i];
		         	}
		         	if(i < 7)
		         	{
		         		clusterBytes[entryOffset + i] = '\0';
		         	}
		         	clusterBytes[entryOffset + 8] = '\0';
         		}

         		clusterBytes[entryOffset + 11] = 0x00;
         		clusterBytes[entryOffset + 26] = cluster;
         		clusterBytes[entryOffset + 28] = 0;
         		clusterBytes[entryOffset + 29] = 0;
         		clusterBytes[entryOffset + 30] = 0;
         		clusterBytes[entryOffset + 31] = 0;
         	

         	if(j != 15  && clusterBytes[entryOffset] == 0x00)
         	{
         		//if we replaced the last entry, we need to set the next entry to be the last entry
         		clusterBytes[entryOffset + 32] == 0x00;
         	}

         	write_sector(realCluster, clusterBytes);
         	return 0;
         }

      }

      if(nextCluster > 0x00 && nextCluster < 0xFF0)
      {
         flc = nextCluster;
      }
      else if(nextCluster == 0)
      {
      	int newCluster = findFreeCluster();

      	if(newCluster < 0)
      	{
      		printf("Could not expand directory--file system is full.\n");
      		exit(1);
      	}

      	set_fat_entry(flc, nextCluster, fat);
      	flc = nextCluster;
      }
   } while (nextCluster > 0x00 && nextCluster < 0xFF0);

   	//if we get here, we need to try to make a new cluster. 
	//If we can't make a new cluster, print out that we can't
}
