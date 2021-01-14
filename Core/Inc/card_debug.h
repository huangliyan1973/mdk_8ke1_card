
#ifndef CARD_DEBUG_H
#define CARD_DEBUG_H

#include "lwip/arch.h"
#include "shell.h"

#define NET_DEBUG       1

#define CARD_PLATFORM_ASSERT(x)  do { printf("Assertion \"%s\" failed at line %d in %s\n", \
                                        x, __LINE__, __FILE__); } while(0)

#define CARD_PLATFORM_DIAG(x)  do { printf x; } while(0)

#ifndef CARD_NOASSERT
#define CARD_ASSERT(message, assertion) do { if (!(assertion)) { \
                CARD_PLATFORM_ASSERT(message); }} while(0)
#else /* CARD_NOASSERT */
#define CARD_ASSERT(message, assertion)
#endif
                
#ifndef CARD_ERROR
#ifndef CARD_NOASSERT
#define CARD_PLATFORM_ERROR(message) CARD_PLATFORM_ASSERT(message)
#elif defined CARD_DEBUG
#define CARD_PLATFORM_ERROR(message) CARD_PLATFORM_DIAG((message))
#else
#define CARD_PLATFORM_ERROR(message)
#endif
/* if "expression" isn't true, then print "message" and excute "handler" expression */
#define CARD_ERROR(message, expression, handler) do { if (!(expression)) { \
                    CARD_PLATFORM_ERROR(message); handler; }} while(0)
#endif /* CARD_ERROR */
                    
#ifdef CARD_DEBUG
#define CARD_DEBUGF(debug, message) do { \
                                    CARD_PLATFORM_DIAG(message); \
                                    } while(0)
#else
#define CARD_DEBUGF(debug, message)
#endif

#endif /* CARD_DEBUG_H */
