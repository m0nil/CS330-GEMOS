#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
int main(int argc, char *argv[])
{
	if (argc != 2)
	{	
		perror("argc");
		printf("Unable to execute\n");
		exit(1);
	}
	const char *dir_path = argv[1];
	DIR *directory = opendir(dir_path);
	if (!directory)
	{	
		// printf("%s\n", dir_path);
		perror("opendir");
		printf("Unable to execute\n");
		exit(1);
	}
	struct dirent *entry; //for iterating over every file/dir/sym_link in the directory
	unsigned long total_size = 0;
	struct stat st;
	if (stat(dir_path, &st) <= -1)
	{	
		perror("stat");
		printf("Unable to execute\n");
		exit(1);
	}
	total_size += st.st_size;
	// printf("%lu\n", total_size); //size of the directory
	while ((entry = readdir(directory)) != NULL)
	{	
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) //ignore . and ..
		{
			continue;
		}
		if (entry->d_type == DT_REG) //regular file
		{	
			char path[1000];
			strcpy(path, dir_path);
			strcat(path, "/");
			strcat(path, entry->d_name);
			struct stat st1;
			if (stat(path, &st1) == -1)
			{	
				perror("stat");
				printf("Unable to execute\n");
				exit(1);
			}
			total_size += st1.st_size;
		}
		
		else if (entry->d_type == DT_DIR && strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..")!=0){ //sub directory
			//creating a pipe to fetch child's output
			int fds[2];
			if (pipe(fds) == -1)
			{	
				perror("pipe");
				printf("Unable to execute\n");
				exit(1);
			}
			pid_t childPid = fork();
			if (childPid < 0){
				perror("fork");
				exit(1);
			}
			else if (childPid == 0){
				// child process
				close(fds[0]);
				dup2(fds[1], STDOUT_FILENO);
				close(fds[1]);
				char path[1000];
				strcpy(path, dir_path);
				strcat(path, "/");
				strcat(path, entry->d_name);
				execl("./myDU", "./myDU", path, NULL);
				perror("execl");
				exit(1);
			}
			else{
				//parent process
				int status;
				wait(&status);
				//child would have written its size in the pipe now parent will read it
				close(fds[1]);
				char buffer[1000];
				int n = read(fds[0], buffer, 1000);
				buffer[n] = '\0';
				char* endptr;
				long unsigned int temp_size= strtoul(buffer, &endptr, 10);
				if (temp_size == 0)
				{
					printf("Unable to execute\n");
					exit(1);
				}
				total_size += temp_size;
				close(fds[0]);
			}
		}
		else if (entry->d_type == DT_LNK)
		{	
			// printf("link\n");
			char path1[1000];
			char linkPath[1000];
			strcpy(linkPath, dir_path);
			strcat(linkPath, "/");
			strcat(linkPath, entry->d_name);
			struct stat linkStat;
			// printf("%s\n", linkPath);
			if (stat(linkPath, &linkStat) != 0) {
				perror("stat");
				return 1;
			}

			if(readlink(linkPath,path1, sizeof(path1)-1)==-1){
				perror("readlink");
				printf("Unable to execute\n");
				exit(1);
			}
			// printf("%s\n", path1);
			int fds[2];
			if (pipe(fds) == -1)
			{	perror("pipe");
				printf("Unable to execute\n");
				exit(1);
			}
			pid_t childPid = fork();
			if (childPid < 0){
				perror("fork");
				exit(1);
			}
			else if (childPid == 0){
				// child process
				close(fds[0]);
				dup2(fds[1], STDOUT_FILENO);
				close(fds[1]);
				char path[1000];
				strcpy(path, dir_path);
				strcat(path, "/");
				strcat(path, path1);
				printf("%s\n", path);
				execl("./myDU", "./myDU", path, NULL);
				perror("execl");
				exit(1);
			}
			else{
				//parent process
				int status;
				wait(&status);
				close(fds[1]);
				char buffer[1000];
				int n = read(fds[0], buffer, 1000);
				buffer[n] = '\0';
				char* endptr;
				long unsigned int temp_size= strtoul(buffer, &endptr, 10);
				if (temp_size == 0)
				{
					printf("Unable to execute\n");
					exit(1);
				}
				total_size += temp_size;
				close(fds[0]);
			}
			}
	}
	printf("%lu\n", total_size);
	closedir(directory);
	return 0;
}
