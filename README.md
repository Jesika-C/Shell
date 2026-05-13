# Shell
Command line interface that is written in C.
Created in Ubuntu VM with Visual Studio Code.

Group Members: 

Blake & Jesika 

References:

Source 1: https://www.w3schools.com
Source 2: https://www.geeksforgeeks.org

Phase 2 Implement Features: 

- Can background commands using &
- Can redirect contents for output redirection (>) and input redirection (<) 
- Can use a single pipe (|)
- Will normalize the output if there is not a space between commands and operators
- Wont crash on empty input 
- Throws errors for :
	-> Missing command on either side of pipe
	-> More than one operator
	-> Missing file when redirecting
	-> Not having & at the end of command

Limitations:

- Can not use more than one operation on a single command line
- Only allows single pipe


Instructions:

1. You must type gcc myshell.c -o myshell into the command line
2. Next, in the command line type ./myshell
3. After this the display will show a prompt of myshell> and this is where you can type in the command you want to execute, which could be an output or input redirection, a pipe, background &, or a simple command with no operators
4. You may continue entering commands for as long as you want
5. To exit the program just type exit into the command line and it will show a prompt saying "Shell terminated" and take you out of the program