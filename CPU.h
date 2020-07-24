
#ifndef TRACE_ITEM_H
#define TRACE_ITEM_H

// this is tpts
enum trace_item_type {
	ti_NOP = 0,

	ti_RTYPE,
	ti_ITYPE,
	ti_LOAD,
	ti_STORE,
	ti_BRANCH,
	ti_JTYPE,
	ti_SPECIAL,
	ti_JRTYPE
};

struct trace_item {
	unsigned char type;			// see above
	unsigned char sReg_a;		// 1st operand
	unsigned char sReg_b;		// 2nd operand
	unsigned char dReg;			// dest. operand unsigned int PC;			
        unsigned int PC;        // program counter
	unsigned int Addr;			// mem. address
};

struct branch_item {
    int branch_prediction;
    int addr;
};

#endif


//size 32
#define MASK 0x000000f8
#define SHIFT_BITS 4
#define BRANCH_PREDICT_TABLE_SIZE 32


/*
//size 64
#define MASK 0x0000001f8
#define SHIFT_BITS 4
#define BRANCH_PREDICT_TABLE_SIZE 64
*/

/*
//size 128
#define MASK 0x000003f8
#define SHIFT_BITS 4
#define BRANCH_PREDICT_TABLE_SIZE 128
*/


static FILE *trace_fd;
static int trace_buf_ptr;
static int trace_buf_end;
static struct trace_item * trace_buf;

struct branch_item branch_prediction_table[BRANCH_PREDICT_TABLE_SIZE];

void write_instruction(struct trace_item *, unsigned int, int);
void init_branch_table();
int predict_branch(int);
int update_table(int, int);
int get_pc_bits(int);
int is_big_endian(void);
uint32_t my_ntohl(uint32_t);
void trace_init();
void trace_uninit();
int trace_get_item(struct trace_item **);
void init_buffers();

