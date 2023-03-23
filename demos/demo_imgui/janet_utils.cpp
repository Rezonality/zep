#include <sstream>
#include <string>

#include <zep/mcommon/logger.h>

#include "janet.h"

namespace Zep
{

JanetTable* env = nullptr;
std::ostringstream strPrintOutput;

static Janet io_print(int32_t argc, Janet* argv)
{
    for (int32_t i = 0; i < argc; ++i)
    {
        int32_t len;
        const uint8_t* vstr;
        if (janet_checktype(argv[i], JANET_BUFFER))
        {
            JanetBuffer* b = janet_unwrap_buffer(argv[i]);
            vstr = b->data;
            len = b->count;
        }
        else
        {
            vstr = janet_to_string(argv[i]);
            len = janet_string_length(vstr);
        }
        if (len)
        {
            strPrintOutput << std::string((char*)vstr, len);
        }
    }

    strPrintOutput << std::endl;
    ZLOG(DBG, strPrintOutput.str());
    return janet_wrap_nil();
}

extern "C" {
void janet_buffer_format(JanetBuffer* b, const char* strfrmt, int32_t argstart, int32_t argc, Janet* argv);
}

static Janet io_printf(int32_t argc, Janet* argv)
{
    janet_arity(argc, 1, -1);
    const char* fmt = janet_getcstring(argv, 0);
    JanetBuffer* buf = janet_buffer(10);
    janet_buffer_format(buf, fmt, 0, argc, argv);

    if (true)
    {
        janet_buffer_push_u8(buf, '\n');
    }

    if (buf->count)
    {
        strPrintOutput << (char*)buf->data;
    }

    // Clear buffer to make things easier for GC
    buf->count = 0;
    buf->capacity = 0;
    free(buf->data);
    buf->data = NULL;
    return janet_wrap_nil();
}

std::string janet_run(const std::string& strText, const std::string& strPathIn, Janet* out)
{
    std::string strPath = strPathIn;
    JanetParser parser;
    int errflags = 0, done = 0;
    int32_t index = 0;
    Janet ret = janet_wrap_nil();
    const uint8_t* where = strPath.c_str() ? janet_cstring(strPath.c_str()) : NULL;

    std::ostringstream strOut;
    std::ostringstream tmp;
    strPrintOutput.swap(tmp);

    if (where)
    {
        janet_gcroot(janet_wrap_string(where));
    }

    if (strPath.empty())
    {
        strPath = "<unknown>";
    }

    janet_parser_init(&parser);

    /* While we haven't seen an error */
    while (!done)
    {

        /* Evaluate parsed values */
        while (janet_parser_has_more(&parser))
        {
            Janet form = janet_parser_produce(&parser);
            JanetCompileResult cres = janet_compile(form, env, where);
            if (cres.status == JANET_COMPILE_OK)
            {
                JanetFunction* f = janet_thunk(cres.funcdef);
                JanetFiber* fiber = janet_fiber(f, 64, 0, NULL);
                fiber->env = env;
                JanetSignal status = janet_continue(fiber, janet_wrap_nil(), &ret);
                if (status != JANET_SIGNAL_OK && status != JANET_SIGNAL_EVENT)
                {
                    janet_stacktrace(fiber, ret);
                    errflags |= 0x01;
                    done = 1;
                }
            }
            else
            {
                if (cres.macrofiber)
                {
                    janet_stacktrace(cres.macrofiber, janet_wrap_string(cres.error));
                }
                strOut << std::string((const char*)cres.error) << std::endl;
                errflags |= 0x02;
                done = 1;
            }
        }

        /* Dispatch based on parse state */
        switch (janet_parser_status(&parser))
        {
        case JANET_PARSE_DEAD:
            done = 1;
            break;
        case JANET_PARSE_ERROR:
            errflags |= 0x04;
            strOut << "Parse Error " << std::string(janet_parser_error(&parser)) << std::endl;
            done = 1;
            break;
        case JANET_PARSE_PENDING:
            if (index == strText.size())
            {
                janet_parser_eof(&parser);
            }
            else
            {
                janet_parser_consume(&parser, strText[index++]);
            }
            break;
        case JANET_PARSE_ROOT:
            if (index >= strText.size())
            {
                janet_parser_eof(&parser);
            }
            else
            {
                janet_parser_consume(&parser, strText[index++]);
            }
            break;
        }
    }

    /* Clean up and return errors */
    janet_parser_deinit(&parser);
    if (where)
        janet_gcunroot(janet_wrap_string(where));

    if (out)
        *out = ret;

    if (janet_type(ret) != JANET_NIL)
    {
        strOut << (char*)janet_to_string(ret) << std::endl;
        strOut << strPrintOutput.str();
    }
    else
    {
        if (strOut.str().empty() && strPrintOutput.str().empty())
        {
            strOut << "nil";
        }
        else
        {
            strOut << strPrintOutput.str();
        }
    }
    return strOut.str();
}

JanetTable* janet_get()
{
    if (env == nullptr)
    {
        janet_init();

        env = janet_table(0);
        janet_table_put(env, janet_csymbolv("print"), janet_wrap_cfunction((void*)io_print));
        janet_table_put(env, janet_csymbolv("printf"), janet_wrap_cfunction((void*)io_printf));
        janet_table_put(env, janet_csymbolv("eprintf"), janet_wrap_cfunction((void*)io_printf));
        env = janet_core_env(env);

        /* How to add APIs
        janet_cfuns(env, "synth", graph_cfuns);
        janet_register_abstract_type(&graph_type);
        janet_register_abstract_type(&node_type);
        janet_register_abstract_type(&patterngroup_type);
        */
    }

    return env;
}

void janet_init(const std::string& basePath)
{
    janet_get();
}

} // namespace Zep
