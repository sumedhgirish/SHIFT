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
    OPLISTEN,

    // Exceptions
    UNKNOWNOP,
    INVALIDBODYSIZE,
    PERMISSIONERROR,
    KEYGENERROR,
} StatusCode;

#endif
