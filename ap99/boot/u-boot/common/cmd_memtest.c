/*******************************************************************************
 *
 * Copyright (c) 2010 TP-Link Technologies CO.,LTD.
 * All rights reserved.
  
 * Filename:		cmd_memtest.c
 * Version:			1.0
 * Abstract:		
 *
 * Author:			÷£–«πÃ <zhengxinggu@tp-link.net>
 * Created Date:	03/02/2010
 *
 * Modified History:	
 *
 *
 *******************************************************************************/
#include <common.h>
#include <command.h>

enum {
	TEST_PATTERN_READ_ONLY = 0,
	TEST_PATTERN_WRITE_ONLY = 1,
	TEST_PATTERN_READ_WRITE,
	TEST_PATTERN_WRITE_READ,
	TEST_PATTERN_ALL,
};

#define UBOOT_RESERVED_MEM			0x00100000		/* 1MB memory */

/* ∂¡≤‚ ‘ */
#define MEM_TEST_R_PRINT_DELTA		0x01000000		/* ¥Ú”°º‰∏Ù */
#define MEM_TEST_R_BURST			0x00000010		/* ≤‚ ‘¥Œ ˝ */

/* –¥≤‚ ‘ */
#define MEM_TEST_W_PRINT_DELTA		0x01000000		/* ¥Ú”°º‰∏Ù */
#define MEM_TEST_W_BURST			0x00000001		/* ≤‚ ‘¥Œ ˝ */

/* ∂¡–¥≤‚ ‘ */
#define MEM_TEST_RW_PRINT_DELTA		0x01000000		/* ¥Ú”°º‰∏Ù */
#define MEM_TEST_RW_BURST			0x00000001		/* ≤‚ ‘¥Œ ˝ */

#define TEST_BURSTS(i, bursts)		((bursts) ? (i < bursts) : 1)
#define TEST_PRINT(x)				printf x
#define DBG_PRINT(j, d, arg) \
    do { \
    if (d <= j++) \
	{ \
		j = 0; \
        printf arg; \
        if (ctrlc()) \
        { \
			putc ('\n'); \
			return 0; \
        } \
    } \
    }while(0)

extern int cmd_get_data_size(char* arg, int default_size);

static const ulong bitpattern[] = {
	0x00000001,	/* single bit */
	0x00000003,	/* two adjacent bits */
	0x00000007,	/* three adjacent bits */
	0x0000000F,	/* four adjacent bits */
	0x00000005,	/* two non-adjacent bits */
	0x00000015,	/* three non-adjacent bits */
	0x00000055,	/* four non-adjacent bits */
	0xaaaaaaaa,	/* alternating 1/0 */
};

#if (CONFIG_COMMANDS & CFG_CMD_MEMORY)

static void mem_fill_mem(ulong area, ulong max_addr)
{
	ulong    i, j, k;
	ulong    count, extent;

	vu_long	*p1;

	extent = max_addr - area;
    count  = (extent / sizeof (ulong));

	p1 = (ulong *) area;
	for(i=0; i<count; i++)
	{
		j = i & 0x00000007;
		k = i & 0x0000001f;
		*p1 += (bitpattern[j] << k);
		p1++;
	}
	TEST_PRINT(("memory fill OK!\n"));
}


static int mem_test_read(ulong area, ulong max_addr, ulong bursts)
{
	ulong    i, j, k;
	ulong    count, extent;

	vu_long	*p1;
	vu_long	data;
	
	extent = max_addr - area;
    count  = (extent / sizeof (ulong));
	count = count ? count : 1;

	k = 0;
	data = 0;

	TEST_PRINT(("memory  read test start...!\n"));

	for (i = 0; TEST_BURSTS(i, bursts); i++)
	{
        p1 = (ulong *) area;
        for (j = 0; j < count; j++)
		{
            data += (*p1++);
			DBG_PRINT(k,MEM_TEST_R_PRINT_DELTA, ("\rmem read test:data(sum) = 0x%08x p1 = 0x%08x", data, p1));
        }
    }
	TEST_PRINT(("\nmemory  read test end!\n"));
	return 1;
}

static int mem_test_write(ulong area, ulong max_addr, ulong bursts)
{
	ulong    i, j, k, l;
	ulong    count, extent;
	ulong	val;

	vu_long	*p1;
	
	extent = max_addr - area;
    count  = (extent / sizeof (ulong));
	count = count ? count : 1;    

	l = 0;
	TEST_PRINT(("memory  write test start...!\n"));
	for (i = 0; TEST_BURSTS(i, bursts); i++)
	{
        p1 = (ulong *) area;
        for (j = 0; j < count; j++)
		{
			for (k = 0; k < sizeof(bitpattern)/sizeof(bitpattern[0]); k++)
			{
				for(val = bitpattern[k]; val != 0; val <<= 1)
				{
					*p1 = val;
					*p1 = ~val;
					DBG_PRINT(l, MEM_TEST_W_PRINT_DELTA, ("\rmem write test:p1 = 0x%08x, val = 0x%08x", p1, val));
				}
			}
			*p1++;
        }
    }
	TEST_PRINT(("\nmemory write test end!\n"));
	return 1;
}

static int mem_test_read_write(ulong area, ulong max_addr, ulong bursts)
{
	ulong    i, j, k, l;
	ulong    count, extent;
	ulong	val;
	ulong	readback;

	vu_long	*p1;
	
	extent = max_addr - area;
    count  = (extent / sizeof (ulong));
	count = count ? count : 1;

	l = 0;
	TEST_PRINT(("memory read write test start...!\n"));
	for (i = 0; TEST_BURSTS(i, bursts); i++)
	{
        p1 = (ulong *) area;
        for (j = 0; j < count; j++)
		{
			for (k = 0; k < sizeof(bitpattern)/sizeof(bitpattern[0]); k++)
			{
				for(val = bitpattern[k]; val != 0; val <<= 1)
				{
					*p1 = val;
					readback = *p1;
					if(readback != val)
					{
				     	printf ("FAILURE (data line): expected %08lx, actual %08lx\n",val, readback);
						return 0;
					}
					*p1 = ~val;
					readback = *p1;
					if(readback != ~val)
					{
				     	printf ("FAILURE (data line): expected %08lx, actual %08lx\n",val, readback);
						return 0;
					}
					DBG_PRINT(l, MEM_TEST_RW_PRINT_DELTA, ("\rmem read and write test:data = 0x%08x, p1= 0x%08x", val, p1));
				}
			}
			p1++;
        }
    }
	TEST_PRINT(("\nmemory read write test end!\n"));
	return 1;
}

int cmd_get_test_pattern(char* arg, int default_pattern)
{
	/* Check for a size specification .b, .w or .l.
	 */
	if (!strcmp(arg, "r") || !strcmp(arg, "read"))
	{
		return TEST_PATTERN_READ_ONLY;
	}
	else if (!strcmp(arg, "w") || !strcmp(arg, "write"))
	{
		return TEST_PATTERN_WRITE_ONLY;
	}
	else if (!strcmp(arg, "rw"))
	{
		return TEST_PATTERN_READ_WRITE;
	}
	else if (!strcmp(arg, "wr"))
	{
		return TEST_PATTERN_READ_WRITE;
	}
	else
	{
		return default_pattern;
	}
}

int do_mem_test (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	DECLARE_GLOBAL_DATA_PTR;

	ulong *start, *end, *end_max;
	int	size;
	int pattern;
	int bursts;
	int ret;

	if (argc < 2)
	{
		size = 4;
		pattern = TEST_PATTERN_WRITE_READ;
	}
	
	if ((size = cmd_get_data_size(argv[0], 4)) < 0)
		return 1;

	if (argc > 1)
	{
		pattern = cmd_get_test_pattern(argv[1], TEST_PATTERN_ALL);
	}

	if (argc > 2)
	{
		start = (ulong *)simple_strtoul(argv[2], NULL, 16);
		if (((ulong *)(gd->bd->bi_memstart + gd->bd->bi_memsize) < start) || ((ulong *)(gd->bd->bi_memstart) > start))
		{
			printf ("bad start_addr:0x%08x\n", start);
			return 1;
		}
	}
	else
	{
		start = (ulong *)gd->bd->bi_memstart;
	}

	if (argc > 3)
	{
		end = (ulong *)simple_strtoul(argv[3], NULL, 16);
		if (((gd->bd->bi_memstart + gd->bd->bi_memsize) < end) || start > end)
		{
			printf ("bad end_addr:0x%08x\n", end);
			return 1;
		}
	}
	else
	{
		if (TEST_PATTERN_READ_ONLY == pattern)
		{
			end = (ulong *)(gd->bd->bi_memstart + gd->bd->bi_memsize);
		}
		else
		{
			end = (ulong *)(gd->bd->bi_memstart + gd->bd->bi_memsize - UBOOT_RESERVED_MEM);
		}
	}

	if (argc > 4)
	{
		bursts = (ulong *)simple_strtoul(argv[4], NULL, 16);
	}
	else
	{
		bursts = 0; /* for ever */
	}
	

	printf("memory test start: test erea 0x%08x - 0x%08x\n", start, end);

	if ((TEST_PATTERN_READ_ONLY == pattern) && (end == (ulong *)(gd->bd->bi_memstart + gd->bd->bi_memsize)))
	{
		mem_fill_mem(start, (end -UBOOT_RESERVED_MEM));
	}
	else
	{
		mem_fill_mem(start, end);
	}

	ret = 1;
	switch(pattern)
	{
		case TEST_PATTERN_READ_ONLY:
			mem_test_read(start, end, bursts);
			break;
		case TEST_PATTERN_WRITE_ONLY:
			mem_test_write(start, end, bursts);
			break;
		case TEST_PATTERN_READ_WRITE:
			mem_test_read_write(start, end, bursts);
			break;
		default:
			while(ret)
			{
				if (ret)
					ret = mem_test_read(start, end, MEM_TEST_R_BURST);
				if (ret)
					ret = mem_test_write(start, end, (MEM_TEST_W_BURST));
				if (ret)
					ret = mem_test_read_write(start, end, MEM_TEST_RW_BURST);
			}
			break;
	}
	return 0;
}

U_BOOT_CMD(
	memtest ,    5,    1,     do_mem_test,
	"memtest - TP-LINK RAM test\n",
	"r/w addr_start addr_end [bursts]\n        - TP-LINK RAM test.\n        - e.g. \"memtest rw 80000000 81f00000\"\n"
);
#endif /* (CONFIG_COMMANDS & CFG_CMD_MEMORY) */

