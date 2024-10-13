
#include "strings.h"


void reader_path(vec_str_t& lines, const char* filename, const char* section, size_t max_len) {
    FILE* file;
    const int buf_size = 1024;
    char line[buf_size];
    bool start = false;
    file = fopen(filename, "r");
    if (!file) {
        return;
    }
    while (fgets(line, buf_size, file) != NULL) {
        line[strlen(line) - 1] = '\0';
        if (!start && stricmp(line, section) == 0) {
            start = true;
        } else if (start && line[0] == '#') {
            break;
        } else if (start && line[0] != ';') {
            char* p = strstrip(line);
            // Truncate long lines to max_len (including null)
            if (strlen(p) > 0 && p[0] != ';') {
                if (strlen(p) >= max_len) {
                    p[max_len - 1] = '\0';
                }
                lines.push_back(p);
            }
        }
    }
    fclose(file);
}

void reader_file(vec_str_t& lines, const char* filename, const char* section, size_t max_len) {
    const int buf_size = 1024;
    char filename_txt[buf_size];
    snprintf(filename_txt, buf_size, "%s.txt", filename);
    strlwr(filename_txt);
    reader_path(lines, filename_txt, section, max_len);
}

void parse_format_args(char* buf, const char* src, int args_count, size_t max_len) {
    int num = count_format_args(src);
    if (num >= 0 && num <= args_count) {
        strncpy(buf, src, max_len);
        buf[max_len-1] = '\0';
    }
}

int count_format_args(const char* buf) {
    int num = 0;
    int len = strlen(buf);
    int i;

    for (i = 0; i < len - 1; i++) {
        if (buf[i] == '%' && buf[i + 1] != '%') {
            if (buf[i + 1] != 'd' && buf[i + 1] != 'i' && buf[i + 1] != 'u') {
                return -1;
            }
            num++;
        }
    }
    return num;
}

char* parse_str(char* buf, size_t len, const char* s1, const char* s2, const char* s3, const char* s4) {
    buf[0] = '\0';
    strncat(buf, s1, len);
    if (s2) {
        strncat(buf, " ", 2);
        strncat(buf, s2, len);
    }
    if (s3) {
        strncat(buf, " ", 2);
        strncat(buf, s3, len);
    }
    if (s4) {
        strncat(buf, " ", 2);
        strncat(buf, s4, len);
    }
    // strlen count does not include the first null character.
    if (strlen(buf) > 0 && strlen(buf) < len) {
        return buf;
    }
    return NULL;
}

char* strtrail(char* s) {
    size_t size;
    char* end;
    size = strlen(s);
    if (!size) {
        return s;
    }
    end = s + size - 1;
    while (end >= s && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';
    return s;
}

char* strstrip(char* s) {
    size_t size;
    char* end;
    size = strlen(s);
    if (!size) {
        return s;
    }
    end = s + size - 1;
    while (end >= s && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';
    while (*s && isspace(*s)) {
        s++;
    }
    return s;
}

