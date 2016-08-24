#ifndef _INDEXTREE_INNER_H_
#define _INDEXTREE_INNER_H_

#ifndef _LINUX
#define _LINUX
#endif

typedef unsigned  char   u8;
typedef unsigned  short u16;
typedef unsigned  int   u32;
typedef unsigned  int   u4char_t;

typedef unsigned int address_t;

#include "endian_number.h"

#define INXTREE_BLOCK	256
#define INXTREE_INVALID_ADDR 0xFFFFFFFF
#define INXTREE_ADDR_MAX  0x7FFFFFFF

#define F_LOCSTRINX  0x80000000
#define F_DUPLICATEINX  0x01

#define BP(a, b)	(b-a+1)

#define INDEX_BLOCK_NR   2

#define STRINX_WORDS_MAX 3
#define STRINX_DEPTH_MAX 5
#define CHRINX_DEPTH_MIN 1
#define STRINX_LEN_MAX   60  /* a string index must be within a block. */

#define INXTREE_UTF8    1
#define INXTREE_UTF16   2
#define INXTREE_GB2312  3

struct inxtree_header {
	u8 magic        [ BP(1, 2)   ];
	u8 h_version  	[ BP(3, 3)   ];
	u8 d_date       [ BP(4, 7)   ];
	u8 d_identi     [ BP(8, 67)  ];
	u8 d_version    [ BP(68, 69) ];
	u8 d_entries	[ BP(70, 73) ];
    u8 d_coding     [ BP(74, 74) ];
	u8 loc_chrindex	[ BP(75, 75) ];
	u8 loc_strindex [ BP(76, 79) ];
	u8 loc_data     [ BP(80, 83) ];
    u8 flags        [ BP(84, 84) ];
	u8 custom       [ BP(85, 256)];
};

struct inxtree_chrindex {
	u8 wchr         [ BP(1, 4) ];
  	u8 location  	[ BP(5, 8) ];
	u8 len_content 	[ BP(9, 10)];
};

struct inxtree_strindex {
	u8 location     [ BP(1, 4) ];
	u8 len_str      [ BP(5, 5) ];
  	u8 keystr       [1];
};

struct inxtree_dataitem {
    u16  len_data;
    u8  *ptr_data;
};

#define INXTREE_BLOCK_NR(pos)  (pos/INXTREE_BLOCK+1)

#define inxtree_write_u32	endian_write_u32_le

#define inxtree_write_u16   endian_write_u16_le

#define inxtree_read_u32	endian_read_u32_le

#define inxtree_read_u16	endian_read_u16_le

#endif
