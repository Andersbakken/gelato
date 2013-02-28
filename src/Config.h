#ifndef Config_h
#define Config_h

#include <stdio.h>
#include <rct/Path.h>
#include <rct/Rct.h>

struct Config
{
    Config();

    Path socketFile;
};

inline Config::Config()
{
    socketFile = Path::home() + ".gelato";
    FILE *f = fopen((Path::home() + ".gelatorc").constData(), "r");
    if (f) {
        char line[1024];
        int read;
        struct {
            String *string;
            const char *name;
        } const entries[] = {
            { &socketFile, "socket-file" },
            { 0, 0 }
        };
        
        while ((read = Rct::readLine(f, line, sizeof(line))) != -1) {
            char *ch = line;
            while (isspace(*ch))
                ++ch;
            if (*ch == '#')
                continue;
            for (int i=0; entries[i].name; ++i) {
                const int len = strlen(entries[i].name);
                if (!strncmp(ch, entries[i].name, len)) {
                    ch += len;
                    while (isspace(*ch))
                        ++ch;
                    if (*ch == '=') {
                        ++ch;
                        while (isspace(*ch))
                            ++ch;
                        char *end = ch;
                        while (*end && !isspace(*end))
                            ++end;
                        if (end != ch) {
                            *entries[i].string = String(ch, end - ch);
                        }
                    }
                }
            }
        }
        fclose(f);
    }
}

#endif
