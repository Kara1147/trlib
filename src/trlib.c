/*

MIT License

Copyright (c) 2022 Kara

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <config.h>

#include "trlib.h"

inline __attribute__((always_inline)) size_t unescape(char *s, size_t length)
{
	size_t remaining = length;
	unsigned int charval;
	int skip;

	void *tmp;

	char *sp = s;

	while ((tmp = memchr(sp, '\\', remaining)) != NULL) {
		sp = (char *)tmp + 1;

		if (isdigit(*sp)) {
			/* \octal */
			if (sscanf(sp, "%3o%n", &charval, &skip) == 0)
				goto next;

			length -= skip;
			memmove(sp, sp + skip, remaining + 1);
		} else {
			/* \character */
			switch (*sp) {
			case '\\':
				charval = '\\';
				break;
			case 'f':
				charval = '\f';
				break;
			case 'n':
				charval = '\n';
				break;
			case 'r':
				charval = '\r';
				break;
			case 't':
				charval = '\t';
				break;
			case 'v':
				charval = '\v';
				break;
			default:
				goto next;
			}
			length -= 1;
			memmove(sp, sp + 1, remaining);
		}
		/* TODO: maybe handle \xhexadecimal ? */

		((char *)tmp)[0] = charval;
		s[length] = 0;
	next:
		remaining = length - (sp - s);

		if ((remaining) > length)
			return length;
	}

	return length;
}

/* classes */
#define alnum "[:digit:][:upper:][:lower:]"
#define alpha "[:upper:][:lower:]"
#define blank " \\t"
#define cntrl "\\0-\\37"
#define digit "0123456789"
#define graph "[:alnum:][:punct:]"
#define lower "abcdefghijklmnopqrstuvwxyz"
#define print "[:alnum:][:punct:] "
#define punct "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
#define space "\\t\\n\\v\\f\\r "
#define upper "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define xdigit "0123456789ABCDEFabcdef"

size_t expand_POSIX_C_classes(const char *src, char **dest)
{
	int try_realloc = 0;

	size_t i, delta, remaining;
	size_t size = strlen(src);
	char hidden;

	void *tmp;

	char *begin;
	char *end;

	const char *classptr;
	const char *class_names = "alnum " /* this is why 6 is important! */
				  "alpha "
				  "blank "
				  "cntrl "
				  "graph "
				  "lower "
				  "print "
				  "punct "
				  "space "
				  "upper "
				  "xdigit";
	const char *classes[] = { alnum, alpha, blank, cntrl, graph, lower,
				  print, punct, space, upper, xdigit };
	const size_t class_sizes[] = { sizeof(alnum) - 1, sizeof(alpha) - 1,
				       sizeof(blank) - 1, sizeof(cntrl) - 1,
				       sizeof(graph) - 1, sizeof(lower) - 1,
				       sizeof(print) - 1, sizeof(punct) - 1,
				       sizeof(space) - 1, sizeof(upper) - 1,
				       sizeof(xdigit) - 1 };

	*dest = strdup(src);

	if (strchr(*dest, '[')) {
		/** expand all classes
		 * EXAMPLE: [:CLASSNAME:]
		 */

		end = *dest;

		while ((begin = strstr(end, "[:")) != NULL) {
			if ((end = strstr(begin, ":]")) == NULL)
				break;

			hidden = *end;
			*end = '\0';
			end += 2;

			if ((classptr = strstr(class_names, begin + 2))) {
				i = (classptr - class_names) /
				    6; /* 6 is important! */
				delta = class_sizes[i] - (end - begin);

				if ((tmp = realloc(*dest, size + delta)) !=
				    NULL) {
					if (!try_realloc)
						try_realloc++;

					*dest = (char *)tmp;
					memmove(begin + class_sizes[i], end,
						size - (end - *dest));
					memcpy(begin, classes[i],
					       class_sizes[i]);
					size += delta;

					end = begin;
				}
			} else {
				*end = hidden;
			}
		}

		/* TODO: handle "[=equiv=]" and "[x*n]"
		https://pubs.opengroup.org/onlinepubs/9699919799/utilities/tr.html */
	}

	if (strchr(*dest, '\\'))
		size = unescape(*dest, size);

	if (strchr(*dest, '-')) {
		/** expand all ranges
		 * EXAMPLE: CHARACTER_S-CHARACTER_E
		 * WHERE: CHARACTER_E is __LARGER__ than CHARACTER_S
		 */

		remaining = size;
		end = *dest;

		while ((tmp = memchr(end, '-', remaining)) != NULL) {
			end = (char *)tmp + 1;
			begin = end - 2;

			if (*begin == *end) {
				memmove(begin + 2, end, remaining - 2);
			} else if (*begin + 1 == *end) {
				memmove(begin + 1, end, remaining - 1);
			} else if (*begin < *end) {
				i = (*end - *begin);
				delta = i - (end - begin);

				if ((tmp = realloc(*dest, size + delta)) !=
				    NULL) {
					if (!try_realloc)
						try_realloc++;

					*dest = (char *)tmp;
					memmove(begin + i, end,
						size - (end - *dest));
					size += delta;

					end = begin + i;
					for (hidden = *begin + 1; hidden < *end;
					     hidden++)
						*(++begin) = hidden;
				}
			}

			end++;
			remaining = size - (end - *dest);
		}
	}

	if (try_realloc && ((tmp = realloc(*dest, size)) != NULL))
		*dest = (char *)tmp;

	(*dest)[size] = '\0';

	return size;
}

char *tr(char *s, const char *from, const char *to, const char *mod)
{
	size_t to_size;
	char *to_ex;
	size_t from_size;
	char *from_ex;

	if ((from == NULL) || (*from == '\0'))
		return s;

	from_size = expand_POSIX_C_classes(from, &from_ex);

	if (strchr(mod, 'd')) {
		to_ex = NULL;
	} else {
		if ((to == NULL) || (*to == '\0')) {
			to_size = from_size;
			to_ex = strdup(from_ex);
		} else {
			to_size = expand_POSIX_C_classes(to, &to_ex);
		}
	}

	if (strchr(mod, 't')) {
		if (from_size > to_size)
			from_size = to_size;
	}

	if (strchr(mod, 's')) {
		//TODO: squash all occurances in s of chars in "from"
	}

	if (strchr(mod, 'c')) {
		//TODO: replace all occurances in s of chars NOT IN "from" with the last char of "to"
	} else {
		//TODO: replace all occurances in s of chars in "from" with their respective chars in "to"
	}

	return NULL;
}
