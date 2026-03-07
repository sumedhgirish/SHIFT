#ifndef __STATUS_H__
#define __STATUS_H__

typedef enum
{
    // Operations
    OPLIST,
    OPWRITE,
    OPREAD,
    OPSEND,
    OPRECEIVE,
    OPINTERROGATE,
    OPREPLY,

    // Exceptions
    PERMISSIONERROR,
    KEYGENERROR,
} StatusCode;

#endif
