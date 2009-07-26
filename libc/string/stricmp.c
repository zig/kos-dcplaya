/* KallistiOS ##version##

   stricmp.c
   (c)2000 Dan Potter

   $Id: stricmp.c,v 1.2 2002/02/13 10:44:09 andrewk Exp $
*/

#include <string.h>

/* Works like strcmp, but not case sensitive */
int stricmp(const char * cs,const char * ct) {
	int c1, c2, res;

	/* $$$ ben : This was bugged ! I fix it. */
	do {
		c1 = *cs++; c2 = *ct++;
		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
		res = c1 - c2;
	} while (!res && c1 && c2);

	return res;
}

/* Also provides strcasecmp (same thing) */
int strcasecmp(const char *cs, const char *ct) {
	return stricmp(cs, ct);
}

#if 0
/* Works like strcmp, but not case sensitive */
int stricmp(const char * cs,const char * ct) {
	int c1, c2, res;

	while(1) {
		c1 = *cs++; c2 = *ct++;
		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
		if ((res = c1 - c2) != 0 || (!*cs && !*ct))
			break;
	}

	return res;
}

/* Also provides strcasecmp (same thing) */
int strcasecmp(const char *cs, const char *ct) {
	return stricmp(cs, ct);
}
#endif
