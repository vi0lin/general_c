#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_OUTPUT 4096

// Structure to hold parser state
typedef struct {
    int in_list;
    int list_level;
    int in_code_block;
    char* output;
    size_t output_size;
    size_t output_pos;
} ParserState;

// Initialize parser state
static void init_parser(ParserState* state) {
    state->in_list = 0;
    state->list_level = 0;
    state->in_code_block = 0;
    state->output_size = MAX_OUTPUT;
    state->output = (char*)malloc(state->output_size);
    state->output[0] = '\0';
    state->output_pos = 0;
}

// Append string to output buffer
static void append_output(ParserState* state, const char* str) {
    size_t len = strlen(str);
    if (state->output_pos + len + 1 >= state->output_size) {
        state->output_size *= 2;
        state->output = (char*)realloc(state->output, state->output_size);
    }
    strcpy(state->output + state->output_pos, str);
    state->output_pos += len;
}

// Trim whitespace from start and end of string
static char* trim(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Check if line is a heading
static int is_heading(const char* line, int* level) {
    int count = 0;
    while (line[count] == '#') count++;
    if (count > 0 && count <= 6 && isspace(line[count])) {
        *level = count;
        return 1;
    }
    return 0;
}

// Check if line is a list item
static int is_list_item(const char* line, int* level) {
    int indent = 0;
    while (line[indent] == ' ' || line[indent] == '\t') indent++;
    if (line[indent] == '*' || line[indent] == '-' || isdigit(line[indent])) {
        *level = indent / 2 + 1;
        return 1;
    }
    return 0;
}

// Process inline formatting (bold and italic)
static void process_inline(ParserState* state, const char* text) {
    char buffer[MAX_LINE];
    size_t i = 0, j = 0;
    int in_bold = 0, in_italic = 0;

    while (text[i]) {
        if (text[i] == '*' && text[i+1] == '*') {
            append_output(state, in_bold ? "</strong>" : "<strong>");
            in_bold = !in_bold;
            i += 2;
        } else if (text[i] == '*' && !in_italic) {
            append_output(state, in_italic ? "</em>" : "<em>");
            in_italic = !in_italic;
            i++;
        } else if (text[i] == '`') {
            append_output(state, "<code>");
            i++;
            while (text[i] && text[i] != '`') {
                buffer[j++] = text[i++];
            }
            buffer[j] = '\0';
            append_output(state, buffer);
            append_output(state, "</code>");
            i++;
            j = 0;
        } else {
            buffer[j++] = text[i++];
            if (text[i] == '\0') {
                buffer[j] = '\0';
                append_output(state, buffer);
                j = 0;
            }
        }
    }
}

// int is_image_file(const char *str) {
//   const char *exts[] = {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".svg", NULL};
//   for (int j = 0; exts[j]; j++) {
//     if (strstr(str, exts[j])) return 1;
//   }
//   return 0
// }
//
// static int is_url_start(const char *s) {
//   return strcmp(s, "http://", 7) == 0 ||
//          strcmp(s, "https://", 8) == 0 ||
//          strcmp(s, "www.", 4) == 0;
// }
//
// // Main processing function
// static int process_line_charwise(const char *line) {
//     char buffer[MAX_LINE] = {0};
//     size_t len = strlen(line);
//     size_t i = 0, out = 0;
//     while (i < len && out < MAX_LINE-1) {
//         // === URL detection ===
//         if (is_url_start(line + i)) {
//             size_t url_start = i;
//             // Find end of URL (space, quote, <, >, etc.)
//             while (i < len && !isspace(line[i]) &&
//                    line[i] != '<' && line[i] != '>' &&
//                    line[i] != '"' && line[i] != '\'' &&
//                    line[i] != ')' && line[i] != ']') {
//                 i++;
//             }
//             // Output as Markdown link
//             buffer[out++] = '[';
//             strncpy(buffer + out, line + url_start, i - url_start);
//             out += i - url_start;
//             buffer[out++] = ']';
//             buffer[out++] = '(';
//             strncpy(buffer + out, line + url_start, i - url_start);
//             out += i - url_start;
//             buffer[out++] = ')';
//             continue;
//         }
//         // === Image detection (simple heuristic) ===
//         if (i + 4 < len) {
//             const char *ext = line + i;
//             if (strstr(ext, ".png") || strstr(ext, ".jpg") ||
//                 strstr(ext, ".jpeg") || strstr(ext, ".gif") ||
//                 strstr(ext, ".webp") || strstr(ext, ".svg")) {
//
//                 // Check if it's likely a standalone image path
//                 if ((i == 0 || isspace(line[i-1])) &&
//                     (i+4 == len || isspace(line[i+4]) || line[i+4] == '\0')) {
//                     // Output as Markdown image
//                     snprintf(buffer + out, MAX_LINE - out,
//                             "![image](%.*s)", (int)(strchr(ext, ' ') ? strchr(ext, ' ')-ext : len-i), ext);
//                     // Advance i to end of filename
//                     while (i < len && !isspace(line[i])) i++;
//                     out = strlen(buffer);
//                     continue;
//                 }
//             }
//         }
//         // Normal character
//         buffer[out++] = line[i++];
//     }
//     buffer[out] = '\0';
//     printf("%s\n", buffer);
//     append_output(state, buffer);
// }
void process_line_url(ParserState* state, const char *line) {
    char buffer[8192] = {0};
    size_t i = 0, out = 0;
    size_t len = strlen(line);

    while (i < len && out < sizeof(buffer)-100) {

        // === IMAGE: ![alt text](image.jpg) ===
        if (i+2 < len && line[i] == '!' && line[i+1] == '[') {
            size_t alt_start = i + 2;
            char *alt_end = strchr(line + alt_start, ']');
            if (alt_end && alt_end[1] == '(') {
                size_t url_start = (alt_end - line) + 2;
                char *url_end = strchr(line + url_start, ')');

                if (url_end) {
                    // Output <img> tag
                    buffer[out++] = '<';
                    buffer[out++] = 'i';
                    buffer[out++] = 'm';
                    buffer[out++] = 'g';
                    buffer[out++] = ' ';
                    buffer[out++] = 's';
                    buffer[out++] = 'r';
                    buffer[out++] = 'c';
                    buffer[out++] = '=';
                    buffer[out++] = '"';

                    strncpy(buffer + out, line + url_start, url_end - line - url_start);
                    out += url_end - line - url_start;
                    buffer[out++] = '"';
                    buffer[out++] = ' ';
                    buffer[out++] = 'a';
                    buffer[out++] = 'l';
                    buffer[out++] = 't';
                    buffer[out++] = '=';
                    buffer[out++] = '"';

                    strncpy(buffer + out, line + alt_start, alt_end - line - alt_start);
                    out += alt_end - line - alt_start;

                    strcpy(buffer + out, "\">");
                    out += 2;

                    i = url_end - line + 1;
                    continue;
                }
            }
        }

        // === LINK: [text](url) ===
        else if (line[i] == '[') {
            char *bracket_end = strchr(line + i + 1, ']');
            if (bracket_end && bracket_end[1] == '(') {
                size_t text_start = i + 1;
                size_t text_len = bracket_end - line - i - 1;

                size_t url_start = (bracket_end - line) + 2;
                char *url_end = strchr(line + url_start, ')');

                if (url_end) {
                    // Output <a href="url">text</a>
                    strcpy(buffer + out, "<a href=\"");
                    out += 9;

                    strncpy(buffer + out, line + url_start, url_end - line - url_start);
                    out += url_end - line - url_start;

                    strcpy(buffer + out, "\">");
                    out += 2;

                    strncpy(buffer + out, line + text_start, text_len);
                    out += text_len;

                    strcpy(buffer + out, "</a>");
                    out += 4;

                    i = url_end - line + 1;
                    continue;
                }
            }
        }

        // Normal character
        buffer[out++] = line[i++];
    }

    // buffer[out] = '\0';
    buffer[out] = '\n';
    // printf("%s\n", buffer);
    append_output(state, buffer);
}

// Process a single line of Markdown
static void process_line(ParserState* state, char* line) {
    char* trimmed = trim(line);
    if (strlen(trimmed) == 0) {
        if (!state->in_code_block && !state->in_list) {
            append_output(state, "<p></p>\n");
        }
        return;
    }

    // Handle code blocks
    if (strncmp(trimmed, "```", 3) == 0) {
        state->in_code_block = !state->in_code_block;
        append_output(state, state->in_code_block ? "<pre><code>" : "</code></pre>\n");
        return;
    }

    if (state->in_code_block) {
        append_output(state, trimmed);
        append_output(state, "\n");
        return;
    }

    // Handle headings
    int level;
    if (is_heading(trimmed, &level)) {
        char tag[4];
        sprintf(tag, "h%d", level);
        append_output(state, "<");
        append_output(state, tag);
        append_output(state, ">");
        process_inline(state, trimmed + level + 1);
        append_output(state, "</");
        append_output(state, tag);
        append_output(state, ">\n");
        return;
    }

    // Handle list items
    int list_level;
    if (is_list_item(trimmed, &list_level)) {
        while (state->list_level < list_level) {
            append_output(state, "<ul>\n");
            state->list_level++;
        }
        while (state->list_level > list_level) {
            append_output(state, "</ul>\n");
            state->list_level--;
        }
        if (!state->in_list) {
            append_output(state, "<ul>\n");
            state->in_list = 1;
            state->list_level = list_level;
        }
        append_output(state, "<li>");
        process_inline(state, trimmed + (list_level * 2));
        append_output(state, "</li>\n");
        return;
    }

    // Close any open lists
    if (state->in_list) {
        while (state->list_level > 0) {
            append_output(state, "</ul>\n");
            state->list_level--;
        }
        state->in_list = 0;
    }

    // Handle Image
    // if (is_url_start(line)) {
      // char trimmed[MAX_LINE];
      // sscanf(line, "%s", trimmed);
      // printf("![%s](%s)\n", trimmed, trimmed);
      // append_output(state, trimmed);
      // return;
    //}
    process_line_url(state, line);

    // Handle paragraphs
    append_output(state, "<p>");
    process_inline(state, trimmed);
    append_output(state, "</p>\n");
}

int markdown() {
    ParserState state;
    init_parser(&state);

    // Output HTML header with github-markdown-css styling
    append_output(&state,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<style>\n"
        ".markdown-body {\n"
        "    box-sizing: border-box;\n"
        "    min-width: 200px;\n"
        "    max-width: 980px;\n"
        "    margin: 0 auto;\n"
        "    padding: 45px;\n"
        "}\n"
        "@media (max-width: 767px) {\n"
        "    .markdown-body {\n"
        "        padding: 15px;\n"
        "    }\n"
        "}\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "<article class=\"markdown-body\">\n"
    );

    // Process input
    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, stdin)) {
        line[strcspn(line, "\n")] = '\0';
        process_line(&state, line);
    }

    // Close any open lists
    if (state.in_list) {
        while (state.list_level > 0) {
            append_output(&state, "</ul>\n");
            state.list_level--;
        }
    }

    // Close code block if open
    if (state.in_code_block) {
        append_output(&state, "</code></pre>\n");
    }

    // Output HTML footer
    append_output(&state, "</article>\n</body>\n</html>\n");

    // Print output
    printf("%s", state.output);

    // Cleanup
    free(state.output);
    return 0;
}
