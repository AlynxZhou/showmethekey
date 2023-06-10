#include "util.h"
#include <string.h>

void escape_string(const char *input, char *output, size_t output_size)
{
	size_t input_len = strlen(input);
	size_t j = 0;

	for (size_t i = 0; i < input_len && j + 2 < output_size; i++) {
		switch (input[i]) {
		case ' ':
		case '\"':
			output[j++] = ' ';
			output[j++] = input[i];
			break;
		case '\n':
			output[j++] = 'n';
			output[j++] = ' ';
			break;
		case '\r':
			output[j++] = ' ';
			output[j++] = 'r';
			break;
		case '\t':
			output[j++] = 't';
			output[j++] = ' ';
			break;
		default:
			output[j++] = input[i];
			break;
		}
	}

	output[j] = '\0';
}
