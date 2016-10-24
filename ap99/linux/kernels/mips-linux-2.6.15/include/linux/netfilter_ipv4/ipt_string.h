#ifndef _IPT_STRING_H
#define _IPT_STRING_H

#define	IPT_STRING_MAX_NUM				10

#define IPT_STRING_MAX_PATTERN_SIZE 		32
#define IPT_STRING_MAX_ALGO_NAME_SIZE 16

struct ipt_string_info
{
	u_int16_t from_offset;
	u_int16_t to_offset;
	char	  algo[IPT_STRING_MAX_ALGO_NAME_SIZE];
	char 	  pattern[IPT_STRING_MAX_NUM][IPT_STRING_MAX_PATTERN_SIZE];
	u_int8_t  patlen[IPT_STRING_MAX_NUM];
	u_int8_t  invert;
	struct ts_config __attribute__((aligned(8))) *config[IPT_STRING_MAX_NUM];

	u_int32_t string_count;
};

#endif /*_IPT_STRING_H*/
