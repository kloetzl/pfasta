#pragma once
#include <pfasta.h>

int pfasta_print(int file_descriptor, const struct pfasta_record *pr,
                 int line_length);
long long my_strtonum(const char *numstr, long long minval, long long maxval,
                      const char **errstrp);
void *my_reallocarray(void *ptr, size_t nmemb, size_t size);
