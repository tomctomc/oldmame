
#include "stricmp.h"
#include <stdio.h>

int stricmp(const char *s1, const char *s2)
{
	int c1, c2;
	if( !s1 || !s2 ) {
		return (int) (s1-s2);
	}
	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != '\0');
	return c1 - c2;
}

int strnicmp(const char *s1, const char *s2, int n)
{
	int c1, c2;
	if( !s1 || !s2 ) {
		return (int) (s1-s2);
	}
	do {
		 c1 = tolower(*s1++);
		c2 = tolower(*s2++);
		n--;
	} while (c1 == c2 && c1 != '\0' && n > 0);
	return c1 - c2;
}
