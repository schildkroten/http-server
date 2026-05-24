#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  size_t size;
  size_t num_tokens;
  char **tokens;
} token_list ;

int free_token_list(token_list *list) {
  if (list == NULL) return -1;

  for (int i = 0; i < list->num_tokens; i++) {
    free(list->tokens[i]);
  }
  free(list->tokens);

  list->size = 0;
  list->num_tokens = 0;

  return 0;
}

int print_token_list(token_list *list) {
  if (list == NULL) return -1;

  for (int i = 0; i < list->num_tokens; i++) {
    printf("token: %s, length: %lu\n", list->tokens[i], strlen(list->tokens[i]));
  }

  return 0;
}

int add_token(char *token, size_t token_len, token_list *list) {
  if (list == NULL) return -1;

  list->size += token_len;
  list->num_tokens++;
  list->tokens = realloc(list->tokens, sizeof(char *) * (list->num_tokens + 1));
  list->tokens[list->num_tokens - 1] = strdup(token);

  return 0;
}

char *get_token(char *buffer, const char *delimiter, char **saveptr) {
  if (strlen(buffer) == 0) return NULL;

  char *buffer_copy;
  if ((buffer_copy = strdup(buffer)) == NULL) return NULL;

  char *token_end;
  if ((token_end = strstr(buffer_copy, delimiter)) == NULL) {
    *saveptr = strchr(buffer, '\0');
    return buffer_copy;
  }

  size_t token_length = token_end - buffer_copy;
  char *token;
  if ((token = malloc(token_length + 1)) == NULL) return NULL;

  memcpy(token, buffer_copy, token_length);

  token[token_length] = '\0';

  if (saveptr != NULL) *saveptr = strstr(buffer, delimiter);

  return token;
}

int tokenize_buffer(char *buffer, const char *delimiter, const char *end_token, token_list **list) {
  if (list == NULL) return -1;

  *list = malloc(sizeof(token_list));

  char *saveptr = buffer;

  char *endptr;
  if ((endptr = strstr(saveptr, end_token)) == NULL) return -1;

  char *token;
  while ((token = get_token(saveptr, delimiter, &saveptr)) != NULL) {
    add_token(token, strlen(token), *list);
    if (saveptr == endptr) break;
    saveptr += strlen(delimiter) == 0 ? 1 : strlen(delimiter);
  }

  return 0;
}

typedef struct {
  token_list *start_line;
  token_list *headers;
  token_list *header_values;
  char *body;
} http_request_token_list;

int free_http_token_list(http_request_token_list *request) {
  if (request == NULL) return -1;

  free_token_list(request->start_line);
  free_token_list(request->headers);
  free_token_list(request->header_values);
  free(request->body);

  return 0;
}

int get_header_value(const char *target_header, http_request_token_list *request, char **result) {
  if (request == NULL) return -1;

  int header_index;
  for (header_index = 0; header_index < request->headers->num_tokens; header_index++) {
    if (strcmp(request->headers->tokens[header_index], target_header) == 0) break;
    continue;
  }

  if (request->headers->tokens[header_index] == NULL) return 0;

  if (result != NULL) {
    *result = strdup(request->header_values->tokens[header_index]);
  }

  return 1;
}

int get_header_value_array(const char *target_header, http_request_token_list *request, token_list **list) {
  if (request == NULL) return -1;

  char *header_value;
  if (get_header_value(target_header, request, &header_value) == 0) return -1;

  if (tokenize_buffer(header_value, ",", "\0", list) == -1) return -1;

  return 0;
}

int parse_http_request(char *buffer, http_request_token_list **request) {
  if (request == NULL) return -1;

  *request = malloc(sizeof(http_request_token_list));

  token_list *list;
  if (tokenize_buffer(buffer, "\r\n", "\r\n\r\n", &list) == -1) return -1;
  
  if (tokenize_buffer(list->tokens[0], " ", "\0", &(*request)->start_line) == -1) return -1;
  
  printf("\nSTART LINE\n\n");
  if (print_token_list((*request)->start_line) == -1) return -1;

  (*request)->headers = malloc(sizeof(token_list));
  (*request)->header_values = malloc(sizeof(token_list));
  char *token;
  char *saveptr;

  for (int i = 1; i < list->num_tokens; i++) {
    if ((token = get_token(list->tokens[i], ": ", &saveptr)) == NULL) return -1;
    if (add_token(token, strlen(token), (*request)->headers) == -1) return -1;
    saveptr += 2;
    if (add_token(saveptr, strlen(saveptr), (*request)->header_values) == -1) return -1;
  }

  printf("\nHEADERS\n\n");
  if (print_token_list((*request)->headers) == -1) return -1;

  printf("\nHEADER VALUES\n\n");
  if (print_token_list((*request)->header_values) == -1) return -1;

  char *content_length;
  if (get_header_value("Content-Length", *request, &content_length)) {
    int body_length = atoi(content_length);
    char *body_start = strstr(buffer, "\r\n\r\n") + 4;
    
    (*request)->body = malloc(body_length + 1);

    memcpy(
      (*request)->body,
      body_start,
      body_length
    );

    (*request)->body[body_length] = '\0';

    printf("\nBODY\n\n");
    printf("%s\n", (*request)->body);
  }

  if (free_token_list(list) == -1) return -1;

  return 0;
}

int generate_response(http_request_token_list *request, char **buffer) {
  if (request == NULL) return -1;
  if (strcmp(request->start_line->tokens[0], "GET") != 0) return -1;
  if (strcmp(request->start_line->tokens[2], "HTTP/1.1") != 0) return -1;

  FILE *fileptr;
  char *status;
  char *headers;
  char *body;

  char *path = strcmp(request->start_line->tokens[1], "/") != 0 ?
    request->start_line->tokens[1] + 1 :
    "index.html";

  if ((fileptr = fopen(path, "rb")) != NULL) {
    status = "200 OK";

    fseek(fileptr, 0, SEEK_END);
    size_t file_size = ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);

    if ((body = malloc(file_size + 1)) == NULL) return -1;
    fread(body, file_size, 1, fileptr);

    token_list *accepted_files;
    if (get_header_value_array("Accept", request, &accepted_files) == -1) return -1;

    size_t headers_size = strlen(accepted_files->tokens[0]) + snprintf(NULL, 0, "%lu", file_size) + 59;
    if ((headers = malloc(headers_size)) == NULL) return -1;

    snprintf(
      headers,
      headers_size,
      "Content-Type: %s\r\n"
      "Content-Length: %lu\r\n"
      "Connection: keep-alive\r\n",
      accepted_files->tokens[0],
      file_size
    );
  } else {
    status = "404 NOT FOUND";
    headers = "Connection: close\r\n";
    body = "";
  }

  size_t size = strlen(status) + strlen(headers) + strlen(body) + 14;
  *buffer = malloc(size);

  snprintf(
    *buffer,
    size,
    "HTTP/1.1 %s\r\n%s\r\n%s",
    status,
    headers,
    body
  );

  if (fileptr != NULL) {
    free(headers);
    free(body);
    fclose(fileptr);
  }

  return 0;
}
