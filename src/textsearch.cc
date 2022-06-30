#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "textsearch.h"
#include "printlog.h"

// A utility function to get maximum of two integers
int max(int a, int b)
{
    return (a > b) ? a : b;
}
 
// The preprocessing function for Boyer Moore's bad character heuristic
void badCharHeuristic(char *str, int size, int badchar[NO_OF_CHARS])
{
    int i = 0;
 
    // Initialize all occurrences as -1
    for (i = 0; i < NO_OF_CHARS; i++)
        badchar[i] = -1;
 
    // Fill the actual value of last occurrence of a character
    for (i = 0; i < size; i++)
        badchar[(int) str[i]] = i;
}
 
int textsearch(char *txt, char *pat)
{
    int m = strlen(pat);
    int n = strlen(txt);
    unsigned char* tmp = (unsigned char*)txt;
 
    int badchar[NO_OF_CHARS]; 
 
    badCharHeuristic(pat, m, badchar);
 
    int s = 0; // s is shift of the pattern with respect to text
    while (s <= (n - m))
    {
        int j = m - 1;
        while (j >= 0 && pat[j] == tmp[s + j])
            j--;
 
        if (j < 0)
        {
	    return s;
            s += (s + m < n) ? m - badchar[tmp[s + m]] : 1;
	}
        else
            s +=  max(1, j - badchar[tmp[s + j]]);
    }
    return -1;
}
 

bool SearchInFile(char* szFileName, char* pattern)
{
   bool bRetCode = false;
   char buf[BLOCK_SIZE*BLOCK_COUNT];
   char* comment = NULL;

   FILE* f_list = NULL;

   if (access(szFileName, R_OK) != 0)
   {
      	printlog(llWarning, "SearchInFile", "File not found or inaccessible, file-path=%s", szFileName);
       	return false;
   }

   f_list = fopen(szFileName, "r");
   if (f_list == NULL)
   {
        printlog(llError, "SearchInFile", "Cant open file, file-path=%s", szFileName);
        return false;
    }

    while (!feof(f_list))
    {
        fread(buf , BLOCK_SIZE, BLOCK_COUNT, f_list);
	if(textsearch(buf, pattern) >= 0 )
  	{	
             bRetCode = true;
	     break; 	
	}	
    }

   fclose(f_list);
   return bRetCode;
}
