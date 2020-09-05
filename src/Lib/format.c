#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "Objects/float.h"
#include "Objects/integer.h"
#include "Objects/object.h"
#include "Objects/string.h"

struct format_spec {
    char                fill;
    char                align;
    char                sign;
    char                alt_format;
    char                zero_pad;
    char *              width;
    char *              precision;
    char                type;
};

#define COALESCE(x, y) (x) ? (x) : (y)

int
LoxObject_ParseFormatSpec(const char *spec, struct format_spec *format) {
    // Default format spec is
    // [[fill]align][sign][#][0][minimumwidth][.precision][type]

    *format = (struct format_spec) { 0 };

    const char *start = spec;
    int x;
    for (x=0; x<2; x++) {
        switch (*(spec + x)) {
        case '<':
        case '>':
        case '=':
        case '^':
            spec += x + 1;
            format->align = *spec;
            break;
        }
    }
    
    if (format->align && start < spec) {
        format->fill = *start;
    }

    switch (*spec) {
    case '+':
    case '-':
    case ' ':
        format->sign = *spec;
        spec++;
        break;
    }

    if (*spec == '#') {
        format->alt_format = *spec;
        spec++;
    }

    if (*spec == '0') {
        format->zero_pad = *spec;
        spec++;
    }

    start = spec;
    if (isdigit(*spec)) {
        while (isdigit(*++spec));
        format->width = strndup(start, spec - start);
    }

    start = spec;
    if (*spec == '.') {
        while (isdigit(*++spec));
        format->precision = strndup(start, spec - start);
    }

    if (*spec == 0)
        return 0;

    switch (*spec) {
    case 'b':
    case 'c':
    case 'd':
    case 'o':
    case 'x':
    case 'X':
    case 'n':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case '%':
    case 's':
        format->type = *spec;
        break;

    default:
        fprintf(stderr, "'%c': Bad string format type\n", *spec);
    }

    return 0;
}

Object*
LoxObject_Format(Object *object, const char *spec) {
    if (object->type->format != NULL) {
        LoxString *spec_string = String_fromCharsAndSize(spec, strlen(spec));
        Object *rv = object->type->format(object, (Object*) spec_string);
        LoxObject_Cleanup((Object*) spec_string);
        return rv;
    }
    
    struct format_spec format;
    if (0 != LoxObject_ParseFormatSpec(spec, &format)) {
        // something went wrong
    }

    // First coerce type of object
    switch (format.type) {
    case 'b':
    case 'c':
    case 'd':
    case 'o':
    case 'x':
    case 'X':
        object = Integer_fromObject(object);
        break;

    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case '%':
        object = Float_fromObject(object);
        break;
        
    case 's':
        object = (Object*) String_fromObject(object);
        break;

    // default: no coercion
    }

    // Now convert to text
    char buffer[64], printformat[32];
    int length;
    if (Integer_isInteger(object)) {
        switch (format.type) {
        case 'b':
            break;
        case 'n':
            format.alt_format = '\'';
        default:
            format.type = 'd';
        case 'o':
        case 'x':
        case 'X':
            if (format.type != 'n')
                format.alt_format = '#';
        case 'c':
        case 'd':
            snprintf(printformat, sizeof(printformat), "%%%.1s%.1s%.1s%s%sll%c",
                format.sign ? &format.sign : "",
                format.alt_format ? &format.alt_format : "",
                format.zero_pad ? &format.zero_pad : "",
                format.width ? format.width : "",
                format.precision ? format.precision : "",
                format.type
            );
            length = snprintf(buffer, sizeof(buffer), printformat, Integer_toInt(object));
            break;
        }
    }
    else if (Float_isFloat(object)) {
        if (format.type == 0) {
            format.type = 'g';
            format.alt_format = '#';
        }

        switch (format.type) {
        case 'n':
            format.type = 'g';
            format.alt_format = '\'';
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            snprintf(printformat, sizeof(printformat), "%%%.1s%.1s%.1s%s%sL%c",
                format.sign ? &format.sign : "",
                format.alt_format ? &format.alt_format : "",
                format.zero_pad ? &format.zero_pad : "",
                format.width ? format.width : "",
                format.precision ? format.precision : (format.type == 'f' ? ".6" : ""),
                format.type
            );
            length = snprintf(buffer, sizeof(buffer), printformat, Float_toLongDouble(object));
            break;

        case '%':
            snprintf(printformat, sizeof(printformat), "%%%.1s#%.1s%s%sLf%%",
                format.sign ? &format.sign : "",
                format.zero_pad ? &format.zero_pad : "",
                format.width ? format.width : "",
                format.precision ? format.precision : ".6"
            );
            length = snprintf(buffer, sizeof(buffer), printformat, Float_toLongDouble(object) * 100.0);
        }
    }
    else {
        switch (format.type) {
        case 's':
        default:
            
            break;
        }
    }

    if (format.align) {
        
    }

    free(format.width);
    free(format.precision);
    
    return (Object*) String_fromCharsAndSize(buffer, length);
}