#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int bad;
static int cmpi(const void* a, const void* b) { return *(const int*)a - *(const int*)b; }
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad++; } } while (0)

int main(void) {
    char b[128];
    snprintf(b, sizeof b, "[%5d|%-5d|%05d|%x|%X|%#x|%o|%c|%5s|%-5s|%.2s|%u]", 42, 42, 42, 255, 255, 255, 8, 'z', "ab", "ab", "abcdef", 4000000000u);
    CHECK(strcmp(b, "[   42|42   |00042|ff|FF|0xff|10|z|   ab|ab   |ab|4000000000]") == 0);
    snprintf(b, sizeof b, "%i %ld %lld %+d % d %*d %.*s %%", -7, -123456L, 5000000000LL, 5, 5, 4, 9, 3, "abcdef");
    CHECK(strcmp(b, "-7 -123456 5000000000 +5  5    9 abc %") == 0);
    CHECK(snprintf(b, 4, "abcdefgh") == 8 && strcmp(b, "abc") == 0);

    int x = 0; unsigned h = 0; char w[32];
    CHECK(sscanf("  12 0x1f hello", "%d %i %s", &x, &h, w) == 3 && x == 12 && h == 31 && strcmp(w, "hello") == 0);

    FILE* f = fopen("/tmp/st.txt", "w");
    CHECK(f != NULL);
    fprintf(f, "line %d\n", 1);
    fputs("line two\n", f);
    for (int i = 0; i < 2000; i++) fputc('a' + i % 26, f);   // crosses the 4 KiB buffer later
    fputc('\n', f);
    CHECK(fclose(f) == 0);

    f = fopen("/tmp/st.txt", "r");
    CHECK(f != NULL);
    char line[64];
    CHECK(fgets(line, sizeof line, f) && strcmp(line, "line 1\n") == 0);
    CHECK(ftell(f) == 7);
    CHECK(fgets(line, sizeof line, f) && strcmp(line, "line two\n") == 0);
    CHECK(fseek(f, -2, SEEK_END) == 0 && ftell(f) == 7 + 9 + 2001 - 2);
    int c = fgetc(f); CHECK(c == 'a' + 1999 % 26);
    CHECK(ungetc(c, f) == c && fgetc(f) == c);
    CHECK(fgetc(f) == '\n' && fgetc(f) == EOF && feof(f));
    CHECK(fseek(f, 7, SEEK_SET) == 0 && fseek(f, 2, SEEK_CUR) == 0 && ftell(f) == 9);
    char big[3000];
    CHECK(fread(big, 1, sizeof big, f) == 7 + 9 + 2001 - 9);
    fclose(f);

    f = fopen("/tmp/st.txt", "a+");
    fputs("tail\n", f);
    fseek(f, 0, SEEK_SET);
    CHECK(fgets(line, sizeof line, f) && strcmp(line, "line 1\n") == 0);
    fclose(f);
    CHECK(fopen("/tmp/nope", "r") == NULL);
    CHECK(remove("/tmp/st.txt") == 0);

    CHECK(atoi("-45") == -45 && strtol("0x20", NULL, 0) == 32 && strtoul("777", NULL, 8) == 511);
    CHECK(strcasecmp("HeLLo", "hello") == 0 && strrchr("a/b/c", '/')[1] == 'c');
    int arr[] = { 5, 2, 9, 1, 7, 3 };
    qsort(arr, 6, sizeof(int), cmpi);
    CHECK(arr[0] == 1 && arr[1] == 2 && arr[5] == 9);
    printf("stdiotest %s\n", bad ? "FAILED" : "ok");
    return bad != 0;
}
