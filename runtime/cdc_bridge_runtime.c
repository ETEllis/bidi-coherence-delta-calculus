#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRIDGE64_ROWS 64
#define LINE_MAX_BYTES 1024

typedef struct {
    char witness[64];
    char dyadic[7];
    char triadic[4];
    int index;
    int seen;
} BridgeRow;

static void fail(const char *message) {
    fprintf(stderr, "cdc-bridge-runtime: %s\n", message);
    exit(1);
}

static int starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static void trim_newline(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void first_token_after(const char *line, const char *prefix, char *out, size_t out_size) {
    const char *p = line + strlen(prefix);
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p)) {
        if (i + 1 >= out_size) {
            fail("token too long");
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
}

static int read_attr(const char *line, const char *key, char *out, size_t out_size) {
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char *p = strstr(line, needle);
    size_t i = 0;
    if (!p) {
        return 0;
    }
    p += strlen(needle);
    while (*p && !isspace((unsigned char)*p)) {
        if (i + 1 >= out_size) {
            fail("attribute too long");
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

static int dyadic_index(const char *dyadic) {
    int value = 0;
    if (strlen(dyadic) != 6) {
        return -1;
    }
    for (size_t i = 0; i < 6; i++) {
        if (dyadic[i] != '0' && dyadic[i] != '1') {
            return -1;
        }
        value = (value << 1) | (dyadic[i] - '0');
    }
    return value;
}

static int triadic_index(const char *triadic) {
    int value = 0;
    if (strlen(triadic) != 3) {
        return -1;
    }
    for (size_t i = 0; i < 3; i++) {
        if (triadic[i] < '0' || triadic[i] > '3') {
            return -1;
        }
        value = (value << 2) | (triadic[i] - '0');
    }
    return value;
}

static void expected_triadic(int index, char out[4]) {
    out[0] = (char)('0' + ((index >> 4) & 3));
    out[1] = (char)('0' + ((index >> 2) & 3));
    out[2] = (char)('0' + (index & 3));
    out[3] = '\0';
}

static void expected_dyadic(int index, char out[7]) {
    for (int i = 5; i >= 0; i--) {
        out[5 - i] = ((index >> i) & 1) ? '1' : '0';
    }
    out[6] = '\0';
}

static void load_bridge64(const char *path, BridgeRow rows[BRIDGE64_ROWS]) {
    FILE *fp = fopen(path, "r");
    char line[LINE_MAX_BYTES];
    int dyadic_seen[BRIDGE64_ROWS] = {0};
    int triadic_seen[BRIDGE64_ROWS] = {0};
    int count = 0;

    if (!fp) {
        fail("could not open bridge file");
    }

    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        rows[i].seen = 0;
        rows[i].index = -1;
        rows[i].witness[0] = '\0';
        rows[i].dyadic[0] = '\0';
        rows[i].triadic[0] = '\0';
    }

    while (fgets(line, sizeof(line), fp)) {
        char witness[64];
        char dyadic[16];
        char triadic[16];
        char index_text[16];
        int index;
        int d_index;
        int t_index;
        char expected_t[4];
        char expected_d[7];

        trim_newline(line);
        if (!starts_with(line, "witness bridge64-")) {
            continue;
        }

        first_token_after(line, "witness ", witness, sizeof(witness));
        if (!read_attr(line, "dyadic", dyadic, sizeof(dyadic)) ||
            !read_attr(line, "triadic", triadic, sizeof(triadic)) ||
            !read_attr(line, "index", index_text, sizeof(index_text))) {
            fail("bridge64 witness missing dyadic, triadic, or index attribute");
        }

        index = atoi(index_text);
        if (index < 0 || index >= BRIDGE64_ROWS) {
            fail("bridge64 index out of range");
        }
        if (rows[index].seen) {
            fail("duplicate bridge64 index");
        }

        d_index = dyadic_index(dyadic);
        t_index = triadic_index(triadic);
        if (d_index < 0 || t_index < 0) {
            fail("invalid bridge64 dyadic or triadic code");
        }
        if (d_index != index || t_index != index) {
            fail("bridge64 row does not match its numeric index");
        }
        if (dyadic_seen[d_index] || triadic_seen[t_index]) {
            fail("bridge64 duplicate dyadic or triadic code");
        }

        expected_triadic(index, expected_t);
        expected_dyadic(index, expected_d);
        if (strcmp(triadic, expected_t) != 0 || strcmp(dyadic, expected_d) != 0) {
            fail("bridge64 row does not match canonical codebook");
        }

        snprintf(rows[index].witness, sizeof(rows[index].witness), "%s", witness);
        snprintf(rows[index].dyadic, sizeof(rows[index].dyadic), "%s", dyadic);
        snprintf(rows[index].triadic, sizeof(rows[index].triadic), "%s", triadic);
        rows[index].index = index;
        rows[index].seen = 1;
        dyadic_seen[d_index] = 1;
        triadic_seen[t_index] = 1;
        count++;
    }
    fclose(fp);

    if (count != BRIDGE64_ROWS) {
        fail("bridge64 file does not contain exactly 64 rows");
    }
    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        if (!rows[i].seen) {
            fail("bridge64 table has an unfilled row");
        }
    }
}

static void trits_to_dyadic(const char *trits, char out[7]) {
    if (strlen(trits) != 6) {
        fail("trit projection expects exactly six trits");
    }
    for (size_t i = 0; i < 6; i++) {
        char c = trits[i];
        if (c == '+' || c == '-') {
            out[i] = '1';
        } else if (c == '0' || c == 'o' || c == 'O') {
            out[i] = '0';
        } else {
            fail("trits must use '+', '-', '0', or 'o'");
        }
    }
    out[6] = '\0';
}

static unsigned long long pow2_int(int exp) {
    if (exp < 0 || exp > 62) {
        fail("arity too large for this runtime");
    }
    return 1ULL << exp;
}

static void cmd_verify(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("bridge64 ok rows=64 bijection=dyadic<->triadic source=%s\n", path);
}

static void cmd_lookup_dyadic(const char *path, const char *dyadic) {
    BridgeRow rows[BRIDGE64_ROWS];
    int index = dyadic_index(dyadic);
    if (index < 0) {
        fail("lookup-dyadic expects six binary digits");
    }
    load_bridge64(path, rows);
    printf("dyadic=%s index=%d triadic=%s witness=%s\n",
           rows[index].dyadic, index, rows[index].triadic, rows[index].witness);
}

static void cmd_lookup_triadic(const char *path, const char *triadic) {
    BridgeRow rows[BRIDGE64_ROWS];
    int index = triadic_index(triadic);
    if (index < 0) {
        fail("lookup-triadic expects three base-4 digits");
    }
    load_bridge64(path, rows);
    printf("triadic=%s index=%d dyadic=%s witness=%s\n",
           rows[index].triadic, index, rows[index].dyadic, rows[index].witness);
}

static void cmd_project_trits(const char *path, const char *trits, const char *window) {
    BridgeRow rows[BRIDGE64_ROWS];
    char dyadic[7];
    int index;
    load_bridge64(path, rows);
    trits_to_dyadic(trits, dyadic);
    index = dyadic_index(dyadic);
    printf("trace-window=%s trits=%s occupancy=%s index=%d triadic=%s witness=%s\n",
           window, trits, dyadic, index, rows[index].triadic, rows[index].witness);
}

static void cmd_grid(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("bridge64-grid source=%s\n", path);
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int i = r * 8 + c;
            printf("%02d:%s/%s%s", i, rows[i].dyadic, rows[i].triadic, c == 7 ? "" : "  ");
        }
        printf("\n");
    }
}

static void cmd_grid_svg(const char *path) {
    BridgeRow rows[BRIDGE64_ROWS];
    load_bridge64(path, rows);
    printf("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"960\" height=\"620\" viewBox=\"0 0 960 620\">\n");
    printf("<rect width=\"960\" height=\"620\" fill=\"#f8fafc\"/>\n");
    printf("<text x=\"24\" y=\"36\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"22\" font-weight=\"700\">bridge64 dyadic/triadic codebook</text>\n");
    printf("<text x=\"24\" y=\"62\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"13\" fill=\"#475569\">source=%s</text>\n", path);
    for (int i = 0; i < BRIDGE64_ROWS; i++) {
        int row = i / 8;
        int col = i % 8;
        int x = 24 + col * 114;
        int y = 88 + row * 62;
        printf("<rect x=\"%d\" y=\"%d\" width=\"102\" height=\"50\" rx=\"6\" fill=\"#ffffff\" stroke=\"#334155\"/>\n", x, y);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"11\" fill=\"#64748b\">%02d</text>\n", x + 8, y + 15, i);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"14\" font-weight=\"700\" fill=\"#0f172a\">%s</text>\n", x + 8, y + 32, rows[i].dyadic);
        printf("<text x=\"%d\" y=\"%d\" font-family=\"ui-monospace, Menlo, monospace\" font-size=\"14\" fill=\"#0369a1\">%s</text>\n", x + 62, y + 32, rows[i].triadic);
    }
    printf("</svg>\n");
}

static void cmd_codebook(const char *arity_text) {
    int arity = atoi(arity_text);
    int digit_bits;
    unsigned long long base;
    unsigned long long states;
    if (arity <= 0 || arity % 3 != 0) {
        fail("codebook arity must be a positive multiple of 3");
    }
    digit_bits = arity / 3;
    base = pow2_int(digit_bits);
    states = pow2_int(arity);
    printf("codebook arity=%d dyadic=2^%d slots=3 base=%llu states=%llu equality=2^%d=%llu^3\n",
           arity, arity, base, states, arity, base);
    printf("row-format dyadic=<%d binary digits> triadic=<3 base-%llu digits>\n", arity, base);
}

static void usage(void) {
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  cdc_bridge_runtime verify bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime lookup-dyadic bridge64.cdc 101011\n");
    fprintf(stderr, "  cdc_bridge_runtime lookup-triadic bridge64.cdc 223\n");
    fprintf(stderr, "  cdc_bridge_runtime project-trits bridge64.cdc +-0+0- [window]\n");
    fprintf(stderr, "  cdc_bridge_runtime grid bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime grid-svg bridge64.cdc\n");
    fprintf(stderr, "  cdc_bridge_runtime codebook 9\n");
    exit(2);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
    }
    if (strcmp(argv[1], "verify") == 0 && argc == 3) {
        cmd_verify(argv[2]);
    } else if (strcmp(argv[1], "lookup-dyadic") == 0 && argc == 4) {
        cmd_lookup_dyadic(argv[2], argv[3]);
    } else if (strcmp(argv[1], "lookup-triadic") == 0 && argc == 4) {
        cmd_lookup_triadic(argv[2], argv[3]);
    } else if (strcmp(argv[1], "project-trits") == 0 && (argc == 4 || argc == 5)) {
        cmd_project_trits(argv[2], argv[3], argc == 5 ? argv[4] : "trace");
    } else if (strcmp(argv[1], "grid") == 0 && argc == 3) {
        cmd_grid(argv[2]);
    } else if (strcmp(argv[1], "grid-svg") == 0 && argc == 3) {
        cmd_grid_svg(argv[2]);
    } else if (strcmp(argv[1], "codebook") == 0 && argc == 3) {
        cmd_codebook(argv[2]);
    } else {
        usage();
    }
    return 0;
}
