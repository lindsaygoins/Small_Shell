# Small_Shell
This shell implements a subset of features of the bash shell. This shell:
1. Provides a prompt for running commands
2. Handles blank lines and comments (beginning with #)
3. Provides expansion for the variable $$
4. Executes various commands
5. Supports input and output redirection
6. Supports running commands as foreground or background processes
7. Implements custom handlers for 2 signals, SIGINT and SIGTSTP (SIGINT (Ctrl + C) is ignored by child processes running as foreground processes and SIGTSTP (Ctrl + Z) toggles on and off the ability to run processes in the background)


To complie and run the program, run the following commands:

gcc --std=gnu99 -o smallsh main.c

./smallsh
