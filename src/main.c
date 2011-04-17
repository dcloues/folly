#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lexer.h"

int main(int argc, char **argv)
{
	/*token *tok = NULL;*/
	/*while (tok = get_next_token(stdin)) {*/
		/*char *str = token_to_string(tok);*/
		/*puts(str);*/
		/*free(str);*/
	/*}*/

	token t;
	t.type = number;
	t.value.number = 5;
	char *str = token_to_string(&t);
	printf("Token: %s\n", str);
	free(str);
	str = NULL;

	t.type = string;
	t.value.string = "La la la, this is a string";
	str = token_to_string(&t);
	printf("Token: %s\n", str);
	free(str);
	str = NULL;

	return 0;
}
