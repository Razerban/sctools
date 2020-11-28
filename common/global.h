#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#define INVALID_NUMBER -9999

struct token_t
{
	const char*	token;
	int			value;
};

#define BLOCK_NONE -1
#define BLOCK_LAYERDEF 0
#define BLOCK_REMAP 1
#define BLOCK_MACRO 2
#define BLOCK_CONFIG 3
#define BLOCK_MATRIX 4

#define CONFIG_LED_CAPS 1
#define CONFIG_LED_SCROLL 2
#define CONFIG_LED_NUM 3

#ifndef _MSC_VER
#define strnicmp strncasecmp
#define stricmp strcasecmp
#endif

#endif // __GLOBAL_H__
