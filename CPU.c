/**************************************************************/
/* CS/COE 1541
   just compile with gcc -o pipeline pipeline.c
   and execute using
   ./pipeline  /afs/cs.pitt.edu/courses/1541/short_traces/sample.tr     0
 ***************************************************************/

#include <stdio.h>
// mayhaps remove this later
#include <stdlib.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <string.h>
#include "CPU.h"

struct trace_item * buf_IF1_IF2   = NULL;
struct trace_item * buf_IF2_ID    = NULL;
struct trace_item * buf_ID_EX1    = NULL;
struct trace_item * buf_EX1_EX2   = NULL;
struct trace_item * buf_EX2_MEM1  = NULL;
struct trace_item * buf_MEM1_MEM2 = NULL;
struct trace_item * buf_MEM2_WB   = NULL;
struct trace_item * temp          = NULL;

int main(int argc, char **argv)
{
    struct trace_item *tr_entry;
    size_t size;
    char *trace_file_name;
    int predict_type = 0;
    int trace_view_on = 0;

    unsigned char t_type = 0;
    unsigned char t_sReg_a= 0;
    unsigned char t_sReg_b= 0;
    unsigned char t_dReg= 0;
    unsigned int t_PC = 0;
    unsigned int t_Addr = 0;

    unsigned int cycle_number = 0;

    init_buffers();
    init_branch_table();

    // used in WB stage
    int stallID = 0;

    // used for data hazards
    int stallEX1 = 0;

    if (argc == 1) {
        fprintf(stdout, "\nUSAGE: tv <trace_file> <switch - any character>\n");
        fprintf(stdout, "\n(switch) to turn on or off individual item view.\n\n");
        exit(0);
    }

    trace_file_name = argv[1];

    if (argc >= 3)
        trace_view_on = atoi(argv[2]) ;


    /*fprintf(stdout, "\n ** opening file %s\n", trace_file_name);*/

    trace_fd = fopen(trace_file_name, "rb");

    if (!trace_fd) {
        fprintf(stdout, "\ntrace file %s not opened.\n\n", trace_file_name);
        exit(0);
    }


    if(argc == 4)
        predict_type = atoi(argv[3]);

    trace_init();
    int i = 0;
    //for(i = 0; i<100; i++){
    while(1) {
        fflush(stdout);
        
		//WB Stage
		if(buf_MEM2_WB->type == ti_RTYPE || buf_MEM2_WB->type == ti_ITYPE || buf_MEM2_WB->type == ti_LOAD)
		{
			if(buf_IF2_ID->type != ti_JTYPE && buf_IF2_ID->type != ti_SPECIAL)
			{
				// stall for WB writing to register while ID is reading from register (stalls IF1, IF2, and ID)
				stallID = 1;
			}
		}
		write_instruction(buf_MEM2_WB, cycle_number, trace_view_on);
        
		//MEM2 Stage
		if(buf_MEM1_MEM2->type == ti_LOAD)
		{
			if(buf_MEM1_MEM2->dReg == buf_ID_EX1->sReg_a || buf_MEM1_MEM2->dReg == buf_ID_EX1->sReg_b)
			{
				// data hazard c
				stallEX1 = 1;
			}

		}
        memcpy(buf_MEM2_WB, buf_MEM1_MEM2, sizeof(struct trace_item));
		
		//MEM1 Stage
		if(buf_EX2_MEM1->type == ti_LOAD)
		{
			if(buf_ID_EX1->type == ti_LOAD)
			{
				if(buf_EX2_MEM1->dReg == buf_ID_EX1->sReg_a || buf_EX2_MEM1->dReg == buf_ID_EX1->sReg_b)
				{
					// If the destination register in MEM1 is a load instruction that writes to register X
					// and the ID stage is reading from register X, then we need to stall the instruction
					// in the ID stage to allow forwarding from MEM2/WB to ID/EX1
					// need to insert a NO-OP at EX1/EX2
					stallEX1 = 1;
				}
			}
		}

        memcpy(buf_MEM1_MEM2, buf_EX2_MEM1, sizeof(struct trace_item));

        //EX2 stage
		if(buf_EX1_EX2->type == ti_JTYPE)
		{ //stall IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
			//change pc to buf_EX1_EX2->Addr
			if(buf_ID_EX1->type == ti_NOP)
				stallEX1 = 3;
			else
				stallEX1 = 4;
		}
		else if(buf_EX1_EX2->type == ti_JRTYPE)
		{
			//stall IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
			//change pc to buf_ID_EX1->PC
			if(buf_ID_EX1->type == ti_NOP)
				stallEX1 = 3;
			else
				stallEX1 = 4;
		}
		else if(buf_EX1_EX2->type == ti_BRANCH)
		{

			//branch is taken
			int branch_taken = 0;
			int branch_count = 4;
			if(buf_ID_EX1->type == ti_NOP)
			{
				if(buf_IF2_ID->type == ti_NOP)
				{
					if(buf_EX1_EX2->Addr == buf_IF1_IF2->PC)
						branch_taken = 1;
					branch_count = 2;
				}
				else
				{
					if(buf_EX1_EX2->Addr == buf_IF2_ID->PC)
						branch_taken = 1;
					branch_count = 3;
				}
			}
			else
			{
				if(buf_EX1_EX2->Addr == buf_ID_EX1->PC)
					branch_taken = 1;
				branch_count = 4;
			}
			if(branch_taken == 1)
			{
				if(predict_type == 0)
				{
					//stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					//change pc to buf_EX1_EX2->Addr

					stallEX1 = branch_count;
				}
				else if(predict_type == 1)
				{
					// check 1 bit branch prediction table.
					// If branch was predicted taken last, do nothing, continue instructions.
					// If branch was predicted not taken last, change prediction to not taken,
					// stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					// change pc to buf_EX1_EX2->Addr
					int prediction = predict_branch(buf_EX1_EX2->Addr);
					if(prediction == -1)
					{
						update_table(buf_EX1_EX2->Addr, 1);
						stallEX1 = branch_count;
					}
					else if(prediction == 0)
					{
						update_table(buf_EX1_EX2->Addr, 1);
						stallEX1 = branch_count;
					}
					else {}
					// do nothing, predicted correctly
				}
				else
				{
					// check 2 bit branch prediction table.
					// If branch was predicted taken and the code is 11, do nothing, continue instructions.
					// If branch was predicted taken and the code is 10, change code to 11, continue instructions
					// if branch is predicted not taken and the code is 01, change prediction to taken, change code
					// to 11, stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					// If branch was predicted not taken and the code is 00, change code to 01, stall instructions in
					// IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					// change pc to buf_EX1_EX2->Addr
					int prediction = predict_branch(buf_EX1_EX2->Addr);
					if(prediction == -1)
					{
						// initially assume branch not taken, so if we don't find it and it's taken
						// we need to stall
						update_table(buf_EX1_EX2->Addr, 3);
						stallEX1 = branch_count;
					}
					else if(prediction == 3) {} // do nothing
					else if(prediction == 2)
						update_table(buf_EX1_EX2->Addr, 3);
					else if(prediction == 1)
					{
						update_table(buf_EX1_EX2->Addr, 3);
						stallEX1 = branch_count;
					}
					else if(prediction == 0)
					{
						update_table(buf_EX1_EX2->Addr, 1);
						stallEX1 = branch_count;
					}
				}
			}
			else
			{
				//branch is not taken
				if(predict_type == 0)
				{
					//Do nothing, keep going
				}
				else if(predict_type == 1)
				{
					//check 1 bit branch prediction table.
					//If branch was predicted taken last, change prediction to not taken, stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					//If branch was predicted not taken last, do nothing, continue instructions.
					int prediction = predict_branch(buf_EX1_EX2->Addr);
					if(prediction == -1)
						update_table(buf_EX1_EX2->Addr, 0);
					else if(prediction == 1)
					{
						stallEX1 = branch_count;
						update_table(buf_EX1_EX2->Addr, 0);
					}
					else {} // do nothing
				}
				else
				{
					//check 2 bit branch prediction table.
					//If branch was predicted taken and the code is 11, change code to 10, stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					//If branch was predicted taken and the code is 10, change code to 00, change prediction to not taken, stall instructions in IF1, IF2, ID, and EX1 for four cycles to simulate flushing instructions
					//if branch is predicted not taken and the code is 01, change code to 00, continue instructions
					//If branch was predicted not taken and the code is 00, do nothing, continue instructions
					int prediction = predict_branch(buf_EX1_EX2->Addr);
					if(prediction == -1)
					{
						/*stallEX1 = branch_count;*/
						update_table(buf_EX1_EX2->Addr, 0);
					}
					else if(prediction == 3)
					{
						update_table(buf_EX1_EX2->Addr, 2);
						stallEX1 = branch_count;
					}
					else if(prediction == 2)
					{
						update_table(buf_EX1_EX2->Addr, 0);
						stallEX1 = branch_count;
					}
					else if(prediction == 1)
					{
						update_table(buf_EX1_EX2->Addr, 0);
					}
					else if(prediction == 0) {} // do nothing
				}
			}

		}
		else if(buf_EX1_EX2->type == ti_LOAD || buf_EX1_EX2->type == ti_RTYPE || buf_EX1_EX2->type == ti_ITYPE)
		{
			if(buf_EX1_EX2->dReg == buf_ID_EX1->sReg_a || buf_EX1_EX2->dReg == buf_ID_EX1->sReg_b)
			{
				// data hazard a
				stallEX1 = 1;
			}
		}
        memcpy(buf_EX2_MEM1, buf_EX1_EX2, sizeof(struct trace_item));

        //EX1 stage
        if(stallEX1 == 0)
        {
            /*buf_EX1_EX2 = buf_ID_EX1;*/
            memcpy(buf_EX1_EX2, buf_ID_EX1, sizeof(struct trace_item));
        }
        else
        {
            /*buf_EX1_EX2 = malloc(sizeof(struct trace_item));*/
            memset(buf_EX1_EX2, 0, sizeof(struct trace_item));
            buf_EX1_EX2->type = ti_NOP;
        }

        //ID stage
        if(stallEX1 == 0 && stallID == 0)
        {
            //if(buf_ID_EX1 != NULL)

            /*buf_ID_EX1 = buf_IF2_ID;*/
            memcpy(buf_ID_EX1, buf_IF2_ID, sizeof(struct trace_item));
        }
        else if(stallID > 0 && stallEX1 == 0)
        {
            // PUT NO-OP in ID/EX1
            /*buf_ID_EX1 = malloc(sizeof(struct trace_item));*/
            memset(buf_ID_EX1, 0, sizeof(struct trace_item));
            buf_ID_EX1->type = ti_NOP;

        }
        else
        {
            // hold
        }

        //IF2 stage
        if(stallEX1 == 0 && stallID == 0)
        {
            memcpy(buf_IF2_ID, buf_IF1_IF2, sizeof(struct trace_item));
        }
        else
        {
            //just hold
        }

        //IF1 stage
        if(stallEX1 == 0 && stallID == 0)
        {
            size = trace_get_item(&temp);
            if (!size) {       /* no more instructions (trace_items) to simulate */
                printf("%u\n", cycle_number);
                break;
            }
            else{              /* parse the next instruction to simulate */
                memcpy(buf_IF1_IF2, temp, sizeof(struct trace_item));
			}
        }
        if(stallEX1 > 0)
        {
            stallEX1--;
        }
        if(stallID > 0)
        {
            //just hold, increment pc, decrement stalling variable
            stallID--;
        }
        cycle_number++;
    }

    trace_uninit();

    exit(0);
}

void init_buffers()
{
    buf_IF1_IF2   = calloc(1, sizeof(struct trace_item));
    buf_IF2_ID    = calloc(1, sizeof(struct trace_item));
    buf_ID_EX1    = calloc(1, sizeof(struct trace_item));
    buf_EX1_EX2   = calloc(1, sizeof(struct trace_item));
    buf_EX2_MEM1  = calloc(1, sizeof(struct trace_item));
    buf_MEM1_MEM2 = calloc(1, sizeof(struct trace_item));
    buf_MEM2_WB   = calloc(1, sizeof(struct trace_item));
}


int trace_get_item(struct trace_item **item)
{
    int n_items;

    if (trace_buf_ptr == trace_buf_end) {   /* if no more unprocessed items in the trace buffer, get new data  */
        n_items = fread(trace_buf, sizeof(struct trace_item), TRACE_BUFSIZE, trace_fd);
        if (!n_items) return 0;                         /* if no more items in the file, we are done */

        trace_buf_ptr = 0;
        trace_buf_end = n_items;                        /* n_items were read and placed in trace buffer */
    }

    *item = &trace_buf[trace_buf_ptr];      /* read a new trace item for processing */
    trace_buf_ptr++;

    if (is_big_endian()) {
        (*item)->PC = my_ntohl((*item)->PC);
        (*item)->Addr = my_ntohl((*item)->Addr);
    }

    return 1;
}

int get_address_bits(int address)
{
    /*int intermediate = address << 23;*/
    /*printf("INTERMEDIATE: %d\n", intermediate);*/
    /*printf("INTERMEDIATE: %d\n", intermediate >> 26);*/
    /*return intermediate >> 26;*/
    int intermediate = MASK & address;
    /*printf("intermediate: %d\n", intermediate);*/
    intermediate = intermediate >> 4;
    return intermediate;
}

void trace_uninit()
{
    free(trace_buf);
    fclose(trace_fd);
}

void trace_init()
{
    trace_buf = malloc(sizeof(struct trace_item) * TRACE_BUFSIZE);

    if (!trace_buf) {
        fprintf(stdout, "** trace_buf not allocated\n");
        exit(-1);
    }

    trace_buf_ptr = 0;
    trace_buf_end = 0;
}

uint32_t my_ntohl(uint32_t x)
{
    u_char *s = (u_char *)&x;
    return (uint32_t)(s[3] << 24 | s[2] << 16 | s[1] << 8 | s[0]);
}


int is_big_endian(void)
{
    union {
        uint32_t i;
        char c[4];
    } bint = { 0x01020304 };

    return bint.c[0] == 1;
}

// Add new branch prediction table entry - DAN MCGRATH
int update_table(int address, int prediction)
{
    int address_bits = get_address_bits(address);
    int index = address_bits % BRANCH_PREDICT_TABLE_SIZE;
    /*printf("INDEX: %d\n", index);*/
    branch_prediction_table[index].branch_prediction = prediction;
    branch_prediction_table[index].addr = address;
    return 0;
}

// predict branches - DAN MCGRATH
// returns -1 if not seen branch
int predict_branch(int address)
{
    address = get_address_bits(address);
    return branch_prediction_table[address % BRANCH_PREDICT_TABLE_SIZE].branch_prediction;
}

// initialize branch table - DAN MCGRATH
void init_branch_table()
{
    int i;
    for(i = 0; i < BRANCH_PREDICT_TABLE_SIZE; i++)
    {
        branch_prediction_table[i].branch_prediction = -1;
        branch_prediction_table[i].addr = -1;
    }
}

// SIMULATION OF A SINGLE CYCLE cpu IS TRIVIAL - EACH INSTRUCTION IS EXECUTED
// IN ONE CYCLE
void write_instruction(struct trace_item *tr_entry, unsigned int cycle_number, int trace_view_on)
{
    fflush(stdout);
    /*printf("hello world\n");*/
    /*printf("%d\n", tr_entry->type);*/
    if (trace_view_on) {/* print the executed instruction if trace_view_on=1 */
        switch(tr_entry->type) {
            case ti_NOP:
                printf("[cycle %d] NOP: \n",cycle_number) ;
                break;
            case ti_RTYPE:
                printf("[cycle %d] RTYPE:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(dReg: %d) \n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->dReg);
                break;
            case ti_ITYPE:
                printf("[cycle %d] ITYPE:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
                break;
            case ti_LOAD:
                printf("[cycle %d] LOAD:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(dReg: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->dReg, tr_entry->Addr);
                break;
            case ti_STORE:
                printf("[cycle %d] STORE:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
                break;
            case ti_BRANCH:
                printf("[cycle %d] BRANCH:",cycle_number) ;
                printf(" (PC: %x)(sReg_a: %d)(sReg_b: %d)(addr: %x)\n", tr_entry->PC, tr_entry->sReg_a, tr_entry->sReg_b, tr_entry->Addr);
                break;
            case ti_JTYPE:
                printf("[cycle %d] JTYPE:",cycle_number) ;
                printf(" (PC: %x)(addr: %x)\n", tr_entry->PC,tr_entry->Addr);
                break;
            case ti_SPECIAL:
                printf("[cycle %d] SPECIAL:",cycle_number) ;
                break;
            case ti_JRTYPE:
                printf("[cycle %d] JRTYPE:",cycle_number) ;
                printf(" (PC: %x) (sReg_a: %d)(addr: %x)\n", tr_entry->PC, tr_entry->dReg, tr_entry->Addr);
                break;
        }
    }
}


