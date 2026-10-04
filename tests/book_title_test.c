#include <stdio.h>
#include <string.h>

#include "book_title.h"

static int check(const char *input, const char *expected)
{
    char output[128];
    app_book_display_title(output, sizeof output, input);
    if (strcmp(output, expected) == 0) return 0;
    fprintf(stderr, "title mismatch: got '%s', expected '%s'\n", output, expected);
    return 1;
}

int main(void)
{
    int failed = 0;
    failed |= check("K\xc3\xa1\xc2\xbb\xe2\x80\xb9" "ch.epub", "K\xe1\xbb\x8b" "ch");
    failed |= check("K\xc3\xa1\xc2\xbb\xc2\x8b" "ch.epub", "K\xe1\xbb\x8b" "ch");
    failed |= check("Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t.epub", "Ti\xe1\xba\xbfng Vi\xe1\xbb\x87t");
    failed |= check("Cafe\xcc\x81.epu", "Cafe\xcc\x81");
    return failed;
}
