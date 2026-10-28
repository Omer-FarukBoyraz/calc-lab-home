// Omer Faruk Boyraz 241ADB161
// https://github.com/Omer-FarukBoyraz/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
    FILE *file;
    char line[256];

    if (argc < 2) {
        fprintf(stderr, "Missing input file\n");
        return 1;
    }

    file = fopen(argv[1], "r");

    if (file == NULL) {
        fprintf(stderr, "Cannot open input file\n");
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *p = line;
        long long result;
        long long number;

        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            continue;

        result = strtoll(p, &p, 10);

        while (*p != '\0' && *p != '\n') {
            char op;

            while (*p == ' ' || *p == '\t')
                p++;

            if (*p == '\0' || *p == '\n')
                break;

            op = *p;
            p++;

            while (*p == ' ' || *p == '\t')
                p++;

            number = strtoll(p, &p, 10);

            if (op == '+')
                result += number;
            else if (op == '-')
                result -= number;
        }

        printf("%lld\n", result);
    }

    fclose(file);
    return 0;
}