#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>

void reset_terminal_mode();
void set_conio_terminal_mode();
int kbhit();
int getch();
char* ltrim(char* szX);
char* rtrim(char* szX);
char* trim(char* szX);

