
#include <string.h>
#include <ctype.h>

#include "misc.h"

int stricmp(const char *str1, const char *str2)
{
    if(!str1 || !str2)
        return 1;

    for (;; str1++, str2++)
    {
        int d = tolower(*str1) - tolower(*str2);

        if (d != 0 || !*str1 || !*str2)
            return d;
    }    
}

char *strtok_l(char *str1, char *str2)
{
    static char *str;
    char *pos;
    char *ret = NULL;

    if(NULL != str1)
        str = str1;

    if(NULL == str)
        return NULL;

    pos = strstr(str, str2);
    ret = str;
    if(NULL != pos)
    {
        *pos = '\0';
        str = (pos+strlen(str2));
    }
    else
    {
        str = NULL;
    }

    return ret;
}

char *stolower(char *str)
{
    while(*str)
        *str = tolower(*str);

    return str;
}

char *trim(char *str)
{
    char *end;

    while(!isprint(*str) || isblank(*str))
        str++;

    end = str+strlen(str)-1;
    while(isblank(*end))
        end--;

    end[1] = '\0';

    return str;
}

