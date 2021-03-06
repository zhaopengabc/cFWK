#include "cf_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#define HUGE_STRING_LEN	8192
#define STRING_MAX 256
#define STRING_INC		256

#define MAX_TEST_CASES		256

static void* _malloc(int len){
    return malloc(len);
}
static void _free(void* m){
    free(m);
}
static void* _realloc(void *mem,int size){
    return realloc(mem,size);
}

typedef struct _string{    
    int len;
    int size;
    char* buff;
}_string;
static _string* _string_create(){
    _string* s = _malloc(sizeof(sizeof(_string)));
    s->size = STRING_MAX;
    s->len = 0;
    s->buff = _malloc(STRING_MAX);
    s->buff[0] = 0;
    return s;
}
static void _string_destroy(_string* s){
    if( s == NULL) return;
    memset(s->buff,0,s->size);
    _free(s->buff);
    memset(s,0,sizeof(_string));
    _free(s);
}
static void _string_resize(_string* s, int size){
    s->buff = (char*) _realloc(s->buff,  size);
	s->size = size;
}
static void _string_clear(_string* s)
{
	s->len = 0;
    memset(s->buff,0,s->size);
}
static void _string_append(_string* s, const char* text)
{
	int length;

	if (text == NULL) {
		text = "NULL";
	}

	length = strlen(text);
	if (s->len + length + 1 >= s->size)
		_string_resize(s, s->len + length + 1 + STRING_INC);
	s->len += length;
	strcat(s->buff, text);
}

static void _string_append_format(_string* s,const char* format, ...){
    va_list argp;
	char buf[HUGE_STRING_LEN];
	va_start(argp, format);
	vsprintf(buf, format, argp);
	va_end(argp);
	_string_append(s, buf);
}
static const char* _string_cstring(_string* s){
    return s->buff;
}

typedef enum cf_test_type{
    CF_TEST_TYPE_TEST = 0,
    CF_TEST_TYPE_SUITE
}cf_test_type_e;

typedef struct cf_test{
    cf_test_type_e type;
    const char* name;
    _string* msg;
    void(*func)(cf_test*);
    bool failed;
    jmp_buf *jumpBuf;
}cf_test;

cf_test* cf_test_create(const char* name,void(*test_func)(cf_test*)){
    cf_test* tc = _malloc(sizeof(cf_test));
    memset(tc,0,sizeof(cf_test));
    tc->type = CF_TEST_TYPE_TEST;
    tc->name = name;
    tc->func = test_func;
    return tc;
}
void cf_test_destroy(cf_test* tc){
    if(tc) return;
    if(tc->msg)
        _string_destroy(tc->msg);
    memset(tc,0,sizeof(cf_test));
    _free(tc);
}
const char* cf_test_msg(cf_test* tc){
    if(tc->msg)
        return _string_cstring(tc->msg);
    else
        return NULL;
}
const char* cf_test_name(cf_test* tc){
    return tc->name;
}
void cf_test_run(cf_test* tc){
    jmp_buf buf;
	tc->jumpBuf = &buf;
	if (setjmp(buf) == 0)
	{
		(tc->func)(tc);
	}
	tc->jumpBuf = 0;
}

typedef struct cf_suite {
    cf_test_type_e type;
    int list_size;
    int list_len;
    int fail_count;
    int count;
    cf_test** list;
    _string* summary;
    _string* msg_lines;
    _string* msg;
}cf_suite;

cf_suite* cf_suite_create(void){
    cf_suite* suite = _malloc(sizeof(cf_suite));
    memset(suite,0,sizeof(cf_suite));
    suite->type = CF_TEST_TYPE_SUITE;
    suite->list = _malloc(sizeof(cf_test*)*MAX_TEST_CASES);
    suite->list_size = MAX_TEST_CASES;
    suite->summary = _string_create();
    suite->msg_lines = _string_create();
    suite->msg = _string_create();
    return suite;
}

void cf_suite_destroy(cf_suite* suite){
    if(suite == NULL) return;
    if(suite->type == CF_TEST_TYPE_TEST)
        cf_test_destroy((cf_test*)suite);
    else{
        for(int i = 0;i < suite->list_len;i++)
        {
            if(suite->list[i]->type == CF_TEST_TYPE_TEST){
                cf_test_destroy(suite->list[i]);
            }
            else
            {
                cf_suite_destroy((cf_suite*)suite->list[i]);
            }
        }
        _free(suite->list);
        _free(suite->msg);
        _free(suite->msg_lines);
        _free(suite->summary);
        _free(suite);
    }
}

void cf_suite_add_test(cf_suite* suite,cf_test* tc){
    if(suite->list_len == suite->list_size)
    {
        cf_test* new_list = _malloc(sizeof(cf_test*)*(suite->list_len+MAX_TEST_CASES));
        memcpy(new_list,suite->list,suite->list_len);
        _free(suite->list);
        suite->list = (cf_test**)new_list;
        suite->list_size = suite->list_len+MAX_TEST_CASES;
    }
    suite->list[suite->list_len++] = tc;
    suite->count++;
}
void cf_suite_add_suite(cf_suite* suite,cf_suite* suite_src){
    if(suite->list_len == suite->list_size)
    {
        cf_test* new_list = _malloc(sizeof(cf_test*)*(suite->list_len+MAX_TEST_CASES));
        memcpy(new_list,suite->list,suite->list_len);
        _free(suite->list);
        suite->list = (cf_test**)new_list;
        suite->list_size = suite->list_len+MAX_TEST_CASES;
    }
    suite->list[suite->list_len++] = (cf_test*)suite_src;
    suite->count += suite_src->count;
}
const char* cf_suite_msg(cf_suite* suite){
    if(suite->msg)
        return _string_cstring(suite->msg);
    else
        return NULL;
}

static void _cf_suite_statistical(cf_suite* suite){
    int fail_count = 0;
    _string_clear(suite->msg);
    _string_clear(suite->msg_lines);
    _string_clear(suite->summary);
    for(int i = 0 ; i < suite->list_len ; ++i){
        if(suite->list[i]->type == CF_TEST_TYPE_TEST)
            _string_append(suite->summary, suite->list[i]->failed ? "F":".");
        else{
            cf_suite* sub_suite = (cf_suite*)suite->list[i];
            _string_append(suite->summary, _string_cstring(sub_suite->summary) );
        }
    }
        
    _string_append(suite->msg, _string_cstring(suite->summary));    
    _string_append(suite->msg, "\n");
    if (suite->fail_count == 0)
	{
		int pass_count = suite->count - suite->fail_count;
		const char* test_word = pass_count == 1 ? "test" : "tests";
		_string_append_format(suite->msg, "OK (%d %s)\n", pass_count, test_word);
	}
    else
    {
        if (suite->fail_count == 1)
			_string_append(suite->msg, "There was 1 failure:\n");
		else
			_string_append_format(suite->msg, "There were %d failures:\n", suite->fail_count);

		for (int i = 0 ; i < suite->list_len ; ++i)
		{
            if(suite->list[i]->type == CF_TEST_TYPE_TEST){
                cf_test* tc = suite->list[i];
                if (tc->failed)
                {
                    fail_count++;
                    _string_append_format(suite->msg_lines, "%d) %s: %s\n",
                        fail_count, cf_test_name(tc), cf_test_msg(tc));
                }

            }
            else
            {
                cf_suite* ts = (cf_suite*)suite->list[i];
                if (ts->fail_count > 0)
                {
                    fail_count += ts->fail_count;
                    _string_append(suite->msg_lines, _string_cstring(ts->msg_lines));
                }
            }
		}
        _string_append(suite->msg, _string_cstring(suite->msg_lines));    
		_string_append(suite->msg, "\n!!!FAILURES!!!\n");

		_string_append_format(suite->msg, "Runs: %d ",   suite->count);
		_string_append_format(suite->msg, "Passes: %d ", suite->count - suite->fail_count);
		_string_append_format(suite->msg, "Fails: %d\n",  suite->fail_count);
    }
    
    
}

void cf_suite_run(cf_suite* suite){
    for(int i = 0;i < suite->list_len;i++)
    {
        if(suite->list[i]->type == CF_TEST_TYPE_TEST){
            cf_test_run(suite->list[i]);
            if(suite->list[i]->failed){
                suite->fail_count++;
            }
        }
        else
        {
            cf_suite* ts = (cf_suite*)suite->list[i];
            cf_suite_run(ts);
            suite->fail_count += ts->fail_count;
        }
    }
    _cf_suite_statistical(suite);
}

void cf_assert_common(cf_test* tc,const char* file,int line,const char* msg,bool cond){
    if(cond)    return;
    tc->failed = true;
    if(tc->msg){
        _string_destroy(tc->msg);
    }
    tc->msg = _string_create();
    _string_append_format(tc->msg,"%s:%d %s",file,line,msg);
    if (tc->jumpBuf != 0) longjmp(*(tc->jumpBuf), 0);
}

void cf_assert_str_equals(cf_test* tc, const char* file, int line, const char* message, 
	const char* expected, const char* actual)
{
	if ((expected == NULL && actual == NULL) ||
	    (expected != NULL && actual != NULL &&
	     strcmp(expected, actual) == 0))
	{
		return;
	}
    tc->failed = true;
    tc->msg = _string_create();
    _string_append_format(tc->msg,"%s:%d ",file,line);
	if (message != NULL) 
	{
        _string_append(tc->msg,message);
		_string_append(tc->msg,": ");
	}
    _string_append_format(tc->msg,"expected <%s> but was <%s>",expected,actual);
    if (tc->jumpBuf != 0) longjmp(*(tc->jumpBuf), 0);
}