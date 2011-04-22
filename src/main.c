#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "lexer.h"
#include "linked_list.h"

int main(int argc, char **argv)
{
	token *tok = NULL;
	FILE *fh = fopen(argv[1], "r");
	linked_list *tokens = ll_create();
	while (tok = get_next_token(fh)) {
		ll_insert_tail(tokens, tok);
	}

	ll_node *node = tokens->head;
	while (node) {
		char *str = token_to_string(node->data);
		puts(str);
		free(str);
		node = node->next;
	}

	return 0;
}
