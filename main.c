// Author: Lindsay Goins
// Date: 2/2/21
// Description: Implementation of a shell. This program:
// 1. Provides a prompt for running commands.
// 2. Handles blank lines and comments, which are lines 
//    beginning with the # character.
// 3. Provides expansion for the variable $$.
// 4. Executes 3 commands (exit, cd, and status) via code 
//    built into the shell.
// 5. Executes other commands by creating new processes 
//    using execvp().
// 6. Supports input and output redirection.
// 7. Supports running commands in foreground and 
//    background processes.
// 8. Implements custom handlers for 2 signals, 
//    SIGINT and SIGTSTP.
// Sources used: https://repl.it/@cs344/53siguserc
//		 https://repl.it/@cs344/studentsc#main.c
//		 https://repl.it/@cs344/54sortViaFilesc
//		 https://repl.it/@cs344/42waitpidexitc
//		 https://repl.it/@cs344/42execvforklsc

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h>
# include <unistd.h> 
# include <signal.h>
# include <sys/wait.h>
# include <fcntl.h>


// Global variables to track exit status, background processes, and foreground mode
int status = 0;
int bgPid[200];
int fgMode = 0;


// Struct for user input
struct input {
	char* command;
	char* arg[512];
	char* in;
	char* out;
	char background[2];
};


// Deallocates memory for the command
void deallocateMem(struct input* currCommand) {
	free(currCommand->command);

	int i = 0;
	while (currCommand->arg[i] != NULL) {
		free(currCommand->arg[i]);
		i++;
	}

	if (currCommand->in != NULL) {
		free(currCommand->in);
	}

	if (currCommand->out != NULL) {
		free(currCommand->out);
	}

	free(currCommand);
}


// Custom signal handling function that can turn off and on background processes
void handle_SIGTSTP(int signo) {

	// If the program is not in foreground mode, enter foreground mode
	if (fgMode == 0) {
		fgMode = 1;
		char* message = "Entering foreground-only mode (& is now ignored)\n: ";
		write(STDOUT_FILENO, message, 52);
	}

	// If the program is in foreground mode, exit foreground mode
	else {
		fgMode = 0;
		char* message = "Exiting foreground-only mode\n: ";
		write(STDOUT_FILENO, message, 32);
	}
}


// Expands "$$" to the process ID of smallsh shell
char* expandVar(char* buffer) {
	char pid[10];
	sprintf(pid, "%d", getpid());
	char varExp[3] = "$$";
	char* command = malloc(2049);

	char* varPtr = strstr(buffer, varExp);
	while (varPtr != NULL) {
		strcpy(command, (varPtr + 2));
		strcpy(varPtr, pid);
		strcat(buffer, command);
		varPtr = strstr(buffer, varExp);
	}

	free(command);
	return buffer;
}


// Parses a command and stores its components into a struct
struct input* parseInput(char* buffer) {
	struct input* currCommand = malloc(sizeof(struct input));
	currCommand->command = NULL;
	for (int i = 0; i < 513; i++) {
		currCommand->arg[i] = NULL;
	}
	currCommand->in = NULL;
	currCommand->out = NULL;
	char* saveptr1;

	// Saves the command
	char* token = strtok_r(buffer, " ", &saveptr1);
	int len = strlen(token);
	if (token[len - 1] == '\n') {
		token[len - 1] = '\0';
	}
	currCommand->command = malloc(len + 1);
	strcpy(currCommand->command, token);

	// Saves the arguments
	int i = 0;
	token = strtok_r(NULL, " ", &saveptr1);
	while (token != NULL) {
		
		// If the next character is I/O redirection or an '&'
		if (('<' == *token) || ('>' == *token) || ('&' == *token)) {
			if (('&' == *token)) {
				
				// If the '&' is the last character entered
				if (strcmp(token, "&\n") == 0) {
					currCommand->background[0] = '&';
					return currCommand;
				}

				// If the '&' is not the last character
				else {
					currCommand->arg[i] = malloc(strlen(token) + 1);
					strcpy(currCommand->arg[i], token);
					i++;
				}
			} 
			
			// If the character is I/O redirection
			else {
				break;
			}
		}

		// If the next word is an argument
		else {
			int len = strlen(token);
			if (token[len - 1] == '\n') {
				token[len - 1] = '\0';
			}
			currCommand->arg[i] = malloc(len + 1);
			strcpy(currCommand->arg[i], token);
			i++;
		}

		token = strtok_r(NULL, " ", &saveptr1);
	}

	while (token != NULL) {
		
		// Saves the input redirection
		if ('<' == *token) {
			token = strtok_r(NULL, " ", &saveptr1);
			int len = strlen(token);
			if (token[len - 1] == '\n') {
				token[len - 1] = '\0';
			}
			currCommand->in = malloc(len + 1);
			strcpy(currCommand->in, token);
			token = strtok_r(NULL, " ", &saveptr1);
		}

		// Saves the ouput redirection
		if (token != NULL) {
			if ('>' == *token) {
				token = strtok_r(NULL, " ", &saveptr1);
				int len = strlen(token);
				if (token[len - 1] == '\n') {
					token[len - 1] = '\0';
				}
				currCommand->out = malloc(len + 1);
				strcpy(currCommand->out, token);
				token = strtok_r(NULL, " ", &saveptr1);
			}
		}

		// Saves the background option
		if (token != NULL) {
			if (strcmp(token, "&\n") == 0) {
				currCommand->background[0] = '&';
				token = strtok_r(NULL, " &\n", &saveptr1);
			}
		}
	}
	return currCommand;
}


// Gets info from user regarding the command
struct input* getInput(void) {
		char* buffer;
		buffer = malloc(2049);
		size_t length = 2049;
		ssize_t line;
		struct input* currCommand;

		// Read user command
		line = getline(&buffer, &length, stdin);

		// If there was an error reading the command
		if (line == -1) {
			clearerr(stdin);
		}

		// If there are more than 2048 characters in the command
		if (line > 2048) {
			printf("Too many characters in the command. Input must be under 2048 characters.\n");
			return 0;
		}

		// If the command has 2048 characters or less
		else {

			// Perform variable expansion for $$
			buffer = expandVar(buffer);

			// If the input is a comment or a blank line
			if (buffer[0] == '#' || buffer[0] == '\n') {
				struct input* currCommand = malloc(sizeof(struct input));
				currCommand->command = NULL;
				free(buffer);
				return currCommand;
			}

			// If the input is not a comment, parse the input
			else {
				currCommand = parseInput(buffer);
				free(buffer);
				return currCommand;
			}
		}
}


// Redirects the input and output based on user specification
void ioRedirection(struct input* currCommand) {
	int result;

	// If there is input redirection
	if (currCommand->in != NULL) {

		// Open input file or print error if unable to open
		int inFD = open(currCommand->in, O_RDONLY, 0644);
		if (inFD == -1) {
			status = 1;
			perror("open()");
			exit(1);
		}

		// Redirect stdin to input file or print error
		result = dup2(inFD, 0);
		if (result == -1) {
			perror("dup2()");
			exit(2);
		}
	}

	// If there is output redirection
	if (currCommand->out != NULL) {

		// Open output file or print error if unable to open
		int outFD = open(currCommand->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (outFD == -1) {
			status = 1;
			perror("open()");
			exit(1);
		}

		// Redirect stdout to output file or print error
		result = dup2(outFD, 1);
		if (result == -1) {
			perror("dup2()");
			exit(2);
		}
	}
}


// Checks to see if background child processes have terminated
void checkBgPid(void) {
	int childStatus;
	
	for (int i = 0; i < 201; i++) {

		// If the child has exited
		if (waitpid(bgPid[i], &childStatus, WNOHANG) != 0 && bgPid[i] != 0) {

			// If the child exited normally
			if (WIFEXITED(childStatus)) {
				printf("background pid %d is done : exit value %d\n", bgPid[i], WEXITSTATUS(childStatus));
				fflush(stdout);
			}

			// If the child exited abnormally
			else {
				printf("background pid %d is done : terminated by signal %d\n", bgPid[i], WTERMSIG(childStatus));
				fflush(stdout);
			}

			// Remove the pid from the list of background pids
			bgPid[i] = 0;
		}
	}
}


// Runs a command in the foreground
void jobForeground(struct input* currCommand) {
	char* argv[514];

	// Set first value in array to the command
	argv[0] = malloc(strlen(currCommand->command) + 1);
	strcpy(argv[0], currCommand->command);

	// Create array of arguments
	int i = 0;
	while (currCommand->arg[i] != NULL) {
		argv[i + 1] = malloc(strlen(currCommand->arg[i]) + 1);
		strcpy(argv[i + 1], currCommand->arg[i]);
		i++;
	}
	argv[i + 1] = NULL;

	int childStatus;

	// Fork a new process
	pid_t spawnPid = fork();

	switch (spawnPid) {

	// If there was an error with fork()
	case -1:
		perror("fork()");
		exit(1);
		break;

	// Child process
	case 0:

		{
		struct sigaction SIGINT_action = { {0} };
		struct sigaction SIGTSTP_action = { {0} };
		
		// Set signal handler to default behavior for SIGINT
		SIGINT_action.sa_handler = SIG_DFL;
		SIGINT_action.sa_flags = SA_RESTART;
		sigaction(SIGINT, &SIGINT_action, NULL);

		// Set signal handler to ignore SIGTSTP
		SIGTSTP_action.sa_handler = SIG_IGN;
		sigfillset(&SIGTSTP_action.sa_mask);
		SIGTSTP_action.sa_flags = SA_RESTART;
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);
		}

		// If the command has input or output redirection
		if (currCommand->in != NULL || currCommand->out != NULL) {
			ioRedirection(currCommand);
		}
		
		// Replace the current program with the specified command/arguments
		execvp(argv[0], argv);

		// Returns if there is an error with execvp()
		status = 1;
		perror("execvp()");
		exit(1);
		break;

	// Parent process
	default:

		// Waits for the termination of the child process
		spawnPid = waitpid(spawnPid, &childStatus, 0);

		// Updates the status with the exit value or the termination signal
		if (WIFEXITED(childStatus)) {
			status = WEXITSTATUS(childStatus);
		}

		else {
			status = WTERMSIG(childStatus);
			printf("terminated by signal %d\n", status);
		}

		// Deallocate memory for the array of arguments
		int j = 0;
		while (argv[j] != NULL) {
			free(argv[j]);
			j++;
		}
		break;
	}
}


// Runs a command in the background
void jobBackground(struct input* currCommand) {
	char* argv[514];

	// Set first value in array to the command
	argv[0] = malloc(strlen(currCommand->command) + 1);
	strcpy(argv[0], currCommand->command);

	// Create array of arguments
	int i = 0;
	while (currCommand->arg[i] != NULL) {
		argv[i + 1] = malloc(strlen(currCommand->arg[i]) + 1);
		strcpy(argv[i + 1], currCommand->arg[i]);
		i++;
	}
	argv[i + 1] = NULL;

	int childStatus;

	// Fork a new process
	pid_t spawnPid = fork();

	switch (spawnPid) {

	// If there was an error with fork()
	case -1:
		perror("fork()\n");
		exit(1);
		break;

	// Child process
	case 0:

	{
		struct sigaction SIGTSTP_action = { {0} };

		// Set signal handler to ignore SIGTSTP
		SIGTSTP_action.sa_handler = SIG_IGN;
		sigfillset(&SIGTSTP_action.sa_mask);
		SIGTSTP_action.sa_flags = SA_RESTART;
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	}
		
		printf("background pid is %d\n", getpid());
		fflush(stdout);

		// If there is no input redirection
		if (currCommand->in == NULL) {
			currCommand->in = malloc(10);
			strcpy(currCommand->in, "/dev/null");
		}

		// If there is no output redirection
		if (currCommand->out == NULL) {
			currCommand->out = malloc(10);
			strcpy(currCommand->out, "/dev/null");
		}

		// Redirect I/O
		ioRedirection(currCommand);

		// Replace the current program with the specified command/arguments
		execvp(argv[0], argv);

		// Returns if there is an error with execvp()
		status = 1;
		perror("execvp()");
		exit(1);
		break;

	// Parent process
	default:
	{
		// Add child pid to the array of background pids
		int i = 0;
		while (bgPid[i] != 0) {
			i++;
		}
		bgPid[i] = spawnPid;
	}
		// Check to see if child process is done, but do not wait for it
		if (waitpid(spawnPid, &childStatus, WNOHANG) != 0) {

			// If the child exited normally
			if (WIFEXITED(childStatus)) {
				printf("background pid %d is done : exit value %d\n", bgPid[i], WEXITSTATUS(childStatus));
				fflush(stdout);
			}

			// If the child exited abnormally
			else {
				printf("background pid %d is done : terminated by signal %d\n", bgPid[i], WTERMSIG(childStatus));
				fflush(stdout);
			}
			bgPid[i] = 0;

			// Deallocate memory for the array of arguments
			int j = 0;
			while (argv[j] != NULL) {
				free(argv[j]);
				j++;
			}
		}
		break;
	}
}


// Executes various commands
void executeCommand(struct input* currCommand) {

	// If the command is "exit"
	if (strcmp(currCommand->command, "exit") == 0) {
		deallocateMem(currCommand);

		// Kill child processes
		for (int i = 0; i < 201; i++) {
			kill(bgPid[i], SIGTERM);
		}

		// Kill parent process
		kill(getpid(), SIGTERM);
	}
	
	// If the command is "cd"
	else if (strcmp(currCommand->command, "cd") == 0) {
		
		// If an argument is not provided, change the directory to the HOME directory
		if (currCommand->arg[0] == '\0') {
			chdir(getenv("HOME"));
		}
		
		// If an argument was provided
		else {
			
			// If the directory could not be found
			if (chdir(currCommand->arg[0]) == -1) {
				printf("The provided path could not be found.\n");
				fflush(stdout);
			}
		}
	}

	// If the command is "status"
	else if (strcmp(currCommand->command, "status") == 0) {
		if (status > 1) {
			printf("terminated by signal %d\n", status);
		}

		else {
			printf("exit value %d\n", status);
			fflush(stdout);
		}
	}

	// If the command is not "exit", "cd", or "status" and the command is run in the background
	else if (*currCommand->background == '&' && fgMode == 0) {
		jobBackground(currCommand);
	}

	// If the command is not "exit", "cd", or "status" and the command is run in the foreground
	else {
		jobForeground(currCommand);
	}
}


// Prompts user for commands and executes those commands until the user enters "exit"
int main(void) {
	
	// Initialize sigaction struct and loop
	struct sigaction SIGINT_action = { {0} };
	struct sigaction SIGTSTP_action = { {0} };
	int loop = 0;

	while (loop == 0) {

		// Set up SIGINT sigaction struct
		SIGINT_action.sa_handler = SIG_IGN;
		SIGINT_action.sa_flags = SA_RESTART;
		sigaction(SIGINT, &SIGINT_action, NULL);

		// Set up SIGTSTP sigaction struct
		SIGTSTP_action.sa_handler = handle_SIGTSTP;
		sigfillset(&SIGTSTP_action.sa_mask);
		SIGTSTP_action.sa_flags = SA_RESTART;
		sigaction(SIGTSTP, &SIGTSTP_action, NULL);

		// Checks for completed background processes
		checkBgPid();

		printf(": ");
		fflush(stdout);

		// Gets user input and stores it in a struct
		struct input* currCommand = getInput();
		
		// If the input is a comment or blank line
		if (currCommand->command == NULL) {
			free(currCommand);
			continue;
		}

		// Otherwise, execute the command
		else {
			executeCommand(currCommand);
			deallocateMem(currCommand);
		}
	}
	return 0;
}
