typedef enum { identifier, number, string } token_type;

typedef union {
	char *string;
	int number;
} value;

typedef struct {
	token_type type;
	value value;
} token;

const char* token_type_string(token *token);
char* token_to_string(token *token);

