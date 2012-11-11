#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "chord.h"

#define LSTR_VADD(str, fmt) \
	{ \
		va_list args; \
		va_start(args, fmt); \
		int formatted_len = vsnprintf(NULL, 0, fmt, args) + 1; \
		va_end(args); \
		\
		va_start(args, fmt); \
		lstr_add_node(str, formatted_len, fmt, args); \
		va_end(args); \
	}

void lstr_add_node(LinkedString *str, int formatted_len, const char *fmt, va_list args);

LinkedString *lstr_empty()
{
	return calloc(1, sizeof(LinkedString));
}

LinkedString *lstr_new(const char *fmt, ...)
{
	LinkedString *str = lstr_empty();
	
	LSTR_VADD(str, fmt)
	
	return str;
}

void lstr_free(LinkedString *str)
{
	if (str) {
		while (str->first != NULL) {
			LinkedStringNode *next = str->first->next;
			free(str->first);
			str->first = next;
		}
		free(str);
	}
}

/* A va_list can only be iterated over once, so we have to ask the calling
 * function to figure out the formatted length by creating two va_lists from
 * its variadic arguments.
 */
void lstr_add_node(LinkedString *str, int formatted_len, const char *fmt, va_list args)
{
	LinkedStringNode *node = calloc(1, sizeof(LinkedStringNode));
	node->len = formatted_len;
	node->chars = malloc(node->len);
	vsprintf((char *)node->chars, fmt, args);
	
	if (str->last == NULL) {
		str->first = node;
		str->last = node;
	}
	else {
		LinkedStringNode *prev = str->last;
		str->last = node;
		prev->next = node;
	}
}

void lstr_add(LinkedString *str, const char *fmt, ...)
{
	LSTR_VADD(str, fmt)
}

char *lstr_flat(LinkedString *str)
{
	int len = 0;
	LinkedStringNode *node = str->first;
	while (node) {
		len += node->len;
		node = node->next;
	}
	
	char *flat = malloc(len);
	flat[0] = '\0';
	
	node = str->first;
	while (node) {
		strcat(flat, node->chars);
		node = node->next;
	}
	return flat;
}