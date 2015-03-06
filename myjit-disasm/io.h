
#define OUTPUT_BYTES_PER_LINE   (8)

void output_color_normal();
void output_color_white();
void output_color_yellow();
void output_code(unsigned long addr, unsigned char *data, int size, char *text);

void input_init();
void input_free();
void input_clear();
unsigned char *input_buffer();
int input_size();
int input_read();
void input_convert();
