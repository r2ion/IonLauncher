/* see copyright notice in squirrel.h */
#include "sqstdaux.h"
#include "../include/squirrel.h"
#include "../squirrel.h"
#include "../squirrel/sqstring.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void(__fastcall* SQLogEntityFunction)(const SQObject* object, SQChar* output, size_t outputSize);

void sqstd_printerror(HSQUIRRELVM v)
{
    const ScriptContext context = static_cast<ScriptContext>(v->sharedState->cSquirrelVM->vmContext);
    SquirrelManager* squirrel = g_pSquirrel[context];
    if (!squirrel)
        return;

    auto logger = squirrel->m_logger;
    if (!logger)
        return;

    const SQObject& error = v->_stackOfCurrentFunction[1];
    switch (sq_type(error))
    {
    case OT_NULL:
        logger->error(_SC("SCRIPT ERROR: [{}] NULL"), CSquirrelContext::GetName(context));
        break;
    case OT_INTEGER:
        logger->error(_SC("SCRIPT ERROR: [{}] {}"), CSquirrelContext::GetName(context), _integer(error));
        break;
    case OT_FLOAT:
        logger->error(_SC("SCRIPT ERROR: [{}] {:.14g}"), CSquirrelContext::GetName(context), _float(error));
        break;
    case OT_STRING:
        logger->error(_SC("SCRIPT ERROR: [{}] {}"), CSquirrelContext::GetName(context), _stringval(error));
        break;
    case OT_BOOL:
        logger->error(_SC("SCRIPT ERROR: [{}] {}"), CSquirrelContext::GetName(context),
                      _bool(error) == SQTrue ? _SC("true") : _SC("false"));
        break;
    default:
        logger->error(_SC("SCRIPT ERROR: [{}] (unknown error)"), CSquirrelContext::GetName(context));
        break;
    }

    sqstd_printcallstack(v);
}

void sqstd_printcallstack(HSQUIRRELVM v)
{
    SquirrelManager* squirrel = g_pSquirrel[static_cast<ScriptContext>(v->sharedState->cSquirrelVM->vmContext)];
    if (!squirrel || !squirrel->__sq_stackinfos || !squirrel->__sq_getlocal)
        return;

    auto logger = squirrel->m_logger;
    if (logger)
    {
        SQStackInfos si;
        SQInteger i;
        SQFloat f;
        const SQChar* s;
        SQInteger level = 1; // 1 is to skip this function that is level 0
        const SQChar* name = 0;
        SQInteger seq = 0;
        logger->error(_SC("CALLSTACK"));
        while (SQ_SUCCEEDED(squirrel->sq_stackinfos(v, level, si)))
        {
            const SQChar* fn = _SC("unknown");
            const SQChar* src = _SC("unknown");
            if (si.funcname)
                fn = si.funcname;
            if (si.source)
                src = si.source;

            // Respawn omits the Assert helper from assertion callstacks.
            if (level == 1 && !strcmp(fn, _SC("Assert")))
            {
                level++;
                continue;
            }

            if (const Mod* mod = squirrel->getmodfromsource(src))
                logger->error(_SC("*FUNCTION [{}()] {} line [{}] [{}]"), fn, src, si.line, mod->Name);
            else
                logger->error(_SC("*FUNCTION [{}()] {} line [{}]"), fn, src, si.line);
            level++;
        }
        level = 0;
        logger->error(_SC("LOCALS"));

        for (level = 0; level < 10; level++)
        {
            seq = 0;
            while ((name = squirrel->getlocal(v, level, seq)))
            {
                seq++;
                const SQObject& object = v->_stack[v->_top - 1];
                switch (sq_type(object))
                {
                case OT_NULL:
                    logger->error(_SC("[{}] NULL"), name);
                    break;
                case OT_INTEGER:
                    i = _integer(object);
                    logger->error(_SC("[{}] {}"), name, i);
                    break;
                case OT_FLOAT:
                    f = _float(object);
                    logger->error(_SC("[{}] {:.14g}"), name, f);
                    break;
                case OT_USERPOINTER:
                    logger->error(_SC("[{}] USERPOINTER"), name);
                    break;
                case OT_STRING:
                    s = _stringval(object);
                    logger->error(_SC("[{}] \"{}\""), name, s);
                    break;
                case OT_ASSET:
                    s = _stringval(object);
                    logger->error(_SC("[{}] $\"{}\""), name, s);
                    break;
                case OT_TABLE:
                    logger->error(_SC("[{}] TABLE"), name);
                    break;
                case OT_ARRAY:
                    logger->error(_SC("[{}] ARRAY"), name);
                    break;
                case OT_CLOSURE:
                    logger->error(_SC("[{}] CLOSURE"), name);
                    break;
                case OT_NATIVECLOSURE:
                    logger->error(_SC("[{}] NATIVECLOSURE"), name);
                    break;
                case OT_USERDATA:
                    logger->error(_SC("[{}] USERDATA"), name);
                    break;
                case OT_THREAD:
                    logger->error(_SC("[{}] THREAD"), name);
                    break;
                case OT_CLASS:
                    logger->error(_SC("[{}] CLASS"), name);
                    break;
                case OT_INSTANCE:
                    logger->error(_SC("[{}] INSTANCE"), name);
                    break;
                case OT_ENTITY:
                {
                    SQChar description[256] = {};
                    if (v->sharedState->logEntityFunction)
                    {
                        reinterpret_cast<SQLogEntityFunction>(v->sharedState->logEntityFunction)(&object, description, sizeof(description));
                    }
                    logger->error(_SC("[{}] ENTITY ({})"), name, description);
                    break;
                }
                case OT_WEAKREF:
                    logger->error(_SC("[{}] WEAKREF"), name);
                    break;
                case OT_BOOL:
                    logger->error(_SC("[{}] {}"), name, _bool(object) == SQTrue ? _SC("true") : _SC("false"));
                    break;
                case OT_VECTOR:
                {
                    const SQFloat* vector = _vector(object);
                    logger->error(_SC("[{}] <{:g}, {:g}, {:g}>"), name, vector[0], vector[1], vector[2]);
                    break;
                }
                case OT_STRUCT:
                    logger->error(_SC("[{}] unknown struct"), name);
                    break;
                default:
                    break;
                }
                squirrel->pop(v, 1);
            }
        }
    }
}
