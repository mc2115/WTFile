#include <stdio.h>

#include "../include/Utils.h"

const char* getTaskName()
{
    return "WTFTest";
}

void replace_str(char *target, const char *needle, const char *replacement)
{
    char buffer[1024] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1)
    {
        const char *p = strstr(tmp, needle);

        // walked past last occurrence of needle; copy remaining part
        if (p == NULL)
        {
            strcpy(insert_point, tmp);
            break;
        }

        // copy part before needle
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        // copy replacement string
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;

        // adjust pointers, move on
        tmp = p + needle_len;
    }

    // write altered string back to target
    strcpy(target, buffer);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        reportError("Usage: %s testplan-file", argv[0]);
        exit(1);
    }

    FILE *plan = fopen(argv[1], "r");
    if (plan == NULL)
    {
        reportError("Failed to open file %s for read", argv[1]);
        exit(1);
    }

    char line[2048];
    int status = 0;
    char testname[128];
    while (fgets(line, sizeof(line), plan))
    {
        if (strlen(line) > 3 && line[0] == '#' && line[1] == '#' && line[2] == '#')
        {
            if (status == 0)
            {
                printf("Successfully finished test %s\n", testname);
            }
            else
            {
                printf("Test %s failed\n", testname);
            }
        }
        else if (strlen(line) > 2 && line[0] == '#' && line[1] == '#')
        {
            printf("Start test:%s", &line[2]);
            line[strlen(line)-1] = '\0';
            strncpy(testname, &line[2], sizeof(testname));
            status = 0;
        }
        else
        {
            if (status != 0)
            {
                continue;
            }
            replace_str(line, "client", "../../WTF");
            replace_str(line, "server", "../../WTFServer");
            status = system(line);
            if (status != 0)
            {
                printf("Failed running cmd:%s status:%d\n", line, status);
                continue;
            }

            sleep(1);
        }
    }
    system("pkill WTFServer -g 0 >> server");
    //system("kill -9 $(ps aux |grep WTF |grep $(whoami) | awk '{print $2;}')");
    return status;
}
