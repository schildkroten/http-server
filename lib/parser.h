#ifndef PARSER_H
#define PARSER_H

#include <sys/types.h>
#include <stdlib.h>

typedef struct {
  size_t size;
  size_t max_size;
  size_t num_tokens;
  char **tokens;
} token_list;

int free_token_list(token_list *list);

typedef struct {
  token_list *start_line;
  token_list *headers;
  char *body;
} http_request_token_list;

int free_http_token_list(http_request_token_list *request);

int parse_http_request(char *buffer, http_request_token_list **request);

int generate_response(http_request_token_list *request, char **response);

#endif
