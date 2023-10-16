#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
int main(int argc, char *argv[])
{	
	if (argc == 1){
		printf("Unable to Execute\n");
		return 0;
	}
	char *endptr;
    unsigned long int num = strtoul(argv[argc-1], &endptr, 10);  // Convert base-10 string to unsigned long
    
    if (endptr == argv[argc-1]) {
        printf("Unable to Execute\n");
		return 0;
    } else if (*endptr != '\0') {
        printf("Unable to Execute");
		return 0;
    }
	long result = round( sqrt((double)num));
	
	if (argc == 2)
		printf("%ld\n", result);
	else
		{
			char *args[argc-1];
			for (int i = 1; i < argc-1; i++)
				args[i-1] = argv[i];
			args[argc-2] = (char *)malloc(30*sizeof(char));
			sprintf(args[argc-2], "%ld", result);
			args[argc-1] = NULL;
			// char* to_exec = (char *)malloc(30*sizeof(char));
			// sprintf(to_exec, "./%s", args[0]);
			execv(args[0], args);
			// execv(, args);
		}
	return 0;
}
