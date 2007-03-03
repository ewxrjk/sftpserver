#ifndef CHARSET_H
#define CHARSET_H

#include <wchar.h>
#include <iconv.h>

wchar_t *convertm2w(const char *s);
/* Convert S to a wide character string */

int iconv_wrapper(struct allocator *a, iconv_t cd, char **sp);
/* Convert SP using CD and allocating memory with A */

#endif /* CHARSET_H */

/*
Local Variables:
c-basic-offset:2
comment-column:40
fill-column:79
indent-tabs-mode:nil
End:
*/
