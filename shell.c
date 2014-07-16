/***********************************/
/*                                 */
/*  Brian Dicola and Austin Odell  */
/*  CSC 1600: Operating Systems    */
/*  Team Project - Phase 5         */
/*                                 */
/***********************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

pid_t childid;
pid_t savecid = 0;
pid_t parentid;
int tstpsave, identifier;
int isChild = 0;  //Is there a child at the moment?

#define NUMENTRIES 50
#define ENTRYSIZE 50
#define INPUTSIZE 2500

void readIn(char input[INPUTSIZE]);
void parse(void *tokens,char input[INPUTSIZE],char parsedData[ENTRYSIZE][NUMENTRIES]);
void sigtstp_handler();
void resume_handler();

/***********************************/
/*                                 */
/*  This program is a custom       */
/*  shell.  It accepts a line of   */
/*  input and parses that input    */
/*  into individual tokens which   */
/*  are then dealt with            */
/*  accordingly.                   */
/*                                 */
/***********************************/

int main(int argc, char *argv[])
{
	char input[INPUTSIZE], parsedData[NUMENTRIES][ENTRYSIZE], *args[NUMENTRIES];
	int tokens = 0, a = 0, childstate, customOut = 0, customIn = 0, outputFile, inputFile, resumed = 0;
	int wasSignal = 0;

	signal(SIGINT,SIG_IGN);
	signal(SIGTSTP, sigtstp_handler);
	
	parentid = getpid();
	//printf("Parent id = %p \n", parentid);
	
	// Infinite loop - runs until "exit" is typed
	while(1)
	{
		identifier = 0;
		wasSignal = 0;
		
		// Prints "my-shell$: " on each new line
		fputs("my-shell$: ", stdout);
		
		// Reads input from stdin (user typed data)
		readIn(input);
		
		// Parse input into individual tokens and store it in parsedData array
		parse(&tokens,input,parsedData);
		
		// Checks if first token is "exit"
		if(strcmp(parsedData[0],"exit")==0)
		{
			return 0; // exits
 		}
		else if(strcmp(parsedData[0],"resume")==0)
		{
			resume_handler();
			resumed = 1;
		}
		
		
		
		if(resumed == 0)
		{
		// Stores each token into pointer array to be used in execvp command
			for(a = 0; a < tokens; a++)
			{
				args[a] = parsedData[a];
			}
			
			if(strcmp(args[0], "^Z") == 0 || strcmp(args[0], "^C") == 0)
			{
				wasSignal = 1;
				printf("Signal received. \n");
			}
		
			// Stores NULL into the last entry in the pointer array
			args[a+1]=(char *)0;
			
			// Forks a process
			if(wasSignal == 0)
			{
				childid = fork();
				isChild = 1;
				// Child process
				if(childid==0)
				{
					identifier = 1;
					//printf("Child id (global) = %p \n", childid);
					signal(SIGINT,SIG_DFL);
					signal(SIGTSTP,sigtstp_handler);
					
					// Iterate through tokens and check for ">" or "<"
					for(a = 0; a < tokens; a++)
					{
						// Checks for ">" token
						if(strcmp(parsedData[a],">")==0 && a<(tokens-1))
						{
							close(1); // Closes stdout
							
							// Opens file for output with flags - write only, create, append
							// Note - append can be removed easily if requirement is for no append
							outputFile = open(parsedData[a+1],O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
							customOut = 1;
							
							// Setting args[a] as NULL prevents exec from seeing ">"
							args[a] = (char *)0;
						}
						// Checks for ">" as first character of token
						else if(parsedData[a][0]=='>')
						{
							// Creates temporary string of filename
							char* tempstr = parsedData[a] + 1;
							close(1); // Closes stdout
							
							// Opens file for output with flags - write only, create, append
							// Note - append can be removed easily if requirement is for no append
							outputFile = open(tempstr,O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
							customOut = 1;
							
							// Settings arga[a] as NULL prevents exec from seeing ">"
							args[a] = (char *)0;
						}
						// Checks for "<" token
						if(strcmp(parsedData[a],"<")==0 && a<(tokens-1))
						{
							close(0); // Closes stdin
							
							// Opens file for output with flag - read only
							inputFile = open(parsedData[a+1],O_RDONLY,S_IRUSR|S_IWUSR);
							customIn = 1;
							
							// Setting args[a] as NULL prevents exec from seeing ">"
							args[a] = (char *)0;
						}
						// Checks for "<" as first character of token
						else if(parsedData[a][0]=='<')
						{
							// Creates temporary string of filename
							char* tempstr = parsedData[a] + 1;
							close(0); // Closes stdin
							
							// Opens file for output with flag - read only
							inputFile = open(tempstr,O_RDONLY,S_IRUSR|S_IWUSR);
							customIn = 1;
							
							// Setting args[a] as NULL prevents exec from seeing ">"
							args[a] = (char *)0;
						}
					}
					
					// If daemon process is requested
					if(strcmp(args[tokens - 1], "&") == 0)
					{
						// Setting args[tokens-1] as NULL prevents exec from seeing "&"
						args[tokens - 1] = (char *)0;
						printf("Spawning Daemon...\n");
						
						// Changes the file mode mask
						umask(0);
						
						// Creates a new SID for the daemon child process
						pid_t sid = setsid();
						printf("SID = %d\n", sid);
						
						// If there is an error creating a daemon
						if (sid < 0) 
						{
							exit(EXIT_FAILURE);
						}
						
						// Closes the standard file descriptors
						close(STDIN_FILENO);
						close(STDOUT_FILENO);
						close(STDERR_FILENO);
						
						// Daemon execution - happens in the background
						if(execvp(args[0],args)==-1) 
						{
							// If execution fails
							printf("%s\n",strerror(errno));
						}
						
					}
					
					// Try executing user input - no daemon requested
					else if(execvp(args[0],args)==-1) 
					{
						// If execution failed
						printf("%s\n",strerror(errno));
					}
					
					// Exit child process error-free
					exit(0);
				}
				// If there is an error forking
				else if(childid<0)
				{
					// Exit with an error
					exit(1);
				}
				
				// Parent process
				else
				{
					identifier = 0;
					
					if(strcmp(args[tokens - 1], "&") != 0)
					{
						// Wait for child process to complete if no ampersand
						waitpid(-1,&childstate,WUNTRACED);
						isChild = 0;
						resumed  = 0;
						// If the child has exited with an error notify the user
						if(WEXITSTATUS(childstate!=0))
						{
							printf("An error occured during execution\n");
						}
					}
				}
			} 
		}
					else 
					{
						resumed = 0;
						waitpid(-1,&childstate,WUNTRACED);
						isChild = 0;
						//savecid = 0;
					}
	}
	if(customOut==1)
	{
		close(outputFile);
	}
	if(customIn==1)
	{
		close(inputFile);
	}
	return 0; // exits
}

/***********************************/
/*                                 */
/*  The readIn function takes the  */
/*  user's input and stores it in  */
/*  an array of characters.        */
/*                                 */
/***********************************/
void readIn(char input[INPUTSIZE])
{
	int a = 0;
	
	// The size limit of the input is "INPUTSIZE" (a global variable)
	while(a<INPUTSIZE)
	{
		// The next character is fetched from stdin and stored
		input[a++] = fgetc(stdin);
		
		// As long as there is a character and if the most recent character is the
		// new line character
		if(a>1 && (input[a-1] == '\n'))
		{
			// Replaces new line character with nul character
			input[a-1]='\0';
			
			// Stop reading input
			break;
		}
	}
}

/***********************************/
/*                                 */
/*  The parse function takes the   */
/*  input character array and      */
/*  separates out each token into  */
/*  a two dimensional character    */
/*  array.                         */
/*                                 */
/***********************************/
void parse(void *tokens, char input[INPUTSIZE], char parsedData[NUMENTRIES][ENTRYSIZE])
{
	char *ptr = input;
	int *num = (int *) tokens, count = 0;
	
	// Set ptr equal to the first token separated by a space (" ")
	ptr = strtok(input," ");
	
	// Loop executing "NUMENTRIES" times - a global variable indicating the most tokens
	// that can be held by the two dimensional array
	for(count = 0; count<NUMENTRIES; count++)
	{
		// If there is another token
		if(ptr != NULL)
		{
			// Copy token into array
			strcpy(parsedData[count],ptr);
			
			// Parse next token
			ptr = strtok(NULL," ");
		}
		else
		{
			// Set last token entry to nul
			parsedData[count][0] = '\0';
			
			// Store number of tokens
			*num = count;
			
			// Exit loop
			count = NUMENTRIES;
		}
	}
}

void sigtstp_handler()
{
	//Signal came from parent
	if(identifier == 0)
	{
		if(isChild == 1)
		{
			if(savecid == 0)
			{
				savecid = childid;
				//printf("\nPausing PID %d\n",savecid);
			}
			else
			{
				printf("Overwriting previously stopped process with %p...\n", childid);
				kill(savecid,SIGKILL);
				savecid = childid;
			}
		}
	}
	return;
}

void resume_handler()
{
	int childstate;
	
	if(savecid != 0)
	{
		//printf("Resuming PID %d\n",savecid);
		kill(savecid,SIGCONT);
	}
	else
	{
		//printf("Nothing to resume...\n");
	}
	return;
}
