#ifndef STR_H
#define STR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LinkedStringNode LinkedStringNode;
typedef struct LinkedString LinkedString;

struct LinkedStringNode {
	const char *chars;
	int len;
	LinkedStringNode *next;
};

struct LinkedString {
	LinkedStringNode *first;
	LinkedStringNode *last;
};

LinkedString *lstr_empty();
LinkedString *lstr_new(const char *fmt, ...);
void lstr_free(LinkedString *str);
void lstr_add(LinkedString *str, const char *fmt, ...);
char *lstr_flat(LinkedString *str);

#if LOG_LEVEL <= LOG_LEVEL_FATAL && !defined DISABLED_ALL_LOGS
	#define LogString(level, lstr) \
		{ \
			char *LogString__str = lstr_flat(lstr); \
			Log(level, LogString__str); \
			free(LogString__str); \
		}
#else
	#define LogString(level, lstr)
#endif

#ifdef __cplusplus
}
#endif

#endif